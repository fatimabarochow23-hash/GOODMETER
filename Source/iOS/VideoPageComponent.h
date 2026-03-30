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

    void paint(juce::Graphics&) override {}

    void mouseUp(const juce::MouseEvent&) override
    {
        if (onTap != nullptr)
            onTap();
    }
};

class VideoSwipeOverlay : public juce::Component
{
public:
    std::function<void(int direction)> onSwipe;

    void paint(juce::Graphics&) override {}

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStart = e.position;
        dragActive = true;
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
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
    void resized() override;

    bool loadVideo(const juce::File& file);
    void clearVideo();
    juce::String getCurrentVideoPath() const;
    bool shouldConsumeHorizontalSwipe(juce::Point<float> point) const;

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
    juce::Rectangle<int> getPresentedVideoBounds(juce::Rectangle<int> hostBounds) const;
    EmbeddedMeterKind wrapMeterKind(int rawIndex) const;
    bool attachSyncedAudioIfAvailable();
    void syncAudioTransportToPosition(double positionSeconds, bool preservePlayingState);
    juce::File getExpectedAudioFileForVideo(const juce::File& videoFile) const;
    void setPlayButtonVisualState(bool shouldShowPlaying);

    void timerCallback() override;
    static juce::String fmtTime(double seconds);
    double getDurationSeconds() const;
    int getDrawerHeight(bool landscape) const;
    juce::Rectangle<int> getDrawerBounds() const;

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
    bool userRequestedPlayingState = false;
    int playbackIntentHoldFrames = 0;
    double forcedPausePosition = 0.0;
};
