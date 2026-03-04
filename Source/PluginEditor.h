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
#include "SpectrogramComponent.h"
#include "StereoImageComponent.h"
#include "Band3Component.h"
#include "PsrMeterComponent.h"
#include "HoloNonoComponent.h"

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

    //==========================================================================
    // Mouse handlers for jiggle drag-drop reorder
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    GOODMETERAudioProcessor& audioProcessor;

    // Custom LookAndFeel
    GoodMeterLookAndFeel customLookAndFeel;

    // Meter components (raw pointers - owned by MeterCardComponents)
    LevelsMeterComponent* levelsMeter = nullptr;
    PhaseCorrelationComponent* phaseMeter = nullptr;
    VUMeterComponent* vuMeter = nullptr;
    SpectrumAnalyzerComponent* spectrumAnalyzer = nullptr;
    SpectrogramComponent* spectrogramMeter = nullptr;
    StereoImageComponent* stereoImageMeter = nullptr;
    Band3Component* band3Meter = nullptr;
    PsrMeterComponent* psrMeter = nullptr;
    HoloNonoComponent* holoNono = nullptr;

    // Meter card components
    std::unique_ptr<MeterCardComponent> levelsCard;
    std::unique_ptr<MeterCardComponent> vuMeterCard;
    std::unique_ptr<MeterCardComponent> threeBandCard;
    std::unique_ptr<MeterCardComponent> spectrumCard;
    std::unique_ptr<MeterCardComponent> phaseCard;
    std::unique_ptr<MeterCardComponent> stereoImageCard;
    std::unique_ptr<MeterCardComponent> spectrogramCard;
    std::unique_ptr<MeterCardComponent> psrCard;
    std::unique_ptr<MeterCardComponent> nonoCard;

    // Viewport for scrolling (if needed)
    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;

    //==========================================================================
    // Mini Mode (compact layout with aggressive space squeezing)
    //==========================================================================
    bool isMiniMode = true;
    void toggleMiniMode();

    //==========================================================================
    // Jiggle / Edit mode + 1v1 Swap Drag Engine
    //==========================================================================
    bool isJiggleMode = false;
    std::vector<int> panelOrder = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
    juce::ComponentAnimator animator;
    float jigglePhases[9] = {};

    // Drag state
    int draggedPanelSlot = -1;          // slot index in panelOrder being dragged (-1 = none)
    juce::Point<int> dragOffset;        // mouse offset from card origin
    bool dragActivated = false;         // true only after real drag motion detected
    juce::Rectangle<int> dragOriginSlotBounds;  // dragged card's slot bounds BEFORE pickup
    std::vector<juce::Rectangle<int>> slotTargetBounds;  // computed slot positions

    // Hover-to-swap state
    int currentHoverTarget = -1;        // panelOrder slot being hovered over
    juce::uint32 hoverStartTime = 0;    // when hovering began
    static constexpr juce::uint32 swapHoverMs = 1000;  // 1 second hover to swap
    juce::uint32 jiggleEnteredTime = 0;  // timestamp when jiggle mode was entered
    bool readyToSwap = false;           // true when hover confirmed, swap on mouseUp

    void enterJiggleMode();
    void exitJiggleMode();
    MeterCardComponent* getCardByIndex(int idx);
    int findPanelSlotAt(juce::Point<int> posInContent);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GOODMETERAudioProcessorEditor)
};
