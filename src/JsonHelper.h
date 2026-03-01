#pragma once
#include <juce_core/juce_core.h>

/**
 * JSON helper utilities for building structured JSON responses.
 * All host output is serialized through these helpers for consistency.
 */
namespace JsonHelper
{

/** Create a JSON object from key-value pairs. */
inline juce::var makeObject()
{
    return juce::var (new juce::DynamicObject());
}

/** Set a property on a var that wraps a DynamicObject. */
inline void set (juce::var& obj, const juce::String& key, const juce::var& value)
{
    if (auto* dyn = obj.getDynamicObject())
        dyn->setProperty (key, value);
}

/** Create a success result. */
inline juce::var makeSuccess (const juce::String& action, const juce::var& data = {})
{
    auto result = makeObject();
    set (result, "action", action);
    set (result, "success", true);
    if (! data.isVoid())
        set (result, "data", data);
    return result;
}

/** Create an error result. */
inline juce::var makeError (const juce::String& action, const juce::String& message)
{
    auto result = makeObject();
    set (result, "action", action);
    set (result, "success", false);
    set (result, "error", message);
    return result;
}

/** Create a JSON array. */
inline juce::var makeArray()
{
    return juce::var (juce::Array<juce::var>());
}

/** Append a value to a JSON array var. */
inline void append (juce::var& arr, const juce::var& value)
{
    if (auto* a = arr.getArray())
        a->add (value);
}

/** Serialize a var to a compact JSON string. */
inline juce::String toJsonString (const juce::var& v, bool compact = true)
{
    return juce::JSON::toString (v, compact);
}

/** Parse a JSON string into a var. Returns {} on failure. */
inline juce::var parseJson (const juce::String& jsonText, juce::String& errorOut)
{
    auto result = juce::JSON::parse (jsonText);
    if (result == juce::var())
        errorOut = "Failed to parse JSON input";
    return result;
}

} // namespace JsonHelper
