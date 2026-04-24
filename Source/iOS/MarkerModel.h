#pragma once

#include <JuceHeader.h>

struct GoodMeterMarkerItem
{
    juce::String id;
    double seconds = 0.0;
    juce::Colour colour = juce::Colours::white;
    juce::String note;
    juce::StringArray tags;
    juce::String frameImagePath;
    bool framePending = false;
    bool isVideo = false;
};
