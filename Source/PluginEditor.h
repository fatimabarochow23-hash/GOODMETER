/*
  ==============================================================================
    PluginEditor.h
    GOODMETER - Main plugin editor

    60Hz Timer-driven UI with vertical meter layout
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "GoodMeterLookAndFeel.h"
#include "MeterCardComponent.h"
#include "LevelsMeterComponent.h"
#include "PhaseCorrelationComponent.h"
#include "VUMeterComponent.h"
#include "SpectrumAnalyzerComponent.h"

//==============================================================================
/**
 * Main plugin editor with 60Hz update timer
 */
class GOODMETERAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer
{
public:
    GOODMETERAudioProcessorEditor(GOODMETERAudioProcessor&);
    ~GOODMETERAudioProcessorEditor() override;

    //==========================================================================
    void paint(juce::Graphics&) override;
    void resized() override;

    //==========================================================================
    void timerCallback() override;

private:
    GOODMETERAudioProcessor& audioProcessor;

    // Custom LookAndFeel
    GoodMeterLookAndFeel customLookAndFeel;

    // Meter components (raw pointers - owned by MeterCardComponents)
    LevelsMeterComponent* levelsMeter = nullptr;
    PhaseCorrelationComponent* phaseMeter = nullptr;
    VUMeterComponent* vuMeter = nullptr;
    SpectrumAnalyzerComponent* spectrumAnalyzer = nullptr;

    // Meter card components (Phase 3 will add remaining meters)
    std::unique_ptr<MeterCardComponent> levelsCard;
    std::unique_ptr<MeterCardComponent> vuMeterCard;
    std::unique_ptr<MeterCardComponent> threeBandCard;
    std::unique_ptr<MeterCardComponent> spectrumCard;
    std::unique_ptr<MeterCardComponent> phaseCard;
    std::unique_ptr<MeterCardComponent> stereoImageCard;
    std::unique_ptr<MeterCardComponent> spectrogramCard;

    // Viewport for scrolling (if needed)
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GOODMETERAudioProcessorEditor)
};
