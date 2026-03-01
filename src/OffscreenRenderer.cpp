#include "OffscreenRenderer.h"
#include "JsonHelper.h"

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

    // Set the size (triggers resized())
    editor->setSize (width, height);

    // Let any async operations complete
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

    // Offscreen render to Image
    juce::Image image (juce::Image::ARGB, width, height, true);
    {
        juce::Graphics g (image);
        g.fillAll (juce::Colours::black);
        editor->paintEntireComponent (g, true);
    }

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
    set (data, "width",        width);
    set (data, "height",       height);
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

    editor->setSize (width, height);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

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
