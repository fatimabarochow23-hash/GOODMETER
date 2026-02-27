/*
  ==============================================================================
    PluginEditor.cpp
    GOODMETER - Main plugin editor implementation

    60Hz Timer-driven UI with vertical meter layout
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GOODMETERAudioProcessorEditor::GOODMETERAudioProcessorEditor(GOODMETERAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set custom LookAndFeel
    setLookAndFeel(&customLookAndFeel);

    // Create viewport and content container
    viewport = std::make_unique<juce::Viewport>();
    contentComponent = std::make_unique<juce::Component>();

    addAndMakeVisible(viewport.get());
    viewport->setViewedComponent(contentComponent.get(), false);
    viewport->setScrollBarsShown(true, false);  // Vertical scroll only

    //==========================================================================
    // Create meter cards
    //==========================================================================

    levelsCard = std::make_unique<MeterCardComponent>(
        "LEVELS",
        GoodMeterLookAndFeel::accentPink,
        true  // Default expanded
    );

    // Create Levels Meter and transfer ownership to card
    levelsMeter = new LevelsMeterComponent();
    levelsMeter->setStandard("EBU R128");  // Default standard
    levelsCard->setContentComponent(std::unique_ptr<juce::Component>(levelsMeter));

    vuMeterCard = std::make_unique<MeterCardComponent>(
        "VU METER",
        GoodMeterLookAndFeel::accentYellow,
        true  // ✅ Expanded to show VU meter
    );

    // Create VU Meter and transfer ownership to card
    vuMeter = new VUMeterComponent();
    vuMeterCard->setContentComponent(std::unique_ptr<juce::Component>(vuMeter));

    threeBandCard = std::make_unique<MeterCardComponent>(
        "3-BAND",
        GoodMeterLookAndFeel::accentPurple,
        false  // Default collapsed
    );

    spectrumCard = std::make_unique<MeterCardComponent>(
        "SPECTRUM",
        GoodMeterLookAndFeel::accentCyan,
        true  // ✅ Expanded to show spectrum analyzer
    );

    // Create Spectrum Analyzer and transfer ownership to card
    spectrumAnalyzer = new SpectrumAnalyzerComponent(audioProcessor);
    spectrumCard->setContentComponent(std::unique_ptr<juce::Component>(spectrumAnalyzer));

    phaseCard = std::make_unique<MeterCardComponent>(
        "PHASE",
        GoodMeterLookAndFeel::accentGreen,
        true  // Expanded for testing
    );

    // Create Phase Correlation Meter
    phaseMeter = new PhaseCorrelationComponent();
    phaseCard->setContentComponent(std::unique_ptr<juce::Component>(phaseMeter));

    stereoImageCard = std::make_unique<MeterCardComponent>(
        "STEREO",
        GoodMeterLookAndFeel::accentPink,
        false
    );

    spectrogramCard = std::make_unique<MeterCardComponent>(
        "SPECTROGRAM",
        GoodMeterLookAndFeel::accentYellow,
        false
    );

    // Bind height change callbacks to all cards
    // This allows cards to notify the editor to relayout when they expand/collapse
    auto cardStateChangedCallback = [this]() {
        this->resized();
    };

    levelsCard->onHeightChanged = cardStateChangedCallback;
    vuMeterCard->onHeightChanged = cardStateChangedCallback;
    threeBandCard->onHeightChanged = cardStateChangedCallback;
    spectrumCard->onHeightChanged = cardStateChangedCallback;
    phaseCard->onHeightChanged = cardStateChangedCallback;
    stereoImageCard->onHeightChanged = cardStateChangedCallback;
    spectrogramCard->onHeightChanged = cardStateChangedCallback;

    // Add cards to content component
    contentComponent->addAndMakeVisible(levelsCard.get());
    contentComponent->addAndMakeVisible(vuMeterCard.get());
    contentComponent->addAndMakeVisible(threeBandCard.get());
    contentComponent->addAndMakeVisible(spectrumCard.get());
    contentComponent->addAndMakeVisible(phaseCard.get());
    contentComponent->addAndMakeVisible(stereoImageCard.get());
    contentComponent->addAndMakeVisible(spectrogramCard.get());

    // Set placeholder content for remaining cards (Phase 3 will replace these)
    auto createPlaceholder = [](const juce::String& text) {
        auto* label = new juce::Label();
        label->setText(text, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        label->setSize(400, 100);
        return std::unique_ptr<juce::Component>(label);
    };

    // Note: levelsCard, vuMeterCard, phaseCard, and spectrumCard already have their content set above
    // DO NOT overwrite them with placeholders!
    threeBandCard->setContentComponent(createPlaceholder("Low/Mid/High meters will be here"));
    stereoImageCard->setContentComponent(createPlaceholder("Goniometer/Lissajous will be here"));
    spectrogramCard->setContentComponent(createPlaceholder("Waterfall spectrogram will be here"));

    // Set initial size (matches typical plugin dimensions)
    setSize(500, 700);

    // Start 60Hz timer for UI updates
    startTimerHz(60);
}

GOODMETERAudioProcessorEditor::~GOODMETERAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

//==============================================================================
void GOODMETERAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(GoodMeterLookAndFeel::bgMain);
}

void GOODMETERAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Position viewport to fill entire editor
    viewport->setBounds(bounds);

    // Layout meter cards vertically
    auto contentBounds = juce::Rectangle<int>(0, 0, bounds.getWidth(), 0);
    int yPos = GoodMeterLookAndFeel::cardSpacing;

    auto layoutCard = [&](MeterCardComponent* card) {
        if (card != nullptr)
        {
            // CRITICAL: Use actual current height, not getDesiredHeight()
            // This preserves animation state during 60Hz timer callbacks
            int cardHeight = card->getHeight();

            // Only update X, Y, Width - preserve animated Height
            card->setBounds(GoodMeterLookAndFeel::cardSpacing,
                          yPos,
                          bounds.getWidth() - GoodMeterLookAndFeel::cardSpacing * 2,
                          cardHeight);
            yPos += cardHeight + GoodMeterLookAndFeel::cardSpacing;
        }
    };

    layoutCard(levelsCard.get());
    layoutCard(vuMeterCard.get());
    layoutCard(threeBandCard.get());
    layoutCard(spectrumCard.get());
    layoutCard(phaseCard.get());
    layoutCard(stereoImageCard.get());
    layoutCard(spectrogramCard.get());

    // Set content component size
    contentComponent->setSize(bounds.getWidth(), yPos);
}

//==============================================================================
void GOODMETERAudioProcessorEditor::timerCallback()
{
    // Read atomic values from processor (thread-safe)
    float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
    float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
    float rmsL = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
    float rmsR = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
    float lufs = audioProcessor.lufsLevel.load(std::memory_order_relaxed);
    float phase = audioProcessor.phaseCorrelation.load(std::memory_order_relaxed);

    // Update Levels Meter
    if (levelsMeter != nullptr)
    {
        levelsMeter->updateMetrics(peakL, peakR, lufs);
    }

    // Update VU Meter
    if (vuMeter != nullptr)
    {
        vuMeter->updateVU(rmsL, rmsR);
    }

    // Update Phase Correlation Meter
    if (phaseMeter != nullptr)
    {
        phaseMeter->updateCorrelation(phase);
    }

    // Phase 3: Update other meter components here
    // vuMeter->updateValue(...);
    // etc.
}
