/*
  ==============================================================================
    IOSPhotoVideoPicker.h
    GOODMETER iOS - Photos video import bridge
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace GoodMeterIOSPhotoVideoPicker
{
    using Completion = std::function<void(const juce::URL&)>;

    bool isAvailable();
    void open(juce::Component* parent, Completion completion);
}
