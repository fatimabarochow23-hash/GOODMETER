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
#include "MarkerModel.h"

#define MARATHON_ART_STYLE 1

#if MARATHON_ART_STYLE
    #include "MarathonRenderer.h"
#endif

// ─── Custom play/pause button: draws ▶ triangle or ⏸ pause bars ──────────
class PlayPauseButton : public juce::Component
{
public:
    std::function<void()> onClick;
    bool playing = false;

    void setColours(juce::Colour fill, juce::Colour icon)
    {
        fillColour = fill;
        iconColour = icon;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);

        // Filled circle background
        float dim = juce::jmin(b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float>(dim, dim).withCentre(b.getCentre());
        g.setColour(fillColour);
        g.fillEllipse(circle);

        // Draw icon
        g.setColour(iconColour);
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
            // Play: right-pointing triangle
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

private:
    juce::Colour fillColour = GoodMeterLookAndFeel::textMain;
    juce::Colour iconColour = GoodMeterLookAndFeel::bgMain;
};

class TransportGestureHandle : public juce::Component
{
public:
    std::function<void(const juce::MouseEvent&)> onDown;
    std::function<void(const juce::MouseEvent&)> onDrag;
    std::function<void(const juce::MouseEvent&)> onUp;

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (onDown) onDown(e);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (onDrag) onDrag(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (onUp) onUp(e);
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

    // Optional page-2 transport override for video sessions. Codex added this
    // so page 2 can control the active video-backed transport instead of only
    // the extracted audio engine, which previously caused hidden page-5 sync to
    // re-start playback right after the user pressed pause here.
    std::function<bool()> hasExternalTransport;
    std::function<bool()> isExternalTransportPlaying;
    std::function<double()> getExternalTransportPosition;
    std::function<double()> getExternalTransportLength;
    std::function<juce::String()> getExternalTransportName;
    std::function<void()> playExternalTransport;
    std::function<void()> pauseExternalTransport;
    std::function<void()> rewindExternalTransport;
    std::function<void(double)> seekExternalTransport;
    std::function<void()> jumpToEndExternalTransport;
    std::function<bool()> isMarkerModeActive;
    std::function<void()> addMarkerAtCurrentPosition;
    std::function<std::vector<GoodMeterMarkerItem>()> getCurrentMarkerItems;

    MetersPageComponent(GOODMETERAudioProcessor& proc, iOSAudioEngine& engine)
        : processor(proc), audioEngine(engine)
    {
#if MARATHON_ART_STYLE
        bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
        randomizeBackground();
#endif

        // Create viewport for scrolling
        viewport = std::make_unique<juce::Viewport>();
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(viewport.get());
        viewport->setViewedComponent(contentComponent.get(), false);
        viewport->setScrollBarsShown(false, false, true, false);
        viewport->addMouseListener(this, true);
        contentComponent->addMouseListener(this, true);
        // iPhone page 2 uses custom vertical drag forwarding from each card.
        // Disable Viewport's own drag scrolling so the two systems don't fight.
        viewport->setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);

        //==================================================================
        // Transport bar controls (direct children, Apple Music style)
        //==================================================================

        // Row 1: time current + progress slider + time remaining
        currentTimeLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
        currentTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        currentTimeLabel.setJustificationType(juce::Justification::centredRight);
        currentTimeLabel.setText("0:00", juce::dontSendNotification);
        GoodMeterLookAndFeel::markAsIOSEnglishMono(currentTimeLabel);
        addAndMakeVisible(currentTimeLabel);

        remainingTimeLabel.setFont(juce::Font(juce::FontOptions(11.5f)));
        remainingTimeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        remainingTimeLabel.setJustificationType(juce::Justification::centredLeft);
        remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
        GoodMeterLookAndFeel::markAsIOSEnglishMono(remainingTimeLabel);
        addAndMakeVisible(remainingTimeLabel);

        progressSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        progressSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        progressSlider.setRange(0.0, 1.0, 0.001);
        progressSlider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::textMain);
        progressSlider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.15f));
        progressSlider.setColour(juce::Slider::backgroundColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.08f));
        progressSlider.onDragStart = [this]()
        {
            transportScrubDragging = true;
            transportSeekPending = false;
            pendingTransportSeekSeconds = 0.0;
            lastTransportSeekCommitMs = 0;
        };
        progressSlider.onValueChange = [this]()
        {
            if (hasExternalTransport != nullptr && hasExternalTransport())
            {
                if (progressSlider.isMouseButtonDown() && seekExternalTransport != nullptr)
                {
                    const double total = getExternalTransportLength != nullptr ? getExternalTransportLength() : 0.0;
                    const double target = progressSlider.getValue() * total;
                    queueTransportSeek(target);
                    currentTimeLabel.setText(fmtTime(target), juce::dontSendNotification);
                    remainingTimeLabel.setText("-" + fmtTime(juce::jmax(0.0, total - target)),
                                               juce::dontSendNotification);
                }
                return;
            }

            if (progressSlider.isMouseButtonDown())
            {
                const double total = audioEngine.getTotalLength();
                const double target = progressSlider.getValue() * total;
                queueTransportSeek(target);
                currentTimeLabel.setText(fmtTime(target), juce::dontSendNotification);
                remainingTimeLabel.setText("-" + fmtTime(juce::jmax(0.0, total - target)),
                                           juce::dontSendNotification);
            }
        };
        progressSlider.onDragEnd = [this]()
        {
            transportScrubDragging = false;
            flushQueuedTransportSeek(true);
        };
        addAndMakeVisible(progressSlider);

        // Row 2: |<< << ▶/⏸ >> >>| + volume slider
        auto makeTransportBtn = [this](juce::TextButton& btn, const juce::String& text)
        {
            btn.setButtonText(text);
            btn.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            btn.setColour(juce::TextButton::textColourOffId, GoodMeterLookAndFeel::textMain);
            GoodMeterLookAndFeel::markAsIOSEnglishMono(btn);
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
            const bool useExternal = (hasExternalTransport != nullptr && hasExternalTransport());
            if (!useExternal && !audioEngine.isFileLoaded())
                return;

            const bool currentlyPlaying = useExternal
                ? (isExternalTransportPlaying != nullptr && isExternalTransportPlaying())
                : audioEngine.isPlaying();
            const bool shouldPlay = !currentlyPlaying;
            transportUserIntentActive = true;
            transportUserIntentPlaying = shouldPlay;
            transportUserIntentUntilMs = juce::Time::getMillisecondCounter() + 650;

            playPauseBtn.playing = shouldPlay;
            playPauseBtn.repaint();

            juce::Component::SafePointer<MetersPageComponent> safeThis(this);

            if (useExternal)
            {
                juce::Timer::callAfterDelay(16, [safeThis, shouldPlay]()
                {
                    if (safeThis == nullptr)
                        return;

                    if (shouldPlay)
                    {
                        if (safeThis->playExternalTransport != nullptr)
                            safeThis->playExternalTransport();
                    }
                    else if (safeThis->pauseExternalTransport != nullptr)
                    {
                        safeThis->pauseExternalTransport();
                    }
                });

                return;
            }

            if (!audioEngine.isFileLoaded())
                return;

            juce::Timer::callAfterDelay(16, [safeThis, shouldPlay]()
            {
                if (safeThis == nullptr || !safeThis->audioEngine.isFileLoaded())
                    return;

                if (shouldPlay)
                    safeThis->audioEngine.play();
                else
                    safeThis->audioEngine.pause();
            });
        };

        rewindBtn.onClick = [this]()
        {
            if (hasExternalTransport != nullptr && hasExternalTransport())
            {
                if (rewindExternalTransport != nullptr)
                    rewindExternalTransport();
                return;
            }

            audioEngine.stop();
        };
        skipBackBtn.onClick = [this]()
        {
            if (hasExternalTransport != nullptr && hasExternalTransport())
            {
                if (seekExternalTransport != nullptr)
                {
                    const double pos = getExternalTransportPosition != nullptr ? getExternalTransportPosition() : 0.0;
                    seekExternalTransport(juce::jmax(0.0, pos - 5.0));
                }
                return;
            }

            double pos = audioEngine.getCurrentPosition();
            audioEngine.seek(juce::jmax(0.0, pos - 5.0));
        };
        skipFwdBtn.onClick = [this]()
        {
            if (hasExternalTransport != nullptr && hasExternalTransport())
            {
                if (seekExternalTransport != nullptr)
                {
                    const double pos = getExternalTransportPosition != nullptr ? getExternalTransportPosition() : 0.0;
                    const double total = getExternalTransportLength != nullptr ? getExternalTransportLength() : 0.0;
                    seekExternalTransport(juce::jmin(pos + 5.0, total));
                }
                return;
            }

            double pos = audioEngine.getCurrentPosition();
            double total = audioEngine.getTotalLength();
            audioEngine.seek(juce::jmin(pos + 5.0, total));
        };
        stopBtn.onClick = [this]()
        {
            if (hasExternalTransport != nullptr && hasExternalTransport())
            {
                if (jumpToEndExternalTransport != nullptr)
                    jumpToEndExternalTransport();
                return;
            }

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
        transportFileLabel.setFont(juce::Font(juce::FontOptions(12.5f)));
        transportFileLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        transportFileLabel.setJustificationType(juce::Justification::centred);
        transportFileLabel.setMinimumHorizontalScale(1.0f);
        GoodMeterLookAndFeel::markAsIOSEnglishMono(transportFileLabel);
        addAndMakeVisible(transportFileLabel);

        transportGestureHandle.onDown = [this](const juce::MouseEvent& e)
        {
            transportHandleDragging = true;
            dragStartTransportReveal = transportReveal;
            dragStartTransportY = e.getScreenPosition().y;
            transportTargetReveal = transportReveal;
        };
        transportGestureHandle.onDrag = [this](const juce::MouseEvent& e)
        {
            if (!transportHandleDragging)
                return;

            const bool landscape = isLandscapeLayout();
            const float dragRange = (float) juce::jmax(1, getExpandedTransportBarHeight(landscape)
                                                          - getCollapsedTransportBarHeight(landscape));
            const float deltaY = (float) (e.getScreenPosition().y - dragStartTransportY);
            transportReveal = juce::jlimit(0.0f, 1.0f, dragStartTransportReveal - deltaY / dragRange);
            resized();
            repaint();
        };
        transportGestureHandle.onUp = [this](const juce::MouseEvent&)
        {
            if (!transportHandleDragging)
                return;

            transportHandleDragging = false;
            transportTargetReveal = (transportReveal < 0.56f) ? 0.0f : 1.0f;
        };
        addAndMakeVisible(transportGestureHandle);

        //==================================================================
        // Create all 8 meter cards
        //==================================================================
        levelsCard = std::make_unique<MeterCardComponent>("LEVELS", GoodMeterLookAndFeel::accentPink, true);
        levelsMeter = new LevelsMeterComponent(processor);
        levelsMeter->setupTargetMenu();
        levelsCard->setContentComponent(std::unique_ptr<juce::Component>(levelsMeter));

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
            card.useMonospacedTitleFont = true;
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
        g.fillAll(isDarkTheme ? juce::Colour(0xFF07080B) : GoodMeterLookAndFeel::bgMain);

        const bool landscape = isLandscapeLayout();

        if (bgCanvas != nullptr)
        {
#if MARATHON_ART_STYLE
            if (isDarkTheme)
            {
                auto monoFont = juce::Font(juce::Font::getDefaultMonospacedFontName(),
                                           landscape ? 15.5f : 18.0f,
                                           juce::Font::plain);
                bgCanvas->drawToGraphics(g, getLocalBounds().toFloat(), monoFont);
            }
            else
            {
                juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(),
                                    landscape ? 15.0f : 17.5f,
                                    juce::Font::plain);
                const auto bounds = getLocalBounds().toFloat();
                const int gridH = bgCanvas->getHeight();
                const int gridW = bgCanvas->getWidth();
                const float cellW = bounds.getWidth() / gridW;
                const float cellH = bounds.getHeight() / gridH;

                g.setFont(monoFont);
                for (int y = 0; y < gridH; ++y)
                {
                    for (int x = 0; x < gridW; ++x)
                    {
                        auto cell = bgCanvas->getCell(x, y);
                        if (cell.symbol == U' ')
                            continue;

                        auto tint = GoodMeterLookAndFeel::textMain.withAlpha(0.050f + cell.brightness * 0.125f);
                        g.setColour(tint);
                        juce::String str = juce::String::charToString(cell.symbol);
                        g.drawText(str,
                                   juce::roundToInt(bounds.getX() + x * cellW),
                                   juce::roundToInt(bounds.getY() + y * cellH),
                                   juce::roundToInt(cellW),
                                   juce::roundToInt(cellH),
                                   juce::Justification::centred,
                                   false);
                    }
                }
            }
#endif
        }

        if (!transportPlateBounds.isEmpty() && transportReveal > 0.04f)
        {
            auto plate = transportPlateBounds.toFloat();
            const float radius = 18.0f;
            const auto plateFill = isDarkTheme
                                       ? juce::Colour(0xFF0B1017).withAlpha(0.15f)
                                       : juce::Colour(0xFFFFFFFF).withAlpha(0.18f);
            const auto plateOutline = isDarkTheme
                                          ? juce::Colour(0xFFF6EEE3).withAlpha(0.08f)
                                          : juce::Colour(0xFF1A1A24).withAlpha(0.06f);
            juce::Path platePath;
            platePath.addRoundedRectangle(plate, radius);

            g.setColour(plateFill);
            g.fillPath(platePath);

            g.setColour(plateOutline);
            g.drawRoundedRectangle(plate.reduced(0.5f), radius, 0.95f);
        }

        if (transportReveal < 0.98f)
        {
            const auto hookArea = getTransportHookBounds(landscape).toFloat();
            const auto hookColour = isDarkTheme
                ? juce::Colour(0xFFF6EEE3).withAlpha(0.92f)
                : GoodMeterLookAndFeel::textMain.withAlpha(0.78f);
            g.setColour(hookColour);

            juce::Path hook;
            const float w = hookArea.getWidth();
            const float h = hookArea.getHeight();
            const float x = hookArea.getX();
            const float y = hookArea.getY();
            hook.startNewSubPath(x + w * 0.16f, y + h * 0.28f);
            hook.quadraticTo(x + w * 0.24f, y + h * 0.80f, x + w * 0.50f, y + h * 0.80f);
            hook.quadraticTo(x + w * 0.76f, y + h * 0.80f, x + w * 0.84f, y + h * 0.28f);
            g.strokePath(hook, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        if (getCurrentMarkerItems == nullptr)
            return;

        const auto markers = getCurrentMarkerItems();
        if (markers.empty() || !progressSlider.isVisible())
            return;

        double total = 0.0;
        if (hasExternalTransport != nullptr && hasExternalTransport())
            total = getExternalTransportLength != nullptr ? getExternalTransportLength() : 0.0;
        else
            total = audioEngine.getTotalLength();

        if (total <= 0.001)
            return;

        auto rail = progressSlider.getBounds().toFloat();
        const float centerY = rail.getCentreY();
        for (const auto& marker : markers)
        {
            const float t = (float) juce::jlimit(0.0, 1.0, marker.seconds / total);
            const float x = rail.getX() + rail.getWidth() * t;
            const auto glowColour = marker.colour.withAlpha(isDarkTheme ? 0.20f : 0.14f);
            const auto dotColour = marker.colour.withAlpha(isDarkTheme ? 0.94f : 0.78f);
            g.setColour(glowColour);
            g.fillEllipse(x - 4.6f, centerY - 4.6f, 9.2f, 9.2f);
            g.setColour(dotColour);
            g.fillEllipse(x - 2.05f, centerY - 2.05f, 4.1f, 4.1f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const bool landscape = isLandscapeLayout();

        auto topArea = bounds.removeFromTop(getTopAreaHeight(landscape));
        layoutTopArea(topArea, landscape);

        // Viewport owns the full remaining page; transport is an overlay above it.
        viewport->setBounds(bounds);
        layoutTransport(bounds, landscape);
        layoutCards();
    }

    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        if (isMarkerModeActive != nullptr
            && isMarkerModeActive()
            && addMarkerAtCurrentPosition != nullptr)
        {
            addMarkerAtCurrentPosition();
        }
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

    void setDarkTheme(bool dark)
    {
        isDarkTheme = dark;
        GoodMeterLookAndFeel::setEditorialPopupMode(true, dark);
        updateThemeColors();

        if (levelsMeter != nullptr)
            levelsMeter->setMarathonDarkStyle(dark);
        if (phaseMeter != nullptr)
            phaseMeter->setMarathonDarkStyle(dark);
        if (stereoImageMeter != nullptr)
            stereoImageMeter->setMarathonDarkStyle(dark);
        if (vuMeter != nullptr)
            vuMeter->setMarathonDarkStyle(dark);
        if (spectrumAnalyzer != nullptr)
            spectrumAnalyzer->setMarathonDarkStyle(dark);
        if (spectrogramMeter != nullptr)
            spectrogramMeter->setMarathonDarkStyle(dark);
        if (band3Meter != nullptr)
            band3Meter->setMarathonDarkStyle(dark);
        if (psrMeter != nullptr)
            psrMeter->setMarathonDarkStyle(dark);

        // Propagate to all meter cards
        for (auto* card : getAllCards())
        {
            if (card != nullptr)
            {
                card->isDarkTheme = dark;
                card->useEditorialDarkStyle = dark;
                card->useEditorialLightStyle = !dark;
            }
        }

        repaint();
    }

    void setLoudnessStandard(int standardId)
    {
        if (levelsMeter != nullptr)
            levelsMeter->setStandardById(standardId);
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
            card->useMonospacedTitleFont = true;

            if (compactMode)
                card->setExpanded(true, false);

            card->resized();
            card->repaint();
        }
    }

    void updateThemeColors()
    {
        auto transportTextCol = isDarkTheme ? juce::Colour(0xFFF3EEE4)
                                            : GoodMeterLookAndFeel::textMain;
        auto transportBgCol = isDarkTheme ? juce::Colour(0xFF1E2230)
                                          : GoodMeterLookAndFeel::bgMain;
        auto thumbCol = transportTextCol;
        auto trackCol = transportTextCol.withAlpha(isDarkTheme ? 0.28f : 0.15f);
        auto railCol = transportTextCol.withAlpha(isDarkTheme ? 0.10f : 0.08f);

        currentTimeLabel.setColour(juce::Label::textColourId, transportTextCol.withAlpha(isDarkTheme ? 0.96f : 0.78f));
        remainingTimeLabel.setColour(juce::Label::textColourId, transportTextCol.withAlpha(isDarkTheme ? 0.96f : 0.78f));
        transportFileLabel.setColour(juce::Label::textColourId, transportTextCol);

        progressSlider.setColour(juce::Slider::thumbColourId, thumbCol);
        progressSlider.setColour(juce::Slider::trackColourId, trackCol);
        progressSlider.setColour(juce::Slider::backgroundColourId, railCol);

        volumeSlider.setColour(juce::Slider::thumbColourId, thumbCol);
        volumeSlider.setColour(juce::Slider::trackColourId, trackCol);
        volumeSlider.setColour(juce::Slider::backgroundColourId, railCol);

        rewindBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
        skipBackBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
        skipFwdBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);
        stopBtn.setColour(juce::TextButton::textColourOffId, transportTextCol);

        playPauseBtn.setColours(isDarkTheme ? juce::Colour(0xFFF3EEE4) : transportTextCol,
                                isDarkTheme ? juce::Colour(0xFF1E2230) : transportBgCol);
    }

    bool useLightPanelCardsInDarkTheme() const
    {
        return false;
    }

#if MARATHON_ART_STYLE
    void randomizeBackground()
    {
        // Codex: 主人希望第 2 页 dark 不是普通黑底，而是更像 Marathon
        // 的信息图底板。我这里只做 iOS page shell 的符号场，不碰共享 meter。
        static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
        juce::Random rng;
        const auto preset = MarathonField::Preset::audio;

        for (int y = 0; y < bgCanvas->getHeight(); ++y)
        {
            int consecutiveCount = 0;
            char32_t lastSymbol = 0;

            for (int x = 0; x < bgCanvas->getWidth(); ++x)
            {
                if (MarathonField::shouldLeaveBlank(x, y, bgCanvas->getWidth(), bgCanvas->getHeight(), preset))
                {
                    bgCanvas->setCell(x, y, U' ', juce::Colours::white, 0, 0.0f);
                    lastSymbol = U' ';
                    consecutiveCount = 0;
                    continue;
                }

                int idx = rng.nextInt(static_cast<int>(std::size(symbols)));
                char32_t sym = symbols[(size_t) idx];

                if (sym == lastSymbol)
                {
                    consecutiveCount++;
                    if (consecutiveCount >= 3)
                    {
                        do
                        {
                            idx = rng.nextInt(static_cast<int>(std::size(symbols)));
                            sym = symbols[(size_t) idx];
                        } while (sym == lastSymbol);
                        consecutiveCount = 0;
                    }
                }
                else
                {
                    consecutiveCount = 0;
                }

                if (x % 7 == 0 && (sym == U'.' || sym == U'·'))
                    sym = U'□';
                else if (y % 6 == 0 && (sym == U'.' || sym == U'·'))
                    sym = U'+';

                lastSymbol = sym;
                auto brightness = MarathonField::brightnessForCell(x, y,
                                                                   bgCanvas->getWidth(),
                                                                   bgCanvas->getHeight(),
                                                                   preset);
                bgCanvas->setCell(x, y, sym, juce::Colours::white, 0, brightness);
            }
        }
    }
#endif

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
        const int expanded = getExpandedTransportBarHeight(landscape);
        const int collapsed = getCollapsedTransportBarHeight(landscape);
        return juce::roundToInt((float) collapsed + ((float) expanded - (float) collapsed) * transportReveal);
    }

    int getExpandedTransportBarHeight(bool landscape) const
    {
        return landscape ? landscapeTransportBarH : portraitTransportBarH;
    }

    int getCollapsedTransportBarHeight(bool landscape) const
    {
        return landscape ? 18 : 22;
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
        const int expandedHeight = getExpandedTransportBarHeight(landscape);
        const int collapsedHeight = getCollapsedTransportBarHeight(landscape);
        const int hiddenOffset = juce::roundToInt(((float) expandedHeight - (float) collapsedHeight) * (1.0f - transportReveal));
        auto fullTransportArea = juce::Rectangle<int>(0, getHeight() - expandedHeight + hiddenOffset, getWidth(), expandedHeight);
        auto tb = fullTransportArea.reduced(landscape ? 16 : 16, landscape ? 2 : 4);
        transportPlateBounds = {};

        if (landscape)
        {
            transportPlateBounds = tb.reduced(6, 2);
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

            const float fileFontSize = rightArea.getWidth() > 230 ? 13.2f : 12.4f;
            transportFileLabel.setFont(juce::Font(juce::FontOptions(fileFontSize)));
            refreshTransportFileLabelText();

            auto progressArea = controlRow.withRight(buttonArea.getX() - groupGap);

            currentTimeLabel.setBounds(progressArea.removeFromLeft(42));
            remainingTimeLabel.setBounds(progressArea.removeFromRight(52));
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
            updateTransportChildPresentation();
            transportGestureHandle.setBounds(transportReveal < 0.45f ? getTransportHookBounds(landscape)
                                                                     : getExpandedGestureBounds(landscape));
            return;
        }

        transportPlateBounds = tb.reduced(4, 2);

        // File name (small, centered at top)
        transportFileLabel.setBounds(tb.removeFromTop(18));
        transportFileLabel.setFont(juce::Font(juce::FontOptions(12.4f)));
        transportFileLabel.setJustificationType(juce::Justification::centred);
        refreshTransportFileLabelText();

        // Row 1: time | progress slider | time
        auto progressRow = tb.removeFromTop(28);
        currentTimeLabel.setBounds(progressRow.removeFromLeft(42));
        remainingTimeLabel.setBounds(progressRow.removeFromRight(52));
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
        updateTransportChildPresentation();
        transportGestureHandle.setBounds(transportReveal < 0.45f ? getTransportHookBounds(landscape)
                                                                 : getExpandedGestureBounds(landscape));
    }

    juce::Rectangle<int> getExpandedGestureBounds(bool landscape) const
    {
        if (transportPlateBounds.isEmpty())
            return {};

        const auto plate = transportPlateBounds;
        const int width = landscape ? 120 : juce::jmin(160, plate.getWidth() - 20);
        const int height = landscape ? 18 : 22;
        return juce::Rectangle<int>(width, height)
            .withCentre({ plate.getCentreX(), plate.getY() + height / 2 + 2 });
    }

    juce::Rectangle<int> getTransportHookBounds(bool landscape) const
    {
        const int width = landscape ? 34 : 38;
        const int height = landscape ? 12 : 14;
        return juce::Rectangle<int>(width, height)
            .withCentre({ getWidth() / 2, getHeight() - getCollapsedTransportBarHeight(landscape) / 2 });
    }

    void updateTransportChildPresentation()
    {
        const float alpha = juce::jlimit(0.0f, 1.0f, (transportReveal - 0.10f) / 0.90f);
        const bool interactive = alpha > 0.35f;
        for (juce::Component* component : { static_cast<juce::Component*>(&currentTimeLabel),
                                            static_cast<juce::Component*>(&remainingTimeLabel),
                                            static_cast<juce::Component*>(&progressSlider),
                                            static_cast<juce::Component*>(&rewindBtn),
                                            static_cast<juce::Component*>(&skipBackBtn),
                                            static_cast<juce::Component*>(&playPauseBtn),
                                            static_cast<juce::Component*>(&skipFwdBtn),
                                            static_cast<juce::Component*>(&stopBtn),
                                            static_cast<juce::Component*>(&volumeSlider),
                                            static_cast<juce::Component*>(&transportFileLabel) })
        {
            if (component == nullptr)
                continue;
            component->setAlpha(alpha);
            component->setVisible(alpha > 0.02f);
            component->setEnabled(interactive);
        }
    }

    //==========================================================================
    // Timer: update meters + transport
    //==========================================================================
    void timerCallback() override
    {
        if (!transportHandleDragging && std::abs(transportReveal - transportTargetReveal) > 0.001f)
        {
            transportReveal += (transportTargetReveal - transportReveal) * 0.22f;
            if (std::abs(transportReveal - transportTargetReveal) < 0.01f)
                transportReveal = transportTargetReveal;
            resized();
            repaint();
        }

        flushQueuedTransportSeek(false);

        // Read atomic values from processor
        float peakL = processor.peakLevelL.load(std::memory_order_relaxed);
        float peakR = processor.peakLevelR.load(std::memory_order_relaxed);
        float rmsL = processor.rmsLevelL.load(std::memory_order_relaxed);
        float rmsR = processor.rmsLevelR.load(std::memory_order_relaxed);
        float momentary = processor.lufsLevel.load(std::memory_order_relaxed);
        float shortTerm = processor.lufsShortTerm.load(std::memory_order_relaxed);
        float integrated = processor.lufsIntegrated.load(std::memory_order_relaxed);
        float phase = processor.phaseCorrelation.load(std::memory_order_relaxed);

        // Codex: 主人在 iOS 第 2 页发现 Levels 里的 LU Range 一直像没接值。
        // 桌面端会先把 short-term LUFS 推进 LRA history，再实时计算 LRA；
        // iOS 这条 timer 之前漏了这两步，所以这里只补 page-2 的同款接线。
        if (++lraFrameCounter >= 6)
        {
            processor.pushShortTermLUFSForLRA(shortTerm);
            processor.calculateLRARealtime();
            lraFrameCounter = 0;
        }

        float luRangeVal = processor.luRange.load(std::memory_order_relaxed);

        // Update setter-based components
        if (levelsMeter != nullptr)
            levelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);

        if (vuMeter != nullptr)
            vuMeter->updateVU(rmsL, rmsR);

        if (phaseMeter != nullptr)
            phaseMeter->updateCorrelation(phase);

        // ── Update transport ──
        const bool useExternalTransportNow = (hasExternalTransport != nullptr && hasExternalTransport());
        if (useExternalTransportNow)
        {
            const auto nextName = getExternalTransportName != nullptr
                                      ? getExternalTransportName()
                                      : juce::String("No file loaded");
            if (transportFileLabelSourceText != nextName)
            {
                transportFileLabelSourceText = nextName;
                refreshTransportFileLabelText();
            }

            const double pos = getExternalTransportPosition != nullptr ? getExternalTransportPosition() : 0.0;
            const double total = getExternalTransportLength != nullptr ? getExternalTransportLength() : 0.0;

            currentTimeLabel.setText(fmtTime(pos), juce::dontSendNotification);
            remainingTimeLabel.setText("-" + fmtTime(total - pos), juce::dontSendNotification);

            if (!transportScrubDragging && total > 0.0)
                progressSlider.setValue(pos / total, juce::dontSendNotification);

            const bool nowPlaying = isExternalTransportPlaying != nullptr && isExternalTransportPlaying();
            const auto nowMs = juce::Time::getMillisecondCounter();

            if (transportUserIntentActive)
            {
                if (nowPlaying == transportUserIntentPlaying)
                {
                    transportUserIntentActive = false;
                }
                else if (static_cast<std::int32_t>(transportUserIntentUntilMs - nowMs) > 0)
                {
                    if (playPauseBtn.playing != transportUserIntentPlaying)
                    {
                        playPauseBtn.playing = transportUserIntentPlaying;
                        playPauseBtn.repaint();
                    }

                    return;
                }
                else
                {
                    transportUserIntentActive = false;
                }
            }

            if (playPauseBtn.playing != nowPlaying)
            {
                playPauseBtn.playing = nowPlaying;
                playPauseBtn.repaint();
            }
        }
        else if (audioEngine.isFileLoaded())
        {
            const auto nextName = audioEngine.getCurrentFileName();
            if (transportFileLabelSourceText != nextName)
            {
                transportFileLabelSourceText = nextName;
                refreshTransportFileLabelText();
            }

            double pos = audioEngine.getCurrentPosition();
            double total = audioEngine.getTotalLength();

            currentTimeLabel.setText(fmtTime(pos), juce::dontSendNotification);
            remainingTimeLabel.setText("-" + fmtTime(total - pos), juce::dontSendNotification);

            if (!transportScrubDragging && total > 0.0)
                progressSlider.setValue(pos / total, juce::dontSendNotification);

            const bool nowPlaying = audioEngine.isPlaying();
            const auto nowMs = juce::Time::getMillisecondCounter();

            if (transportUserIntentActive)
            {
                if (nowPlaying == transportUserIntentPlaying)
                {
                    transportUserIntentActive = false;
                }
                else if (static_cast<std::int32_t>(transportUserIntentUntilMs - nowMs) > 0)
                {
                    if (playPauseBtn.playing != transportUserIntentPlaying)
                    {
                        playPauseBtn.playing = transportUserIntentPlaying;
                        playPauseBtn.repaint();
                    }

                    return;
                }
                else
                {
                    transportUserIntentActive = false;
                }
            }

            if (playPauseBtn.playing != nowPlaying)
            {
                playPauseBtn.playing = nowPlaying;
                playPauseBtn.repaint();
            }
        }
        else
        {
            if (transportFileLabelSourceText != "No file loaded")
            {
                transportFileLabelSourceText = "No file loaded";
                refreshTransportFileLabelText();
            }
            currentTimeLabel.setText("0:00", juce::dontSendNotification);
            remainingTimeLabel.setText("-0:00", juce::dontSendNotification);
        }
    }

    static juce::String compressTransportNameToWidth(const juce::String& text,
                                                     const juce::Font& font,
                                                     float maxWidth)
    {
        const auto clean = juce::URL::removeEscapeChars(text.trim());
        if (clean.isEmpty())
            return {};

        if (font.getStringWidthFloat(clean) <= maxWidth)
            return clean;

        const int length = clean.length();
        int keepFront = juce::jmax(4, length / 2 - 2);
        int keepBack = juce::jmax(4, length - keepFront - 1);

        while (keepFront > 1 || keepBack > 1)
        {
            const auto candidate = clean.substring(0, keepFront)
                                 + juce::String::fromUTF8("…")
                                 + clean.substring(clean.length() - keepBack);
            if (font.getStringWidthFloat(candidate) <= maxWidth)
                return candidate;

            if (keepFront >= keepBack && keepFront > 1)
                --keepFront;
            else if (keepBack > 1)
                --keepBack;
            else
                break;
        }

        return clean.substring(0, juce::jmin(6, clean.length()))
             + juce::String::fromUTF8("…");
    }

    void refreshTransportFileLabelText()
    {
        const auto bounds = transportFileLabel.getBounds();
        if (bounds.getWidth() <= 4)
            return;

        const auto displayText = compressTransportNameToWidth(transportFileLabelSourceText,
                                                              transportFileLabel.getFont(),
                                                              (float) bounds.getWidth() - 8.0f);
        transportFileLabel.setText(displayText, juce::dontSendNotification);
    }

    static juce::String fmtTime(double t)
    {
        if (t < 0.0) t = 0.0;
        int m = static_cast<int>(t) / 60;
        int s = static_cast<int>(t) % 60;
        return juce::String(m) + ":" + juce::String(s).paddedLeft('0', 2);
    }

    void queueTransportSeek(double seconds)
    {
        pendingTransportSeekSeconds = juce::jmax(0.0, seconds);
        transportSeekPending = true;
    }

    void flushQueuedTransportSeek(bool force)
    {
        if (!transportSeekPending)
            return;

        const auto nowMs = juce::Time::getMillisecondCounter();
        if (!force && static_cast<std::int32_t>(nowMs - lastTransportSeekCommitMs) < 70)
            return;

        const bool useExternal = (hasExternalTransport != nullptr && hasExternalTransport());
        if (useExternal)
        {
            if (seekExternalTransport != nullptr)
                seekExternalTransport(pendingTransportSeekSeconds);
        }
        else
        {
            audioEngine.seek(pendingTransportSeekSeconds);
        }

        lastTransportSeekCommitMs = nowMs;
        transportSeekPending = false;
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
            const bool transportHidden = transportReveal < 0.35f;
            const int rowsVisible = transportHidden ? (landscape ? 2 : 3) : 1;
            const int expandedCardHeight = transportHidden
                ? juce::jmax(140, (viewHeight - spacing * (rowsVisible + 1)) / rowsVisible)
                : (landscape ? juce::jmax(280, viewHeight - spacing)
                             : 280);

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
    juce::Rectangle<int> transportPlateBounds;
    juce::String transportFileLabelSourceText = "No file loaded";
    TransportGestureHandle transportGestureHandle;

#if MARATHON_ART_STYLE
    std::unique_ptr<DotMatrixCanvas> bgCanvas;
#endif

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
    bool isDarkTheme = false;
    int lraFrameCounter = 0;
    bool transportUserIntentActive = false;
    bool transportUserIntentPlaying = false;
    std::uint32_t transportUserIntentUntilMs = 0;
    bool transportScrubDragging = false;
    bool transportSeekPending = false;
    double pendingTransportSeekSeconds = 0.0;
    std::uint32_t lastTransportSeekCommitMs = 0;
    float transportReveal = 1.0f;
    float transportTargetReveal = 1.0f;
    bool transportHandleDragging = false;
    float dragStartTransportReveal = 1.0f;
    int dragStartTransportY = 0;
};
