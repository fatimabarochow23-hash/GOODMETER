/*
  ==============================================================================
    VideoPageComponent.h
    GOODMETER iOS - Page 5: Video player with hidden drawer transport
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"
#include "../MeterCardComponent.h"
#include "../LevelsMeterComponent.h"
#include "../VUMeterComponent.h"
#include "../Band3Component.h"
#include "../SpectrumAnalyzerComponent.h"
#include "../PhaseCorrelationComponent.h"
#include "../StereoImageComponent.h"
#include "../SpectrogramComponent.h"
#include "../PsrMeterComponent.h"
#include "MarkerModel.h"

#define MARATHON_ART_STYLE 1

#if MARATHON_ART_STYLE
    #include "MarathonRenderer.h"
#endif

class VideoDrawerToggleButton : public juce::Button
{
public:
    VideoDrawerToggleButton() : juce::Button("videoDrawerToggle") {}

    void setOpen(bool shouldBeOpen)
    {
        if (isOpen == shouldBeOpen)
            return;

        isOpen = shouldBeOpen;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool) override
    {
        auto area = getLocalBounds().toFloat();
        auto accent = GoodMeterLookAndFeel::textMain.withAlpha(isHovered ? 0.92f : 0.74f);

        auto pill = area.withSizeKeepingCentre(34.0f, 4.0f);
        g.setColour(accent.withAlpha(0.22f));
        g.fillRoundedRectangle(pill, 2.0f);

        juce::Path chevron;
        auto icon = area.withSizeKeepingCentre(10.0f, 6.0f);

        if (isOpen)
        {
            chevron.startNewSubPath(icon.getX(), icon.getBottom());
            chevron.lineTo(icon.getCentreX(), icon.getY());
            chevron.lineTo(icon.getRight(), icon.getBottom());
        }
        else
        {
            chevron.startNewSubPath(icon.getX(), icon.getY());
            chevron.lineTo(icon.getCentreX(), icon.getBottom());
            chevron.lineTo(icon.getRight(), icon.getY());
        }

        g.setColour(accent);
        g.strokePath(chevron, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    bool isOpen = false;
};

class VideoPlayPauseButton : public juce::Component
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
        auto ink = fillColour;

        float dim = juce::jmin(b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float>(dim, dim).withCentre(b.getCentre());
        g.setColour(ink);
        g.fillEllipse(circle);

        g.setColour(iconColour);
        auto iconArea = circle.reduced(dim * 0.28f);

        if (playing)
        {
            float barW = iconArea.getWidth() * 0.28f;
            float gap = iconArea.getWidth() * 0.15f;
            float cx = iconArea.getCentreX();
            g.fillRoundedRectangle(cx - gap - barW, iconArea.getY(), barW, iconArea.getHeight(), 1.4f);
            g.fillRoundedRectangle(cx + gap, iconArea.getY(), barW, iconArea.getHeight(), 1.4f);
        }
        else
        {
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
        if (onClick != nullptr)
            onClick();
    }

private:
    juce::Colour fillColour = GoodMeterLookAndFeel::textMain;
    juce::Colour iconColour = GoodMeterLookAndFeel::bgMain;
};

class VideoTapOverlay : public juce::Component
{
public:
    std::function<void()> onTap;
    std::function<void()> onDoubleTap;
    std::function<void()> onLongPress;

    void paint(juce::Graphics&) override {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        pressStart = e.position;
        pressStartMs = juce::Time::getMillisecondCounterHiRes();
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        const auto heldMs = juce::Time::getMillisecondCounterHiRes() - pressStartMs;
        const auto moved = e.position.getDistanceFrom(pressStart);

        if (heldMs >= 380.0 && moved <= 18.0f)
        {
            if (onLongPress != nullptr)
                onLongPress();
            return;
        }

        if (e.getNumberOfClicks() >= 2)
        {
            if (onDoubleTap != nullptr)
                onDoubleTap();
            return;
        }

        if (onTap != nullptr)
            onTap();
    }

private:
    juce::Point<float> pressStart;
    double pressStartMs = 0.0;
};

class VideoSwipeOverlay : public juce::Component
{
public:
    std::function<void(int direction)> onSwipe;
    std::function<void()> onDoubleTap;

    void paint(juce::Graphics&) override {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStart = e.position;
        dragActive = true;
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.getNumberOfClicks() >= 2)
        {
            dragActive = false;
            if (onDoubleTap != nullptr)
                onDoubleTap();
            return;
        }

        if (!dragActive)
            return;

        dragActive = false;
        const auto delta = e.position - dragStart;
        if (std::abs(delta.x) < 24.0f || std::abs(delta.x) < std::abs(delta.y))
            return;

        if (onSwipe != nullptr)
            onSwipe(delta.x < 0.0f ? 1 : -1);
    }

private:
    juce::Point<float> dragStart;
    bool dragActive = false;
};

class VideoPageComponent : public juce::Component,
                           private juce::Timer
{
public:
    VideoPageComponent(GOODMETERAudioProcessor& processor, iOSAudioEngine& audioEngine);
    ~VideoPageComponent() override;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    bool loadVideo(const juce::File& file);
    void clearVideo();
    juce::String getCurrentVideoPath() const;
    juce::String getTransportDisplayName() const;
    bool hasLoadedVideo() const;
    bool ownsSharedAudioTransport() const;
    bool isTransportPlaying() const;
    double getTransportPositionSeconds() const;
    double getTransportDurationSeconds() const;
    void playTransport();
    void pauseTransport();
    void rewindTransport();
    void seekTransport(double seconds);
    void jumpToEndTransport();
    bool shouldConsumeHorizontalSwipe(juce::Point<float> point) const;
    void setDarkTheme(bool dark);
    bool isDark() const { return isDarkTheme; }
    std::function<bool()> isMarkerModeActive;
    std::function<void()> addMarkerAtCurrentPosition;
    std::function<std::vector<GoodMeterMarkerItem>()> getCurrentMarkerItems;

private:
    class NativeVideoPlayer;
    enum class EmbeddedMeterKind
    {
        levels = 0,
        vu,
        band3,
        spectrum,
        phase,
        stereo,
        spectrogram,
        psr
    };

    void setDrawerOpen(bool shouldOpen);
    void rebuildMeterSlot(bool topSlot);
    void cycleMeterSlot(bool topSlot, int direction);
    void applyMeterCardTheme(MeterCardComponent* card);
    void applyEmbeddedMeterTheme(juce::Component* content);
    void updateDrawerThemeColors();
    juce::Rectangle<int> getPresentedVideoBounds(juce::Rectangle<int> hostBounds) const;
    EmbeddedMeterKind wrapMeterKind(int rawIndex) const;
    bool attachSyncedAudioIfAvailable();
    void syncAudioTransportToPosition(double positionSeconds, bool preservePlayingState);
    juce::File getExpectedAudioFileForVideo(const juce::File& videoFile) const;
    void setPlayButtonVisualState(bool shouldShowPlaying);
    void queueProgressSeek(double seconds);
    void flushQueuedProgressSeek(bool force);
    void refreshVideoPlaybackSurface(bool preservePlaybackState, const juce::String& reason);

    void timerCallback() override;
    static juce::String fmtTime(double seconds);
    double getDurationSeconds() const;
    int getDrawerHeight(bool landscape) const;
    juce::Rectangle<int> getDrawerBounds() const;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

#if MARATHON_ART_STYLE
    void randomizeBackground();
    void rippleUpdate();
    void triggerFanRipple(int originX, int originY, int dx, int dy, int dragDist);
    void updateFanRipple();
    void updateLongPressRipple();
#endif

    GOODMETERAudioProcessor& processor;
    iOSAudioEngine& audioEngine;
    std::unique_ptr<NativeVideoPlayer> nativePlayer;
    juce::Label fileNameLabel;
    juce::Label hintLabel;
    juce::Label currentTimeLabel;
    juce::Label remainingTimeLabel;
    juce::Slider progressSlider;
    juce::Slider volumeSlider;
    juce::TextButton rewindBtn;
    juce::TextButton skipBackBtn;
    juce::TextButton skipFwdBtn;
    juce::TextButton stopBtn;
    VideoPlayPauseButton playPauseBtn;
    VideoTapOverlay tapOverlay;
    VideoDrawerToggleButton drawerToggle;
    VideoSwipeOverlay topMeterSwipe;
    VideoSwipeOverlay bottomMeterSwipe;

    std::unique_ptr<MeterCardComponent> topMeterCard;
    std::unique_ptr<MeterCardComponent> bottomMeterCard;
    EmbeddedMeterKind topMeterKind = EmbeddedMeterKind::levels;
    EmbeddedMeterKind bottomMeterKind = EmbeddedMeterKind::vu;
    juce::Rectangle<int> topMeterBounds;
    juce::Rectangle<int> bottomMeterBounds;
    float topMeterAlpha = 1.0f;
    float bottomMeterAlpha = 1.0f;

    LevelsMeterComponent* topLevelsMeter = nullptr;
    VUMeterComponent* topVuMeter = nullptr;
    PhaseCorrelationComponent* topPhaseMeter = nullptr;
    LevelsMeterComponent* bottomLevelsMeter = nullptr;
    VUMeterComponent* bottomVuMeter = nullptr;
    PhaseCorrelationComponent* bottomPhaseMeter = nullptr;

    juce::Rectangle<int> videoBounds;
    juce::String currentFileName;
    juce::String currentFilePath;
    juce::String syncedAudioPath;
    bool hasVideoLoaded = false;
    bool syncedAudioLoaded = false;
    bool drawerOpen = false;
    bool lastLandscapeLayout = false;
    bool isDarkTheme = false;
    bool userRequestedPlayingState = false;
    int playbackIntentHoldFrames = 0;
    double forcedPausePosition = 0.0;
    bool progressScrubDragging = false;
    bool progressSeekPending = false;
    double pendingProgressSeekSeconds = 0.0;
    std::uint32_t lastProgressSeekCommitMs = 0;
    double lastObservedVideoPosition = 0.0;
    int stagnantVideoFrameCount = 0;
    int stagnantVideoRecoveryAttempts = 0;
    std::uint32_t lastVideoRecoveryMs = 0;

#if MARATHON_ART_STYLE
    std::unique_ptr<DotMatrixCanvas> bgCanvas;
    bool rippleActive = false;
    int rippleCenterX = 0;
    int rippleCenterY = 0;
    float rippleRadius = 0.0f;
    float rippleVelocity = 0.5f;
    const float rippleAcceleration = 0.15f;
    float autoRippleTimer = 0.0f;
    int autoRipplePhase = 0;

    // Interactive ripple system
    bool longPressActive = false;
    int longPressCenterX = 0;
    int longPressCenterY = 0;
    float longPressRadius = 0.0f;
    int dragStartX = 0;
    int dragStartY = 0;
    double pressStartTime = 0.0;
    bool wasDragged = false;
    const float longPressMaxRadius = 2.0f;
    int longPressWaveCount = 0;

    // Fan ripple system
    bool fanRippleActive = false;
    int fanOriginX = 0;
    int fanOriginY = 0;
    float fanDirectionX = 0.0f;
    float fanDirectionY = 0.0f;
    float fanMaxRadius = 0.0f;
    float fanRadius = 0.0f;
    float fanVelocity = 0.5f;
    const float fanAngle = 0.785398f;
#endif
};
