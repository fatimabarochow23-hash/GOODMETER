/*
  ==============================================================================
    MetersPageComponent.h
    GOODMETER iOS - Page 2: Scrollable grid of 8 meter cards

    Responsive grid layout:
    - iPhone portrait: 1 column
    - iPhone landscape / iPad portrait: 2 columns
    - iPad landscape: 3 columns

    Pinch-to-zoom: discrete column count switching via magnifyGestureChanged()

    Timer bridge (30Hz): reads processor atomics, updates setter-based meters
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"
#include "../MeterCardComponent.h"
#include "../LevelsMeterComponent.h"
#include "../VUMeterComponent.h"
#include "../Band3Component.h"
#include "../SpectrumAnalyzerComponent.h"
#include "../PhaseCorrelationComponent.h"
#include "../StereoImageComponent.h"
#include "../SpectrogramComponent.h"
#include "../PsrMeterComponent.h"
#include "iOSAudioEngine.h"

class MetersPageComponent : public juce::Component,
                             public juce::Timer
{
public:
    MetersPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
        // Create viewport for scrolling
        viewport = std::make_unique<juce::Viewport>();
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(viewport.get());
        viewport->setViewedComponent(contentComponent.get(), false);
        viewport->setScrollBarsShown(true, false);

        // File name header label
        headerLabel.setJustificationType(juce::Justification::centredRight);
        headerLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(headerLabel);

        // Title label
        titleLabel.setText("METERS", juce::dontSendNotification);
        titleLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        addAndMakeVisible(titleLabel);

        //==================================================================
        // Create all 8 meter cards
        //==================================================================
        levelsCard = std::make_unique<MeterCardComponent>("LEVELS", GoodMeterLookAndFeel::accentPink, true);
        levelsMeter = new LevelsMeterComponent(processor);
        levelsMeter->setupTargetMenu();
        levelsCard->setContentComponent(std::unique_ptr<juce::Component>(levelsMeter));
        levelsCard->setHeaderWidget(&levelsMeter->getTargetMenu());

        vuMeterCard = std::make_unique<MeterCardComponent>("VU METER", GoodMeterLookAndFeel::accentYellow, true);
        vuMeter = new VUMeterComponent();
        vuMeterCard->setContentComponent(std::unique_ptr<juce::Component>(vuMeter));

        threeBandCard = std::make_unique<MeterCardComponent>("3-BAND", GoodMeterLookAndFeel::accentPurple, true);
        band3Meter = new Band3Component(processor);
        threeBandCard->setContentComponent(std::unique_ptr<juce::Component>(band3Meter));

        spectrumCard = std::make_unique<MeterCardComponent>("SPECTRUM", GoodMeterLookAndFeel::accentCyan, true);
        spectrumAnalyzer = new SpectrumAnalyzerComponent(processor);
        spectrumCard->setContentComponent(std::unique_ptr<juce::Component>(spectrumAnalyzer));

        phaseCard = std::make_unique<MeterCardComponent>("PHASE", GoodMeterLookAndFeel::accentBlue, true);
        phaseMeter = new PhaseCorrelationComponent();
        phaseCard->setContentComponent(std::unique_ptr<juce::Component>(phaseMeter));

        stereoImageCard = std::make_unique<MeterCardComponent>("STEREO", GoodMeterLookAndFeel::accentSoftPink, true);
        stereoImageMeter = new StereoImageComponent(processor);
        stereoImageCard->setContentComponent(std::unique_ptr<juce::Component>(stereoImageMeter));

        spectrogramCard = std::make_unique<MeterCardComponent>("SPECTROGRAM", GoodMeterLookAndFeel::accentYellow, true);
        spectrogramMeter = new SpectrogramComponent(processor);
        spectrogramCard->setContentComponent(std::unique_ptr<juce::Component>(spectrogramMeter));

        psrCard = std::make_unique<MeterCardComponent>("PSR", juce::Colour(0xFF20C997), true);
        psrMeter = new PsrMeterComponent(processor);
        psrCard->setContentComponent(std::unique_ptr<juce::Component>(psrMeter));

        // Add all cards to content
        contentComponent->addAndMakeVisible(levelsCard.get());
        contentComponent->addAndMakeVisible(vuMeterCard.get());
        contentComponent->addAndMakeVisible(threeBandCard.get());
        contentComponent->addAndMakeVisible(spectrumCard.get());
        contentComponent->addAndMakeVisible(phaseCard.get());
        contentComponent->addAndMakeVisible(stereoImageCard.get());
        contentComponent->addAndMakeVisible(spectrogramCard.get());
        contentComponent->addAndMakeVisible(psrCard.get());

        // Card height change callbacks -> relayout
        auto relayoutCb = [this]() { layoutCards(); };
        levelsCard->onHeightChanged = relayoutCb;
        vuMeterCard->onHeightChanged = relayoutCb;
        threeBandCard->onHeightChanged = relayoutCb;
        spectrumCard->onHeightChanged = relayoutCb;
        phaseCard->onHeightChanged = relayoutCb;
        stereoImageCard->onHeightChanged = relayoutCb;
        spectrogramCard->onHeightChanged = relayoutCb;
        psrCard->onHeightChanged = relayoutCb;

        startTimerHz(30);
    }

    ~MetersPageComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Header row
        auto headerRow = bounds.removeFromTop(44);
        titleLabel.setBounds(headerRow.removeFromLeft(120).reduced(12, 8));
        headerLabel.setBounds(headerRow.reduced(12, 8));

        // Viewport fills the rest
        viewport->setBounds(bounds);

        layoutCards();
    }

    //==========================================================================
    // Pinch-to-zoom: change column count
    //==========================================================================
    void magnifyGestureChanged(float scaleFactor)
    {
        if (scaleFactor > 1.15f && columnOverride > 1)
        {
            columnOverride--;
            layoutCards();
        }
        else if (scaleFactor < 0.85f && columnOverride < 3)
        {
            columnOverride++;
            layoutCards();
        }
    }

private:
    void timerCallback() override
    {
        // Read atomic values from processor
        float peakL = processor.peakLevelL.load(std::memory_order_relaxed);
        float peakR = processor.peakLevelR.load(std::memory_order_relaxed);
        float rmsL = processor.rmsLevelL.load(std::memory_order_relaxed);
        float rmsR = processor.rmsLevelR.load(std::memory_order_relaxed);
        float momentary = processor.lufsLevel.load(std::memory_order_relaxed);
        float shortTerm = processor.lufsShortTerm.load(std::memory_order_relaxed);
        float integrated = processor.lufsIntegrated.load(std::memory_order_relaxed);
        float phase = processor.phaseCorrelation.load(std::memory_order_relaxed);
        float luRangeVal = processor.luRange.load(std::memory_order_relaxed);

        // Update setter-based components
        if (levelsMeter != nullptr)
            levelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);

        if (vuMeter != nullptr)
            vuMeter->updateVU(rmsL, rmsR);

        if (phaseMeter != nullptr)
            phaseMeter->updateCorrelation(phase);

        // Update header with current file
        if (audioEngine.isFileLoaded())
            headerLabel.setText("Now: " + audioEngine.getCurrentFileName(),
                                juce::dontSendNotification);
    }

    void layoutCards()
    {
        auto viewBounds = viewport->getBounds();
        const int width = viewBounds.getWidth();
        const int spacing = 6;

        // Responsive column count
        int numColumns;
        if (columnOverride > 0)
        {
            numColumns = columnOverride;
        }
        else
        {
            numColumns = (width >= 900) ? 3 : ((width >= 500) ? 2 : 1);
        }

        const int columnWidth = (width - spacing * (numColumns + 1)) / numColumns;
        const int cardHeight = 280;

        MeterCardComponent* allCards[] = {
            levelsCard.get(), vuMeterCard.get(),
            threeBandCard.get(), spectrumCard.get(),
            phaseCard.get(), stereoImageCard.get(),
            spectrogramCard.get(), psrCard.get()
        };
        const int numCards = 8;

        // Column-based layout
        std::vector<int> columnHeights(numColumns, spacing);

        for (int i = 0; i < numCards; ++i)
        {
            // Find shortest column
            int col = 0;
            for (int c = 1; c < numColumns; ++c)
                if (columnHeights[c] < columnHeights[col])
                    col = c;

            int x = spacing + col * (columnWidth + spacing);
            int h = allCards[i]->getExpanded() ? cardHeight : 56;

            allCards[i]->setBounds(x, columnHeights[col], columnWidth, h);
            columnHeights[col] += h + spacing;
        }

        // Set content height
        int maxH = 0;
        for (int h : columnHeights)
            maxH = juce::jmax(maxH, h);

        contentComponent->setBounds(0, 0, width, juce::jmax(viewBounds.getHeight() + 1, maxH));
    }

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;

    juce::Label headerLabel;
    juce::Label titleLabel;

    // Cards
    std::unique_ptr<MeterCardComponent> levelsCard, vuMeterCard, threeBandCard, spectrumCard;
    std::unique_ptr<MeterCardComponent> phaseCard, stereoImageCard, spectrogramCard, psrCard;

    // Meter components (raw pointers — owned by their cards)
    LevelsMeterComponent* levelsMeter = nullptr;
    VUMeterComponent* vuMeter = nullptr;
    Band3Component* band3Meter = nullptr;
    SpectrumAnalyzerComponent* spectrumAnalyzer = nullptr;
    PhaseCorrelationComponent* phaseMeter = nullptr;
    StereoImageComponent* stereoImageMeter = nullptr;
    SpectrogramComponent* spectrogramMeter = nullptr;
    PsrMeterComponent* psrMeter = nullptr;

    int columnOverride = 0;  // 0 = auto, 1/2/3 = pinch-override
};
