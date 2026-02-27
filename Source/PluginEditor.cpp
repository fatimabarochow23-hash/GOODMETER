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

    // Create meter cards (Phase 2: Empty placeholders, Phase 3: Add actual meters)
    levelsCard = std::make_unique<MeterCardComponent>(
        "LEVELS",
        GoodMeterLookAndFeel::accentPink,
        true  // Default expanded
    );

    vuMeterCard = std::make_unique<MeterCardComponent>(
        "VU METER",
        GoodMeterLookAndFeel::accentYellow,
        true
    );

    threeBandCard = std::make_unique<MeterCardComponent>(
        "3-BAND",
        GoodMeterLookAndFeel::accentPurple,
        false  // Default collapsed
    );

    spectrumCard = std::make_unique<MeterCardComponent>(
        "SPECTRUM",
        GoodMeterLookAndFeel::accentCyan,
        false
    );

    phaseCard = std::make_unique<MeterCardComponent>(
        "PHASE",
        GoodMeterLookAndFeel::accentGreen,
        false
    );

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

    // Add cards to content component
    contentComponent->addAndMakeVisible(levelsCard.get());
    contentComponent->addAndMakeVisible(vuMeterCard.get());
    contentComponent->addAndMakeVisible(threeBandCard.get());
    contentComponent->addAndMakeVisible(spectrumCard.get());
    contentComponent->addAndMakeVisible(phaseCard.get());
    contentComponent->addAndMakeVisible(stereoImageCard.get());
    contentComponent->addAndMakeVisible(spectrogramCard.get());

    // Set placeholder content height for each card
    // Phase 3 will replace these with actual meter components
    auto createPlaceholder = [](const juce::String& text) {
        auto* label = new juce::Label();
        label->setText(text, juce::dontSendNotification);
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        label->setSize(400, 100);
        return std::unique_ptr<juce::Component>(label);
    };

    levelsCard->setContentComponent(createPlaceholder("Peak, RMS, LUFS meters will be here"));
    vuMeterCard->setContentComponent(createPlaceholder("Classic VU meter will be here"));
    threeBandCard->setContentComponent(createPlaceholder("Low/Mid/High meters will be here"));
    spectrumCard->setContentComponent(createPlaceholder("Spectrum analyzer will be here"));
    phaseCard->setContentComponent(createPlaceholder("Phase correlation meter will be here"));
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
            int cardHeight = card->getDesiredHeight();
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
    // Phase 3: Read atomic values from processor and update meter components
    // For now, this is a placeholder

    // Example of how to read values (Phase 3 implementation):
    // float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
    // float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
    // float rmsL = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
    // float rmsR = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
    // float lufs = audioProcessor.lufsLevel.load(std::memory_order_relaxed);
    // float phase = audioProcessor.phaseCorrelation.load(std::memory_order_relaxed);

    // Update meter components with new values (Phase 3)
    // levelsMeter->setPeakLevels(peakL, peakR);
    // levelsMeter->setRMSLevels(rmsL, rmsR);
    // levelsMeter->setLUFS(lufs);
    // phaseMeter->setCorrelation(phase);

    // Trigger repaint for smooth 60Hz updates
    // repaint();
}
