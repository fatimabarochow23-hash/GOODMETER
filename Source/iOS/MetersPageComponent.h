/*
  ==============================================================================
    MetersPageComponent.h
    GOODMETER iOS - Page 2: Scrollable meter cards + transport bar

    Responsive grid layout:
    - iPhone portrait: 1 column
    - iPhone landscape / iPad portrait: 2 columns
    - iPad landscape: 3 columns

    Transport bar (Apple Music style):
    - Row 1: Progress slider + time labels
    - Row 2: 5 control buttons + volume slider

    Timer bridge (30Hz): reads processor atomics, updates setter-based meters
  ==============================================================================
*/

#pragma once

#include <array>
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

// ─── Custom play/pause button: draws ▶ triangle or ⏸ pause bars ──────────
class PlayPauseButton : public juce::Component
{
public:
    std::function<void()> onClick;
    bool playing = false;

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);
        auto ink = GoodMeterLookAndFeel::textMain;

        // Filled dark circle background
        float dim = juce::jmin(b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float>(dim, dim).withCentre(b.getCentre());
        g.setColour(ink);
        g.fillEllipse(circle);

        // Draw icon in white
        g.setColour(GoodMeterLookAndFeel::bgMain);
        auto iconArea = circle.reduced(dim * 0.28f);

        if (playing)
        {
            // Pause: two vertical bars
            float barW = iconArea.getWidth() * 0.28f;
            float gap = iconArea.getWidth() * 0.15f;
            float cx = iconArea.getCentreX();
            g.fillRoundedRectangle(cx - gap - barW, iconArea.getY(),
                                   barW, iconArea.getHeight(), 1.5f);
            g.fillRoundedRectangle(cx + gap, iconArea.getY(),
                                   barW, iconArea.getHeight(), 1.5f);
        }
        else
        {
            // Play: right-pointing triangle (slight right offset for optical center)
            juce::Path tri;
            float offsetX = iconArea.getWidth() * 0.08f;
            tri.addTriangle(iconArea.getX() + offsetX, iconArea.getY(),
                            iconArea.getX() + offsetX, iconArea.getBottom(),
                            iconArea.getRight() + offsetX, iconArea.getCentreY());
            g.fillPath(tri);
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onClick) onClick();
    }
};

class MetersPageComponent : public juce::Component,
                             public juce::Timer
{
public:
    enum class DisplayMode
    {
        singleColumn = 0,
        fourUp = 1,
        eightUp = 2
    };

    MetersPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
        // Create viewport for scrolling
        viewport = std::make_unique<juce::Viewport>();
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(viewport.get());
        viewport->setViewedComponent(contentComponent.get(), false);
        viewport->setScrollBarsShown(false, false, true, false);
        // iPhone page 2 uses custom vertical drag forwarding from each card.
        // Disable Viewport's own drag scrolling so the two systems don't fight.
        viewport->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);

        //==================================================================
        // Transport bar controls (direct children, Apple Music style)
        //==================================================================

        // Row 1: time current + progress slider + time remaining
        currentTimeLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
        currentTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        currentTimeLabel.setJustificationType(juce::Justification::centredRight);
        currentTimeLabel.setText("0:00", juce::dontSendNotification);
        addAndMakeVisible(currentTimeLabel);

        remainingTimeLabel.setFont(juce::Font(juce::FontOptions(10.0f)));
        remainingTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        remainingTimeLabel.setJustificationType(juce::Justification::centredLeft);
        remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
        addAndMakeVisible(remainingTimeLabel);

        progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        progressSlider.setRange(0.0, 1.0, 0.001);
        progressSlider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::textMain);
        progressSlider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.15f));
        progressSlider.setColour(juce::Slider::backgroundColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
        progressSlider.onValueChange = [this]()
        {
            if (progressSlider.isMouseButtonDown())
            {
                double total = audioEngine.getTotalLength();
                audioEngine.seek(progressSlider.getValue() * total);
            }
        };
        addAndMakeVisible(progressSlider);

        // Row 2: |<< << ▶/⏸ >> >>| + volume slider
        auto makeTransportBtn = [this](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            btn.setColour(juce::TextButton::textColourOffId, GoodMeterLookAndFeel::textMain);
            addAndMakeVisible(btn);
        };

        makeTransportBtn(rewindBtn, "|<<");
        makeTransportBtn(skipBackBtn, "<<");
        makeTransportBtn(skipFwdBtn, ">>");
        makeTransportBtn(stopBtn, ">>|");

        // Play/Pause — custom drawn circle with ▶/⏸
        addAndMakeVisible(playPauseBtn);
        playPauseBtn.onClick = [this]()
        {
            if (!audioEngine.isFileLoaded()) return;
            if (audioEngine.isPlaying()) audioEngine.pause();
            else audioEngine.play();
        };

        rewindBtn.onClick = [this]() { audioEngine.stop(); };
        skipBackBtn.onClick = [this]()
        {
            double pos = audioEngine.getCurrentPosition();
            audioEngine.seek(juce::jmax(0.0, pos - 5.0));
        };
        skipFwdBtn.onClick = [this]()
        {
            double pos = audioEngine.getCurrentPosition();
            double total = audioEngine.getTotalLength();
            audioEngine.seek(juce::jmin(pos + 5.0, total));
        };
        stopBtn.onClick = [this]()
        {
            double total = audioEngine.getTotalLength();
            audioEngine.seek(juce::jmax(0.0, total - 0.1));
        };

        // Volume slider
        volumeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        volumeSlider.setRange(0.0, 1.0, 0.01);
        volumeSlider.setValue(audioEngine.getVolume());
        volumeSlider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::textMain);
        volumeSlider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.15f));
        volumeSlider.setColour(juce::Slider::backgroundColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
        volumeSlider.onValueChange = [this]() { audioEngine.setVolume((float) volumeSlider.getValue()); };
        addAndMakeVisible(volumeSlider);

        // File name label in transport
        transportFileLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        transportFileLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        transportFileLabel.setJustificationType(juce::Justification::centred);
        transportFileLabel.setMinimumHorizontalScale(0.72f);
        addAndMakeVisible(transportFileLabel);

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

        auto configureMobileCard = [](MeterCardComponent& card)
        {
            card.setMobileListMode(true);
        };

        configureMobileCard(*levelsCard);
        configureMobileCard(*vuMeterCard);
        configureMobileCard(*threeBandCard);
        configureMobileCard(*spectrumCard);
        configureMobileCard(*phaseCard);
        configureMobileCard(*stereoImageCard);
        configureMobileCard(*spectrogramCard);
        configureMobileCard(*psrCard);

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

        applyDisplayModeToCards();
        layoutCards();
        startTimerHz(30);
    }

    ~MetersPageComponent() override
    {
        stopTimer();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);

        // Transport bar separator line
        auto transportTop = getHeight() - getTransportBarHeight(isLandscapeLayout());
        g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.12f));
        g.drawHorizontalLine(transportTop, 0.0f, static_cast<float>(getWidth()));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const bool landscape = isLandscapeLayout();

        auto topArea = bounds.removeFromTop(getTopAreaHeight(landscape));
        layoutTopArea(topArea, landscape);

        // ── Transport bar at bottom ──
        auto transportArea = bounds.removeFromBottom(getTransportBarHeight(landscape));
        layoutTransport(transportArea, landscape);

        // Viewport fills the middle
        viewport->setBounds(bounds);
        layoutCards();
    }

    //==========================================================================
    // Pinch-to-zoom: change column count
    //==========================================================================
    void magnifyGestureChanged(float scaleFactor)
    {
        if (displayMode != DisplayMode::singleColumn)
            return;

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

    void setDisplayMode(int newMode)
    {
        auto clamped = juce::jlimit(0, 2, newMode);
        auto requestedMode = static_cast<DisplayMode>(clamped);
        if (displayMode == requestedMode)
            return;

        displayMode = requestedMode;
        columnOverride = 0;
        applyDisplayModeToCards();
        layoutCards();
        repaint();
    }

private:
    std::array<MeterCardComponent*, 8> getAllCards() const
    {
        return {
            levelsCard.get(), vuMeterCard.get(),
            threeBandCard.get(), spectrumCard.get(),
            phaseCard.get(), stereoImageCard.get(),
            spectrogramCard.get(), psrCard.get()
        };
    }

    void applyDisplayModeToCards()
    {
        const bool compactMode = (displayMode != DisplayMode::singleColumn);
        const bool allowToggle = (displayMode == DisplayMode::singleColumn);
        const bool forceLevelsVerticalMiniLayout = (displayMode == DisplayMode::eightUp);

        if (levelsMeter != nullptr)
            levelsMeter->setForceVerticalMiniLayout(forceLevelsVerticalMiniLayout);

        for (auto* card : getAllCards())
        {
            if (card == nullptr)
                continue;

            card->isMiniMode = compactMode;
            card->mobileAllowHeaderToggle = allowToggle;
            card->setMobileListMode(true);

            if (compactMode)
                card->setExpanded(true, false);

            card->resized();
            card->repaint();
        }
    }

    //==========================================================================
    // Transport bar layout (Apple Music style)
    //==========================================================================
    bool isLandscapeLayout() const
    {
        return getWidth() > getHeight();
    }

    int getTopAreaHeight(bool landscape) const
    {
        return landscape ? 28 : 44;
    }

    int getTransportBarHeight(bool landscape) const
    {
        return landscape ? landscapeTransportBarH : portraitTransportBarH;
    }

    void layoutTopArea(juce::Rectangle<int> area, bool landscape)
    {
        juce::ignoreUnused(area);

        if (!landscape)
        {
            transportFileLabel.setJustificationType(juce::Justification::centred);
            return;
        }

        transportFileLabel.setBounds({});
    }

    void layoutTransport(juce::Rectangle<int> area, bool landscape)
    {
        auto tb = area.reduced(landscape ? 16 : 16, landscape ? 2 : 4);

        if (landscape)
        {
            auto controlRow = tb.removeFromTop(tb.getHeight());
            const int smallBtnW = 28;
            const int playW = 40;
            const int btnGap = 6;
            const int groupGap = 10;
            const int volumeW = juce::jlimit(56, 84, controlRow.getWidth() / 10);
            const int buttonsW = smallBtnW * 4 + playW + btnGap * 4;
            const int fileGap = 8;
            const int rightGroupW = juce::jlimit(170, 320, juce::roundToInt(static_cast<float>(controlRow.getWidth()) * 0.34f));
            auto buttonArea = juce::Rectangle<int>(buttonsW, controlRow.getHeight())
                                  .withCentre(controlRow.getCentre());
            auto rightArea = controlRow.removeFromRight(rightGroupW);
            auto volumeArea = rightArea.removeFromRight(volumeW).reduced(0, 8);
            rightArea.removeFromRight(fileGap);
            transportFileLabel.setBounds(rightArea.reduced(0, 8));
            transportFileLabel.setJustificationType(juce::Justification::centredLeft);

            const auto fileName = transportFileLabel.getText();
            const auto fileNameLength = fileName.length();
            const float fileFontSize = fileNameLength > 24 ? 10.5f
                                      : fileNameLength > 16 ? 11.5f
                                      : 12.5f;
            transportFileLabel.setFont(juce::Font(juce::FontOptions(fileFontSize)));

            auto progressArea = controlRow.withRight(buttonArea.getX() - groupGap);

            currentTimeLabel.setBounds(progressArea.removeFromLeft(36));
            remainingTimeLabel.setBounds(progressArea.removeFromRight(40));
            progressSlider.setBounds(progressArea.reduced(0, 7));

            auto btnArea = buttonArea;
            rewindBtn.setBounds(btnArea.removeFromLeft(smallBtnW));
            btnArea.removeFromLeft(btnGap);
            skipBackBtn.setBounds(btnArea.removeFromLeft(smallBtnW));
            btnArea.removeFromLeft(btnGap);
            playPauseBtn.setBounds(btnArea.removeFromLeft(playW));
            btnArea.removeFromLeft(btnGap);
            skipFwdBtn.setBounds(btnArea.removeFromLeft(smallBtnW));
            btnArea.removeFromLeft(btnGap);
            stopBtn.setBounds(btnArea.removeFromLeft(smallBtnW));

            volumeSlider.setBounds(volumeArea);
            return;
        }

        // File name (small, centered at top)
        transportFileLabel.setBounds(tb.removeFromTop(18));
        transportFileLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        transportFileLabel.setJustificationType(juce::Justification::centred);

        // Row 1: time | progress slider | time
        auto progressRow = tb.removeFromTop(28);
        currentTimeLabel.setBounds(progressRow.removeFromLeft(36));
        remainingTimeLabel.setBounds(progressRow.removeFromRight(40));
        progressSlider.setBounds(progressRow);

        tb.removeFromTop(2);

        // Row 2: 5 buttons + volume slider
        auto controlRow = tb.removeFromTop(36);
        int btnW = 40;
        int playW = 36;  // square for circular play button

        // Center the 5 buttons, volume on the right
        int totalBtnW = btnW * 4 + playW + 4 * 4;  // 4 small + 1 play + gaps
        int startX = (controlRow.getWidth() - totalBtnW) / 3;  // offset left

        auto btnArea = controlRow.removeFromLeft(startX + totalBtnW);
        btnArea.removeFromLeft(startX);

        rewindBtn.setBounds(btnArea.removeFromLeft(btnW));
        btnArea.removeFromLeft(4);
        skipBackBtn.setBounds(btnArea.removeFromLeft(btnW));
        btnArea.removeFromLeft(4);
        playPauseBtn.setBounds(btnArea.removeFromLeft(playW));
        btnArea.removeFromLeft(4);
        skipFwdBtn.setBounds(btnArea.removeFromLeft(btnW));
        btnArea.removeFromLeft(4);
        stopBtn.setBounds(btnArea.removeFromLeft(btnW));

        // Volume slider takes remaining right space
        controlRow.removeFromLeft(8);
        volumeSlider.setBounds(controlRow.reduced(0, 6));
    }

    //==========================================================================
    // Timer: update meters + transport
    //==========================================================================
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

        // ── Update transport ──
        if (audioEngine.isFileLoaded())
        {
            transportFileLabel.setText(audioEngine.getCurrentFileName(),
                                        juce::dontSendNotification);

            double pos = audioEngine.getCurrentPosition();
            double total = audioEngine.getTotalLength();

            currentTimeLabel.setText(fmtTime(pos), juce::dontSendNotification);
            remainingTimeLabel.setText("-" + fmtTime(total - pos), juce::dontSendNotification);

            if (!progressSlider.isMouseButtonDown() && total > 0.0)
                progressSlider.setValue(pos / total, juce::dontSendNotification);

            bool nowPlaying = audioEngine.isPlaying();
            if (playPauseBtn.playing != nowPlaying)
            {
                playPauseBtn.playing = nowPlaying;
                playPauseBtn.repaint();
            }
        }
        else
        {
            transportFileLabel.setText("No file loaded", juce::dontSendNotification);
            currentTimeLabel.setText("0:00", juce::dontSendNotification);
            remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
        }
    }

    static juce::String fmtTime(double t)
    {
        if (t < 0.0) t = 0.0;
        int m = static_cast<int>(t) / 60;
        int s = static_cast<int>(t) % 60;
        return juce::String(m) + ":" + juce::String(s).paddedLeft('0', 2);
    }

    //==========================================================================
    // Card grid layout
    //==========================================================================
    void layoutCards()
    {
        const auto previousViewPos = viewport->getViewPosition();
        const int width = juce::jmax(1, viewport->getMaximumVisibleWidth());
        const int viewHeight = juce::jmax(1, viewport->getMaximumVisibleHeight());
        const bool landscape = isLandscapeLayout();
        const auto cards = getAllCards();
        const int numCards = static_cast<int>(cards.size());
        const bool compactMode = (displayMode != DisplayMode::singleColumn);
        const int spacing = compactMode ? 6 : 8;
        const int sideInset = compactMode ? 8 : 10;
        const int usableWidth = juce::jmax(120, width - sideInset * 2);

        int contentHeight = viewHeight + 1;

        if (displayMode == DisplayMode::singleColumn)
        {
            const int numColumns = 1;
            const int columnWidth = usableWidth;
            const int totalGridWidth = columnWidth * numColumns;
            const int startX = juce::jmax(sideInset, (width - totalGridWidth) / 2);
            const int expandedCardHeight = landscape ? juce::jmax(280, viewHeight - spacing)
                                                     : 280;

            int y = spacing;
            for (int i = 0; i < numCards; ++i)
            {
                auto* card = cards[static_cast<size_t>(i)];
                if (card == nullptr)
                    continue;

                const int h = card->getExpanded() ? expandedCardHeight : 56;
                card->setBounds(startX, y, columnWidth, h);
                y += h + spacing;
            }

            contentHeight = juce::jmax(viewHeight + 1, y);
        }
        else
        {
            const bool landscapeFourUp = (displayMode == DisplayMode::fourUp && landscape);
            const int compactSpacing = landscapeFourUp ? 4 : spacing;
            const int compactInset = landscapeFourUp ? 6 : sideInset;
            const int compactUsableWidth = juce::jmax(120, width - compactInset * 2);
            const int numColumns = (displayMode == DisplayMode::fourUp)
                                 ? (landscape ? 4 : 1)
                                 : (landscape ? 4 : 2);
            const int rowsVisible = (displayMode == DisplayMode::fourUp)
                                  ? (landscape ? 1 : 4)
                                  : (landscape ? 2 : 4);
            const int totalRows = (numCards + numColumns - 1) / numColumns;
            const int columnWidth = juce::jmax(120, (compactUsableWidth - compactSpacing * (numColumns - 1)) / numColumns);
            const int totalGridWidth = columnWidth * numColumns + compactSpacing * (numColumns - 1);
            const int startX = juce::jmax(compactInset, (width - totalGridWidth) / 2);
            const int availableHeight = juce::jmax(120, viewHeight - compactSpacing * (rowsVisible + 1));
            const int cardHeight = juce::jmax(88, availableHeight / rowsVisible);

            for (int i = 0; i < numCards; ++i)
            {
                auto* card = cards[static_cast<size_t>(i)];
                if (card == nullptr)
                    continue;

                const int row = i / numColumns;
                const int col = i % numColumns;
                const int x = startX + col * (columnWidth + compactSpacing);
                const int y = compactSpacing + row * (cardHeight + compactSpacing);
                card->setBounds(x, y, columnWidth, cardHeight);
            }

            contentHeight = juce::jmax(viewHeight + 1, compactSpacing + totalRows * (cardHeight + compactSpacing));
        }

        contentComponent->setBounds(0, 0, width, contentHeight);

        const int maxScrollY = juce::jmax(0, contentHeight - viewHeight);
        viewport->setViewPosition(0, juce::jlimit(0, maxScrollY, previousViewPos.y));
    }

    //==========================================================================
    // Members
    //==========================================================================
    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;

    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;

    // Transport bar (Apple Music style)
    static constexpr int portraitTransportBarH = 104;
    static constexpr int landscapeTransportBarH = 42;

    // Row 1: progress
    juce::Label currentTimeLabel;
    juce::Label remainingTimeLabel;
    juce::Slider progressSlider;

    // Row 2: 5 control buttons + volume
    juce::TextButton rewindBtn;      // |<<  reset to start
    juce::TextButton skipBackBtn;    // <<   -5s
    PlayPauseButton playPauseBtn;    // ▶/⏸ play/pause (custom drawn)
    juce::TextButton skipFwdBtn;     // >>   +5s
    juce::TextButton stopBtn;        // >>|  jump to end
    juce::Slider volumeSlider;

    // File name
    juce::Label transportFileLabel;

    // Cards
    std::unique_ptr<MeterCardComponent> levelsCard, vuMeterCard, threeBandCard, spectrumCard;
    std::unique_ptr<MeterCardComponent> phaseCard, stereoImageCard, spectrogramCard, psrCard;

    // Meter components (raw pointers -- owned by their cards)
    LevelsMeterComponent* levelsMeter = nullptr;
    VUMeterComponent* vuMeter = nullptr;
    Band3Component* band3Meter = nullptr;
    SpectrumAnalyzerComponent* spectrumAnalyzer = nullptr;
    PhaseCorrelationComponent* phaseMeter = nullptr;
    StereoImageComponent* stereoImageMeter = nullptr;
    SpectrogramComponent* spectrogramMeter = nullptr;
    PsrMeterComponent* psrMeter = nullptr;

    int columnOverride = 0;
    DisplayMode displayMode = DisplayMode::singleColumn;
};
