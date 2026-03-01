#include "OffscreenRenderer.h"
#include "JsonHelper.h"

#if JUCE_LINUX
 #include <X11/Xlib.h>
 #include <X11/Xutil.h>
 #include <unistd.h>    // usleep

// ============================================================================
//  X11 pixel-capture helper.
//  VST3 editors on Linux use XEmbed — the actual plugin UI lives in a child
//  X window that JUCE's software renderer cannot see.  The only reliable way
//  to grab the rendered pixels is through XGetImage on the root window.
// ============================================================================

/** Capture a region of the root window and return it as a JUCE Image. */
static juce::Image captureX11Window (void* nativeHandle, int w, int h)
{
    Display* dpy = XOpenDisplay (nullptr);
    if (dpy == nullptr)
        return {};

    Window xwin = (Window) nativeHandle;

    // Make sure the window is mapped and sized correctly
    XMapRaised (dpy, xwin);
    XMoveResizeWindow (dpy, xwin, 0, 0,
                       static_cast<unsigned int> (w),
                       static_cast<unsigned int> (h));
    XSync (dpy, False);

    // Give X server time to composite all child windows (XEmbed).
    // usleep is more reliable than JUCE message-loop here because
    // the rendering happens in the X server, not in our event loop.
    usleep (400000);  // 400 ms
    XSync (dpy, False);

    // Translate to root coordinates
    Window root = DefaultRootWindow (dpy);
    int rx = 0, ry = 0;
    Window childRet;
    XTranslateCoordinates (dpy, xwin, root, 0, 0, &rx, &ry, &childRet);

    // Grab from root window — this composites all overlapping child windows
    XImage* ximg = XGetImage (dpy, root, rx, ry,
                              static_cast<unsigned int> (w),
                              static_cast<unsigned int> (h),
                              AllPlanes, ZPixmap);
    if (ximg == nullptr)
    {
        XCloseDisplay (dpy);
        return {};
    }

    // Convert to JUCE Image
    juce::Image image (juce::Image::ARGB, w, h, false);
    {
        juce::Image::BitmapData bm (image, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                unsigned long px = XGetPixel (ximg, x, y);
                auto r = static_cast<uint8_t> ((px >> 16) & 0xFF);
                auto g = static_cast<uint8_t> ((px >>  8) & 0xFF);
                auto b = static_cast<uint8_t> ((px      ) & 0xFF);
                bm.setPixelColour (x, y, juce::Colour (r, g, b));
            }
        }
    }

    XDestroyImage (ximg);
    XCloseDisplay (dpy);
    return image;
}
#endif // JUCE_LINUX

// ============================================================================
// Helper: Ensure a component has a native window peer.
// Under xvfb this creates a real (but invisible) X11 window.
// Returns true if *this call* added the peer (caller must remove it later).
static bool ensureOnDesktop (juce::Component* comp)
{
    if (comp == nullptr) return false;
    if (comp->isOnDesktop())  return false;

    comp->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    comp->setVisible (true);
    return true;
}

// ============================================================================
juce::var OffscreenRenderer::captureSnapshot (juce::AudioPluginInstance* plugin,
                                              const juce::String& outputPath,
                                              int width, int height,
                                              juce::AudioProcessorEditor* existingEditor)
{
    using namespace JsonHelper;

    if (plugin == nullptr)
        return makeError ("capture_ui", "No plugin loaded");

    // Use existing editor or create a temporary one
    std::unique_ptr<juce::AudioProcessorEditor> tempEditor;
    juce::AudioProcessorEditor* editor = existingEditor;
    if (editor == nullptr)
    {
        if (! plugin->hasEditor())
            return makeError ("capture_ui", "Plugin does not have a GUI editor");
        tempEditor.reset (plugin->createEditor());
        editor = tempEditor.get();
    }
    if (editor == nullptr)
        return makeError ("capture_ui", "Failed to create plugin editor");

    // Ensure the editor has a native window peer.
    bool weAddedPeer = ensureOnDesktop (editor);

    // Resize and let the component hierarchy lay out.
    editor->setBounds (0, 0, width, height);
    editor->repaint();
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    juce::Image image;

   #if JUCE_LINUX
    // On Linux, VST3 editors use XEmbed — the actual UI lives in a child
    // X11 window.  JUCE's createComponentSnapshot() only captures the
    // wrapper's gray background, not the plugin's actual content.
    // We must go through the X server to grab the composited pixels.
    if (auto* peer = editor->getPeer())
    {
        image = captureX11Window (peer->getNativeHandle(), width, height);
    }
   #endif

    // Fallback: use JUCE's software snapshot (works on macOS / Windows
    // where editors don't use XEmbed).
    if (image.isNull())
        image = editor->createComponentSnapshot (
            editor->getLocalBounds(), true, 1.0f);

    // Tear down the peer if we created it
    if (weAddedPeer)
        editor->removeFromDesktop();

    if (image.isNull())
        return makeError ("capture_ui", "Failed to capture plugin UI image");

    // Save as PNG
    juce::File outFile (outputPath);
    outFile.getParentDirectory().createDirectory();
    juce::FileOutputStream stream (outFile);

    if (stream.failedToOpen())
        return makeError ("capture_ui",
                          "Cannot open output file for writing: " + outputPath);

    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
        return makeError ("capture_ui", "Failed to write PNG data");

    stream.flush();

    auto data = makeObject();
    set (data, "output_path",  outFile.getFullPathName());
    set (data, "width",        image.getWidth());
    set (data, "height",       image.getHeight());
    set (data, "file_size_bytes", static_cast<int> (outFile.getSize()));
    return makeSuccess ("capture_ui", data);
}

// ============================================================================
juce::var OffscreenRenderer::getParameterLayout (juce::AudioPluginInstance* plugin,
                                                  int width, int height,
                                                  juce::AudioProcessorEditor* existingEditor)
{
    using namespace JsonHelper;

    if (plugin == nullptr)
        return makeError ("get_parameter_layout", "No plugin loaded");

    // Use existing editor or create a temporary one
    std::unique_ptr<juce::AudioProcessorEditor> tempEditor;
    juce::AudioProcessorEditor* editor = existingEditor;
    if (editor == nullptr)
    {
        if (! plugin->hasEditor())
            return makeError ("get_parameter_layout", "Plugin does not have a GUI editor");
        tempEditor.reset (plugin->createEditor());
        editor = tempEditor.get();
    }
    if (editor == nullptr)
        return makeError ("get_parameter_layout", "Failed to create plugin editor");

    // Ensure native peer exists for correct layout measurement
    bool weAddedPeer = ensureOnDesktop (editor);

    editor->setBounds (0, 0, width, height);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (300);

    auto data = makeObject();
    set (data, "editor_width",  editor->getWidth());
    set (data, "editor_height", editor->getHeight());

    // Walk the component tree
    set (data, "component_tree", buildComponentTree (editor));

    // Also list all parameters with their current values
    auto paramsArr = makeArray();
    auto& params = plugin->getParameters();
    for (int i = 0; i < params.size(); ++i)
    {
        auto* param = params[i];
        auto pObj = makeObject();
        set (pObj, "index",            i);
        set (pObj, "name",             param->getName (128));
        set (pObj, "value_normalized", param->getValue());
        set (pObj, "value_text",       param->getCurrentValueAsText());
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
            set (pObj, "parameter_id", withID->paramID);
        append (paramsArr, pObj);
    }
    set (data, "parameters", paramsArr);

    if (weAddedPeer)
        editor->removeFromDesktop();

    return makeSuccess ("get_parameter_layout", data);
}

// ============================================================================
juce::var OffscreenRenderer::buildComponentTree (juce::Component* comp, int depth)
{
    using namespace JsonHelper;

    if (comp == nullptr)
        return {};

    auto node = makeObject();
    set (node, "name",       comp->getName().isEmpty() ? juce::String ("unnamed") : comp->getName());
    set (node, "type",       typeid (*comp).name());
    set (node, "depth",      depth);
    set (node, "visible",    comp->isVisible());

    // Bounds
    auto bounds = comp->getBounds();
    auto boundsObj = makeObject();
    set (boundsObj, "x",      bounds.getX());
    set (boundsObj, "y",      bounds.getY());
    set (boundsObj, "width",  bounds.getWidth());
    set (boundsObj, "height", bounds.getHeight());
    set (node, "bounds", boundsObj);

    // Try to identify component type
    if (dynamic_cast<juce::Slider*> (comp))
        set (node, "widget_type", "slider");
    else if (dynamic_cast<juce::TextButton*> (comp))
        set (node, "widget_type", "button");
    else if (dynamic_cast<juce::ToggleButton*> (comp))
        set (node, "widget_type", "toggle_button");
    else if (dynamic_cast<juce::ComboBox*> (comp))
        set (node, "widget_type", "combo_box");
    else if (dynamic_cast<juce::Label*> (comp))
    {
        set (node, "widget_type", "label");
        auto* label = dynamic_cast<juce::Label*> (comp);
        set (node, "text", label->getText());
    }

    // Children
    if (comp->getNumChildComponents() > 0)
    {
        auto childArr = makeArray();
        for (int i = 0; i < comp->getNumChildComponents(); ++i)
            append (childArr, buildComponentTree (comp->getChildComponent (i), depth + 1));
        set (node, "children", childArr);
    }

    return node;
}
