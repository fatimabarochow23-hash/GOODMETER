/*
  ==============================================================================
    HistoryPageComponent.h
    GOODMETER iOS - Page 4: Imported media history

    Shows files stored in the app's Documents directory:
      - Audio / Video segments
      - Long-press multi-select delete mode
      - Animated pyramid action button reveals LOAD
      - Delete selected files to free device storage
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
#include <set>
#include "../VideoAudioExtractor.h"
#include "IOSShareHelpers.h"
#include "MarkerModel.h"
#include "DigitalTimecodeRenderer.h"
#include "../../JuceLibraryCode/BinaryData.h"
#include "../GoodMeterLookAndFeel.h"

#define MARATHON_ART_STYLE 1

#if MARATHON_ART_STYLE
    #include "MarathonRenderer.h"
#endif

class HistorySegmentButton : public juce::Button
{
public:
    HistorySegmentButton(const juce::String& nameToUse, juce::Colour accentToUse)
        : juce::Button(nameToUse), accent(accentToUse)
    {
        setClickingTogglesState(true);
    }

    void setDarkMode(bool dark)
    {
        isDarkMode = dark;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.0f);

        auto fill = isDarkMode ? juce::Colours::black : GoodMeterLookAndFeel::bgMain;
        auto outline = getToggleState() ? accent : (isDarkMode ? juce::Colours::white.withAlpha(0.16f) : GoodMeterLookAndFeel::textMain.withAlpha(0.16f));

        if (getToggleState())
            fill = accent.withAlpha(isDarkMode ? 0.18f : 0.08f);
        else if (isHovered)
            fill = (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(0.03f);

        if (isPressed)
            fill = accent.withAlpha(isDarkMode ? 0.24f : 0.14f);

        const float radius = isDarkMode ? 9.0f : 12.0f;
        g.setColour(fill);
        g.fillRoundedRectangle(area, radius);

        g.setColour(outline);
        g.drawRoundedRectangle(area, radius, getToggleState() ? 2.0f : 1.2f);

        auto guideColour = getToggleState() ? accent.withAlpha(isDarkMode ? 0.55f : 0.35f)
                                            : (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(isDarkMode ? 0.16f : 0.10f);
        g.setColour(guideColour);
        g.drawLine(area.getX() + 12.0f, area.getY() + 10.0f,
                   area.getX() + 30.0f, area.getY() + 10.0f, 1.0f);

        auto dotArea = area.removeFromTop(20.0f).removeFromLeft(20.0f).reduced(6.0f);
        g.setColour(accent.withAlpha(getToggleState() ? 1.0f : 0.35f));
        g.fillEllipse(dotArea);

        g.setColour(isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f) : GoodMeterLookAndFeel::textMain);
        g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(16.5f, juce::Font::bold));
        g.drawText(getName(), getLocalBounds().reduced(12, 8), juce::Justification::centred, false);
    }

private:
    juce::Colour accent;
    bool isDarkMode = false;
};

class HistoryPyramidButton : public juce::Component,
                             private juce::Timer
{
public:
    explicit HistoryPyramidButton(juce::Colour accentToUse)
        : accent(accentToUse)
    {
        setInterceptsMouseClicks(true, false);
    }

    void setOpen(bool shouldBeOpen, bool animate)
    {
        if (isOpen == shouldBeOpen && !isAnimating)
            return;

        isOpen = shouldBeOpen;

        if (!animate)
        {
            rotationAngle = isOpen ? 180.0f : 0.0f;
            targetRotation = rotationAngle;
            isAnimating = false;
            stopTimer();
            repaint();
            return;
        }

        targetRotation += 180.0f;
        isAnimating = true;
        startTimerHz(60);
        repaint();
    }

    bool getOpen() const { return isOpen; }

    std::function<void(bool)> onToggle;

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto drawColour = accent.withAlpha(isHovering ? 0.95f : 0.75f);
        drawPyramid(g, area.reduced(2.0f), rotationAngle, drawColour, isHovering);
    }

    void mouseMove(const juce::MouseEvent&) override
    {
        if (!isHovering)
        {
            isHovering = true;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isHovering)
        {
            isHovering = false;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        setOpen(!isOpen, true);

        if (onToggle)
            onToggle(isOpen);
    }

private:
    void timerCallback() override
    {
        const float delta = targetRotation - rotationAngle;

        if (std::abs(delta) < 2.0f)
        {
            rotationAngle = targetRotation;
            isAnimating = false;
            stopTimer();

            if (std::abs(rotationAngle) >= 360.0f)
            {
                rotationAngle = std::fmod(rotationAngle, 360.0f);
                targetRotation = rotationAngle;
            }

            repaint();
            return;
        }

        rotationAngle += delta * 0.24f;
        repaint();
    }

    static void drawPyramid(juce::Graphics& g,
                            juce::Rectangle<float> bounds,
                            float rotation,
                            juce::Colour colour,
                            bool isHovered)
    {
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float size = bounds.getHeight() * 0.82f;

        const float baseAngle = 45.0f;
        const float totalRotation = baseAngle + rotation;
        const float rotRad = juce::degreesToRadians(totalRotation);
        const float cosR = std::cos(rotRad);
        const float sinR = std::sin(rotRad);

        struct Vertex3D { float x, y, z; };
        const Vertex3D vertices[4] =
        {
            { 0.0f,  -0.5f,  0.0f  },
            { -0.433f, 0.25f, -0.25f },
            { 0.433f,  0.25f, -0.25f },
            { 0.0f,    0.25f,  0.5f  }
        };

        juce::Point<float> projected[4];
        for (int i = 0; i < 4; ++i)
        {
            const float x = vertices[i].x * cosR + vertices[i].z * sinR;
            const float y = vertices[i].y;

            projected[i].x = cx + x * size;
            projected[i].y = cy + y * size;
        }

        g.setColour(colour);
        const float lineThickness = isHovered ? 2.0f : 1.55f;

        g.drawLine(projected[1].x, projected[1].y, projected[2].x, projected[2].y, lineThickness);
        g.drawLine(projected[2].x, projected[2].y, projected[3].x, projected[3].y, lineThickness);
        g.drawLine(projected[3].x, projected[3].y, projected[1].x, projected[1].y, lineThickness);

        g.drawLine(projected[0].x, projected[0].y, projected[1].x, projected[1].y, lineThickness);
        g.drawLine(projected[0].x, projected[0].y, projected[2].x, projected[2].y, lineThickness);
        g.drawLine(projected[0].x, projected[0].y, projected[3].x, projected[3].y, lineThickness);
    }

    juce::Colour accent;
    bool isHovering = false;
    bool isOpen = false;
    bool isAnimating = false;
    float rotationAngle = 0.0f;
    float targetRotation = 0.0f;
};

class HistoryLoadButton : public juce::Button
{
public:
    HistoryLoadButton() : juce::Button("HistoryLoadButton")
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setDarkMode(bool dark)
    {
        isDarkMode = dark;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto area = getLocalBounds().toFloat();
        auto textColour = isDarkMode ? juce::Colour(0xFFF6EEE3)
                                     : GoodMeterLookAndFeel::textMain;

        if (isPressed)
        {
            g.setColour(textColour.withAlpha(isDarkMode ? 0.12f : 0.08f));
            g.fillRoundedRectangle(area.reduced(1.0f), 8.0f);
        }
        else if (isHovered)
        {
            g.setColour(textColour.withAlpha(isDarkMode ? 0.07f : 0.05f));
            g.fillRoundedRectangle(area.reduced(1.0f), 8.0f);
        }

        g.setColour(textColour);
        g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(13.0f, juce::Font::bold));
        g.drawText("LOAD", getLocalBounds(), juce::Justification::centred, false);
    }

private:
    bool isDarkMode = false;
};

class HistoryActionButton : public juce::Button
{
public:
    HistoryActionButton(const juce::String& labelText, juce::Colour accentToUse)
        : juce::Button(labelText), accent(accentToUse)
    {
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void setDarkMode(bool dark)
    {
        isDarkMode = dark;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto area = getLocalBounds().toFloat().reduced(0.5f);
        auto fill = isDarkMode ? juce::Colours::white.withAlpha(0.03f)
                               : GoodMeterLookAndFeel::bgPanel.withAlpha(0.92f);
        auto outline = accent.withAlpha(isDarkMode ? 0.48f : 0.34f);
        auto textColour = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                                     : GoodMeterLookAndFeel::textMain;

        if (isHovered)
            fill = accent.withAlpha(isDarkMode ? 0.12f : 0.07f);

        if (isPressed)
            fill = accent.withAlpha(isDarkMode ? 0.18f : 0.12f);

        g.setColour(fill);
        g.fillRoundedRectangle(area, 14.0f);
        g.setColour(outline);
        g.drawRoundedRectangle(area, 14.0f, 1.25f);

        g.setColour(textColour);
        g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(14.0f, juce::Font::bold));
        g.drawText(getName(), getLocalBounds(), juce::Justification::centred, false);
    }

private:
    juce::Colour accent;
    bool isDarkMode = false;
};

class HistoryDrawerHandle : public juce::Component
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

class HistoryRowComponent : public juce::Component,
                            private juce::Timer
{
public:
    HistoryRowComponent(const juce::File& fileToUse, juce::Colour accentToUse)
        : file(fileToUse), accent(accentToUse), pyramidButton(accentToUse)
    {
        setInterceptsMouseClicks(true, true);

        fileDisplayName = juce::URL::removeEscapeChars(file.getFileName());
        nameTextColour = GoodMeterLookAndFeel::textMain;

        metaLabel.setText(buildMetaText(file), juce::dontSendNotification);
        metaLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        metaLabel.setFont(makeHistoryReadableFont(16.0f));
        metaLabel.setJustificationType(juce::Justification::centredLeft);
        metaLabel.setMinimumHorizontalScale(0.92f);
        addAndMakeVisible(metaLabel);

        loadButton.setEnabled(false);
        loadButton.setVisible(false);
        loadButton.setAlpha(0.0f);
        loadButton.onClick = [this]()
        {
            if (onLoadRequested)
                onLoadRequested(file);
        };
        addAndMakeVisible(loadButton);

        pyramidButton.onToggle = [this](bool open)
        {
            if (onPyramidToggleRequested)
                onPyramidToggleRequested(file, open);
        };
        addAndMakeVisible(pyramidButton);
    }

    juce::String getFilePath() const { return file.getFullPathName(); }
    int64_t getFileSize() const { return file.getSize(); }

    void setDarkMode(bool dark)
    {
        isDarkMode = dark;
        auto textColor = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f) : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
        auto mutedColor = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.72f) : GoodMeterLookAndFeel::textMuted;
        nameTextColour = textColor;
        metaLabel.setColour(juce::Label::textColourId, mutedColor);
        loadButton.setDarkMode(isDarkMode);
        repaint();
    }

    void setSelectionMode(bool shouldBeEnabled)
    {
        if (selectionMode == shouldBeEnabled)
            return;

        selectionMode = shouldBeEnabled;

        if (selectionMode)
            setActionOpen(false, false);

        pyramidButton.setVisible(!selectionMode);
        resized();
        repaint();
    }

    void setSelected(bool shouldBeSelected)
    {
        if (isSelected == shouldBeSelected)
            return;

        isSelected = shouldBeSelected;
        repaint();
    }

    void setActionOpen(bool shouldBeOpen, bool animate)
    {
        pyramidButton.setOpen(shouldBeOpen, animate);
        targetLoadAlpha = shouldBeOpen ? 1.0f : 0.0f;

        if (targetLoadAlpha > 0.0f)
            loadButton.setVisible(true);

        loadButton.setEnabled(targetLoadAlpha > 0.0f && !selectionMode);

        if (!animate)
        {
            loadAlpha = targetLoadAlpha;
            loadButton.setAlpha(loadAlpha);
            loadButton.setVisible(loadAlpha > 0.001f);
            resized();
            repaint();
            return;
        }

        startTimerHz(60);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.0f);

        auto fill = (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(isDarkMode ? 0.035f : 0.022f);
        auto outline = (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(isDarkMode ? 0.14f : 0.11f);

        if (isSelected)
        {
            fill = accent.withAlpha(0.12f);
            outline = accent.withAlpha(0.75f);
        }

        g.setColour(fill);
        g.fillRoundedRectangle(area, 14.0f);
        g.setColour(outline);
        g.drawRoundedRectangle(area, 14.0f, isSelected ? 2.0f : 1.2f);

        if (selectionMode)
        {
            auto circle = selectionCircleArea.toFloat();
            g.setColour((isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(0.88f));
            g.drawEllipse(circle, 1.8f);

            if (isSelected)
            {
                g.setColour(accent);
                g.fillEllipse(circle.reduced(2.8f));
                g.setColour((isDarkMode ? juce::Colours::black : GoodMeterLookAndFeel::bgMain).withAlpha(0.95f));
                g.fillEllipse(circle.withSizeKeepingCentre(circle.getWidth() * 0.26f,
                                                           circle.getHeight() * 0.26f));
            }
        }

        drawMixedFileName(g, nameArea.toFloat(), fileDisplayName, nameTextColour);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12, 10);

        if (selectionMode)
        {
            auto selectArea = area.removeFromLeft(28);
            selectionCircleArea = selectArea.withSizeKeepingCentre(18, 18);
            area.removeFromLeft(8);
        }
        else
        {
            selectionCircleArea = {};
        }

        if (!selectionMode)
        {
            int loadWidth = juce::roundToInt(56.0f * loadAlpha);
            auto rightArea = area.removeFromRight(30 + (loadWidth > 0 ? loadWidth + 8 : 0));

            if (loadWidth > 0)
            {
                auto loadArea = rightArea.removeFromLeft(loadWidth);
                loadButton.setBounds(loadArea.withTrimmedTop(8).withTrimmedBottom(8));
                rightArea.removeFromLeft(8);
            }
            else
            {
                loadButton.setBounds({});
            }

            pyramidButton.setBounds(rightArea.removeFromRight(44).withSizeKeepingCentre(42, 42));
        }
        else
        {
            pyramidButton.setBounds({});
            loadButton.setBounds({});
        }

        nameArea = area.removeFromTop(28);
        metaLabel.setBounds(area.removeFromTop(22));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        pointerDown = true;
        pointerMoved = false;
        longPressTriggered = false;
        pressStartPos = event.getPosition();
        pressStartMs = juce::Time::getMillisecondCounterHiRes();

        if (!selectionMode)
            startTimerHz(60);
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if ((event.getPosition() - pressStartPos).getDistanceFromOrigin() > 8)
            pointerMoved = true;
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        const bool shouldToggleSelection = selectionMode
                                           && !longPressTriggered
                                           && !pointerMoved;

        pointerDown = false;
        pointerMoved = false;

        if (shouldToggleSelection && onSelectionToggleRequested)
            onSelectionToggleRequested(file);

        if (std::abs(targetLoadAlpha - loadAlpha) < 0.01f)
            stopTimer();
    }

    std::function<void(const juce::File&)> onLoadRequested;
    std::function<void(const juce::File&, bool)> onPyramidToggleRequested;
    std::function<void(const juce::File&)> onLongPressRequested;
    std::function<void(const juce::File&)> onSelectionToggleRequested;

private:
    void timerCallback() override
    {
        bool keepRunning = false;

        if (pointerDown && !selectionMode && !pointerMoved && !longPressTriggered)
        {
            keepRunning = true;

            if (juce::Time::getMillisecondCounterHiRes() - pressStartMs >= 360.0)
            {
                longPressTriggered = true;
                pointerDown = false;

                if (onLongPressRequested)
                    onLongPressRequested(file);
            }
        }

        if (std::abs(targetLoadAlpha - loadAlpha) > 0.01f)
        {
            keepRunning = true;
            loadAlpha += (targetLoadAlpha - loadAlpha) * 0.24f;

            if (std::abs(targetLoadAlpha - loadAlpha) < 0.02f)
                loadAlpha = targetLoadAlpha;

            loadButton.setAlpha(loadAlpha);
            loadButton.setVisible(loadAlpha > 0.001f);
            loadButton.setEnabled(loadAlpha > 0.88f && !selectionMode);
            resized();
            repaint();
        }
        else
        {
            loadAlpha = targetLoadAlpha;
            loadButton.setAlpha(loadAlpha);
            loadButton.setVisible(loadAlpha > 0.001f);
            loadButton.setEnabled(loadAlpha > 0.88f && !selectionMode);
        }

        if (!keepRunning && std::abs(targetLoadAlpha - loadAlpha) < 0.01f)
            stopTimer();
    }

    static juce::String formatFileSize(int64_t bytes)
    {
        constexpr double kb = 1024.0;
        constexpr double mb = kb * 1024.0;
        constexpr double gb = mb * 1024.0;

        if (bytes >= static_cast<int64_t>(gb))
            return juce::String(static_cast<double>(bytes) / gb, 2) + " GB";

        if (bytes >= static_cast<int64_t>(mb))
            return juce::String(static_cast<double>(bytes) / mb, 1) + " MB";

        if (bytes >= static_cast<int64_t>(kb))
            return juce::String(static_cast<double>(bytes) / kb, 1) + " KB";

        return juce::String(bytes) + " B";
    }

    static juce::String buildMetaText(const juce::File& file)
    {
        auto sizeText = formatFileSize(file.getSize());
        auto timeText = file.getLastModificationTime().formatted("%Y-%m-%d %H:%M");
        return sizeText + "  "
             + juce::String(juce::CharPointer_UTF8("\xE2\x80\xA2")) + "  "
             + timeText;
    }

public:
    static juce::Font makeHistoryReadableFont(float size, int styleFlags = juce::Font::plain)
    {
        auto font = juce::Font("PingFang SC", size, styleFlags);
        if (font.getTypefaceName().containsIgnoreCase("PingFang"))
            return font;

        font = juce::Font("Hiragino Sans GB", size, styleFlags);
        if (font.getTypefaceName().containsIgnoreCase("Hiragino"))
            return font;

        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, styleFlags);
    }

private:
    static bool isMixedCjkLike(juce::juce_wchar c) noexcept
    {
        const bool isCjk = (c >= 0x3400 && c <= 0x4DBF)
                        || (c >= 0x4E00 && c <= 0x9FFF)
                        || (c >= 0xF900 && c <= 0xFAFF);
        if (isCjk)
            return true;

        return (c >= 0x3000 && c <= 0x303F)
            || (c >= 0xFF00 && c <= 0xFFEF)
            || c == 0x2014 || c == 0x2015 || c == 0x2026;
    }

    static juce::String compressMiddle(juce::String text)
    {
        if (text.length() <= 20)
            return text;

        const int keepFront = juce::jmax(7, text.length() / 2 - 4);
        const int keepBack = juce::jmax(6, text.length() - keepFront - 1);
        return text.substring(0, keepFront) + juce::String::fromUTF8("…")
             + text.substring(text.length() - keepBack);
    }

    static void drawMixedFileName(juce::Graphics& g,
                                  juce::Rectangle<float> area,
                                  juce::String text,
                                  juce::Colour colour)
    {
        if (text.isEmpty() || area.getWidth() <= 8.0f || area.getHeight() <= 6.0f)
            return;

        auto displayText = text.trim();
        auto font = makeHistoryReadableFont(17.0f, juce::Font::plain);
        const float maxWidth = juce::jmax(10.0f, area.getWidth() - 2.0f);
        for (int attempt = 0; attempt < 10; ++attempt)
        {
            if (font.getStringWidthFloat(displayText) <= maxWidth)
                break;
            displayText = compressMiddle(displayText);
        }

        const float contentHeight = font.getHeight();
        const float baselineY = area.getY() + (area.getHeight() - contentHeight) * 0.46f;
        const auto inkBoost = colour.withAlpha(colour.getFloatAlpha() * (colour.getPerceivedBrightness() > 0.7f ? 0.18f : 0.12f));

        juce::Graphics::ScopedSaveState state(g);
        g.reduceClipRegion(area.toNearestInt());
        g.setFont(font);
        g.setColour(inkBoost);
        g.drawText(displayText,
                   juce::Rectangle<float>(area.getX() + 0.18f, baselineY, area.getWidth(), contentHeight + 3.0f),
                   juce::Justification::centredLeft, false);
        g.setColour(colour);
        g.drawText(displayText,
                   juce::Rectangle<float>(area.getX(), baselineY, area.getWidth(), contentHeight + 3.0f),
                   juce::Justification::centredLeft, false);
    }

    juce::File file;
    juce::Colour accent;
    juce::String fileDisplayName;
    juce::Colour nameTextColour;
    bool selectionMode = false;
    bool isSelected = false;
    bool isDarkMode = false;

    bool pointerDown = false;
    bool pointerMoved = false;
    bool longPressTriggered = false;
    juce::Point<int> pressStartPos;
    double pressStartMs = 0.0;

    float loadAlpha = 0.0f;
    float targetLoadAlpha = 0.0f;

    juce::Rectangle<int> selectionCircleArea;
    juce::Rectangle<int> nameArea;
    juce::Label metaLabel;
    HistoryLoadButton loadButton;
    HistoryPyramidButton pyramidButton;
};

class HistoryMarkerRowComponent : public juce::Component
{
public:
    class MarkerTagChipButton : public juce::Button
    {
    public:
        MarkerTagChipButton(const juce::String& textToUse, juce::Colour accentToUse)
            : juce::Button(textToUse), accent(accentToUse)
        {
            setClickingTogglesState(true);
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void setDarkMode(bool dark)
        {
            isDarkMode = dark;
            repaint();
        }

        void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
        {
            auto area = getLocalBounds().toFloat().reduced(0.5f);
            auto fill = getToggleState()
                ? accent.withAlpha(isDarkMode ? 0.26f : 0.16f)
                : (isDarkMode ? juce::Colours::white.withAlpha(0.04f)
                              : GoodMeterLookAndFeel::bgPanel.withAlpha(0.92f));
            auto outline = getToggleState()
                ? accent.withAlpha(isDarkMode ? 0.95f : 0.88f)
                : (isDarkMode ? juce::Colours::white.withAlpha(0.14f)
                              : GoodMeterLookAndFeel::textMain.withAlpha(0.12f));

            if (isHovered)
                fill = fill.interpolatedWith(accent.withAlpha(isDarkMode ? 0.12f : 0.06f), 0.45f);
            if (isPressed)
                fill = fill.interpolatedWith(accent.withAlpha(isDarkMode ? 0.22f : 0.12f), 0.55f);

            g.setColour(fill);
            g.fillRoundedRectangle(area, 10.0f);
            g.setColour(outline);
            g.drawRoundedRectangle(area, 10.0f, getToggleState() ? 1.6f : 1.1f);

            const auto textColour = getToggleState()
                ? (isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.97f)
                              : GoodMeterLookAndFeel::ink.withAlpha(0.98f))
                : (isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.82f)
                              : GoodMeterLookAndFeel::textMain.withAlpha(0.80f));
            g.setColour(textColour);
            g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f, juce::Font::bold));
            g.drawText(getName(), getLocalBounds().reduced(8, 2), juce::Justification::centred, false);
        }

    private:
        juce::Colour accent;
        bool isDarkMode = false;
    };

    static const std::array<juce::String, 7>& getMarkerTagCatalog()
    {
        static const std::array<juce::String, 7> tags
        {
            "DIA", "FOLEY", "EFX", "SFX", "MUSIC", "AMB", "ADR"
        };
        return tags;
    }

    static juce::Colour getMarkerTagColour(const juce::String& tag)
    {
        if (tag == "DIA")   return juce::Colour(0xFFF58BA8);
        if (tag == "FOLEY") return juce::Colour(0xFFF7B955);
        if (tag == "EFX")   return juce::Colour(0xFF8FD3FF);
        if (tag == "SFX")   return juce::Colour(0xFF9B8CFF);
        if (tag == "MUSIC") return juce::Colour(0xFF72E0B5);
        if (tag == "AMB")   return juce::Colour(0xFF62D9E8);
        if (tag == "ADR")   return juce::Colour(0xFFFF9F7A);
        return GoodMeterLookAndFeel::accentBlue;
    }

    static juce::Font makeMarkerNoteFont(float size, int styleFlags = juce::Font::plain)
    {
        auto font = juce::Font("PingFang SC", size, styleFlags);
        if (font.getTypefaceName().containsIgnoreCase("PingFang"))
            return font;

        font = juce::Font("Hiragino Sans GB", size, styleFlags);
        if (font.getTypefaceName().containsIgnoreCase("Hiragino"))
            return font;

        return juce::Font(juce::Font::getDefaultSansSerifFontName(), size, styleFlags);
    }

    class MarkerNoteEditor : public juce::TextEditor
    {
    public:
        std::function<void(bool)> onFocusChanged;

        void focusGained(FocusChangeType cause) override
        {
            juce::TextEditor::focusGained(cause);
            if (onFocusChanged)
                onFocusChanged(true);
        }

        void focusLost(FocusChangeType cause) override
        {
            juce::TextEditor::focusLost(cause);
            if (onFocusChanged)
                onFocusChanged(false);
        }
    };

    HistoryMarkerRowComponent(const GoodMeterMarkerItem& markerToUse, const juce::String& timecodeToUse)
        : marker(markerToUse), timecode(timecodeToUse)
    {
        noteEditor.setMultiLine(true);
        noteEditor.setReturnKeyStartsNewLine(true);
        noteEditor.setScrollbarsShown(false);
        noteEditor.setPopupMenuEnabled(true);
        noteEditor.setClicksOutsideDismissVirtualKeyboard(true);
        noteEditor.setFont(makeMarkerNoteFont(18.5f));
        noteEditor.setTextToShowWhenEmpty("Write feedback...", GoodMeterLookAndFeel::textMuted.withAlpha(0.72f));
        noteEditor.onTextChange = [this]()
        {
            if (onNoteChanged)
                onNoteChanged(marker.id, noteEditor.getText());

            const int desiredHeight = getDesiredNoteHeight();
            if (desiredHeight != cachedNoteHeight)
            {
                cachedNoteHeight = desiredHeight;
                if (onLayoutChanged)
                    onLayoutChanged();
            }
        };
        noteEditor.onFocusChanged = [this](bool)
        {
            if (onEditorFocusChanged)
                onEditorFocusChanged(noteEditor.hasKeyboardFocus(true));
            repaint();
        };
        addAndMakeVisible(noteEditor);

        tagChooserButton.setButtonText("+");
        tagChooserButton.onClick = [this]()
        {
            tagPaletteVisible = !tagPaletteVisible;
            if (onLayoutChanged)
                onLayoutChanged();
            resized();
            repaint();
        };
        addAndMakeVisible(tagChooserButton);

        for (const auto& tag : getMarkerTagCatalog())
        {
            auto button = std::make_unique<MarkerTagChipButton>(tag, getMarkerTagColour(tag));
            button->onClick = [this, raw = button.get(), tag]()
            {
                if (raw->getToggleState())
                {
                    if (!marker.tags.contains(tag))
                        marker.tags.add(tag);
                }
                else
                {
                    marker.tags.removeString(tag);
                }

                if (onTagsChanged)
                    onTagsChanged(marker.id, marker.tags);

                if (onLayoutChanged)
                    onLayoutChanged();

                repaint();
            };
            addAndMakeVisible(button.get());
            tagButtons.push_back(std::move(button));
        }

        setDarkMode(false);
        setMarker(markerToUse, timecodeToUse);
    }

    void setDarkMode(bool dark)
    {
        isDarkMode = dark;
        const auto textColour = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.94f)
                                           : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
        const auto mutedColour = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.58f)
                                            : GoodMeterLookAndFeel::textMuted.withAlpha(0.86f);
        noteEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        noteEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        noteEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        noteEditor.setColour(juce::TextEditor::textColourId, textColour);
        noteEditor.setColour(juce::TextEditor::highlightColourId, marker.colour.withAlpha(isDarkMode ? 0.26f : 0.16f));
        noteEditor.setTextToShowWhenEmpty("Write feedback...", mutedColour);
        tagChooserButton.setColour(juce::TextButton::buttonColourId,
                                   (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::bgPanel).withAlpha(0.02f));
        tagChooserButton.setColour(juce::TextButton::buttonOnColourId, marker.colour.withAlpha(isDarkMode ? 0.18f : 0.10f));
        tagChooserButton.setColour(juce::TextButton::textColourOffId, textColour.withAlpha(0.88f));
        tagChooserButton.setColour(juce::TextButton::textColourOnId, textColour.withAlpha(0.98f));
        for (auto& button : tagButtons)
            button->setDarkMode(isDarkMode);
        repaint();
    }

    void setMarker(const GoodMeterMarkerItem& markerToUse, const juce::String& timecodeToUse)
    {
        marker = markerToUse;
        timecode = timecodeToUse;
        noteEditor.setText(marker.note, juce::dontSendNotification);
        syncTagButtonsFromMarker();
        if (!marker.frameImagePath.isEmpty())
            thumbnailImage = juce::ImageFileFormat::loadFrom(juce::File(marker.frameImagePath));
        else
            thumbnailImage = {};
        cachedNoteHeight = getDesiredNoteHeight();
        resized();
        repaint();
    }

    void setExpanded(bool shouldExpand)
    {
        expanded = shouldExpand;
        noteEditor.setVisible(expanded);
        cachedNoteHeight = getDesiredNoteHeight();
        resized();
        repaint();
    }

    bool isExpanded() const { return expanded; }
    bool isEditorFocused() const { return noteEditor.hasKeyboardFocus(true); }

    bool dismissEditorIfClickOutside(juce::Point<int> localPos)
    {
        if (!expanded || !isEditorFocused())
            return false;

        if (getEditorBounds().contains(localPos))
            return false;

        if (onNoteChanged)
            onNoteChanged(marker.id, noteEditor.getText());

        noteEditor.giveAwayKeyboardFocus();
        repaint();
        return true;
    }

    int getPreferredHeight() const
    {
        if (!expanded)
            return 44;

        if (marker.isVideo)
            return 42 + getDesiredThumbnailHeight() + 12 + getDesiredNoteHeight() + getTagSectionHeight() + 16;

        return 42 + getDesiredNoteHeight() + getTagSectionHeight() + 16;
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(2.0f, 0.0f);
        auto header = getHeaderBounds().toFloat();
        const auto textColour = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.94f)
                                           : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
        const auto guideColour = (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain)
            .withAlpha(isDarkMode ? 0.08f : 0.07f);

        g.setColour(guideColour);
        g.drawHorizontalLine((float) juce::roundToInt(area.getBottom() - 1.0f), area.getX(), area.getRight());

        const float lineY = header.getCentreY();
        const auto timeBounds = getTimeBounds().toFloat();
        const float gap = 14.0f;
        const float leftStart = area.getX() + 10.0f;
        const float rightEnd = area.getRight() - 10.0f;

        g.setColour(marker.colour.withAlpha(isDarkMode ? 0.94f : 0.88f));
        if (timeBounds.getX() - gap > leftStart)
            g.fillRect(juce::Rectangle<float>(leftStart, lineY - 1.0f, timeBounds.getX() - gap - leftStart, 2.0f));
        if (rightEnd > timeBounds.getRight() + gap)
            g.fillRect(juce::Rectangle<float>(timeBounds.getRight() + gap, lineY - 1.0f,
                                              rightEnd - (timeBounds.getRight() + gap), 2.0f));
        const auto timeGlow = isDarkMode ? juce::Colour(0xFFFFF8EE).withAlpha(0.10f)
                                         : GoodMeterLookAndFeel::ink.withAlpha(0.08f);
        GoodMeterDigitalTimecode::draw(g, timeBounds, timecode, textColour, timeGlow);

        if (!expanded)
            return;

        if (marker.isVideo)
        {
            auto thumbArea = getThumbnailBounds().toFloat();
            if (!thumbnailImage.isNull())
            {
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(getThumbnailBounds());
                auto imageToDraw = thumbnailImage;
                if (imageToDraw.getHeight() > imageToDraw.getWidth())
                    imageToDraw = rotateClockwise(imageToDraw);

                g.drawImageWithin(imageToDraw,
                                  getThumbnailBounds().getX(),
                                  getThumbnailBounds().getY(),
                                  getThumbnailBounds().getWidth(),
                                  getThumbnailBounds().getHeight(),
                                  juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
            }
            else
            {
                g.setColour((isDarkMode ? juce::Colour(0xFFF6EEE3) : GoodMeterLookAndFeel::textMain).withAlpha(0.48f));
                g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(11.5f, juce::Font::plain));
                g.drawText(marker.framePending ? "Capturing frame..." : "No frame preview",
                           thumbArea, juce::Justification::centred, false);
            }
        }

        auto editorBounds = getEditorBounds().toFloat();
        if (noteEditor.hasKeyboardFocus(true) && !editorBounds.isEmpty())
        {
            const auto focusColour = marker.colour.withAlpha(isDarkMode ? 0.95f : 0.88f);
            g.setColour(focusColour.withMultipliedAlpha(0.12f));
            g.fillRoundedRectangle(editorBounds.expanded(2.0f, 2.0f), 10.0f);
            g.setColour(focusColour);
            g.drawRoundedRectangle(editorBounds.expanded(2.0f, 2.0f), 10.0f, 1.6f);
        }

        if (expanded)
        {
            auto summaryBounds = getTagSummaryBounds().toFloat();
            const auto textColour = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.88f)
                                               : GoodMeterLookAndFeel::ink.withAlpha(0.92f);
            float cursorX = summaryBounds.getX() + 32.0f;
            const float centerY = summaryBounds.getCentreY();
            const float chipH = 22.0f;

            for (const auto& tag : marker.tags)
            {
                const auto chipWidth = GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f, juce::Font::bold).getStringWidthFloat(tag) + 22.0f;
                auto chip = juce::Rectangle<float>(chipWidth, chipH).withPosition(cursorX, centerY - chipH * 0.5f);
                const auto accent = getMarkerTagColour(tag);
                g.setColour(accent.withAlpha(isDarkMode ? 0.24f : 0.14f));
                g.fillRoundedRectangle(chip, 10.0f);
                g.setColour(accent.withAlpha(isDarkMode ? 0.94f : 0.88f));
                g.drawRoundedRectangle(chip, 10.0f, 1.2f);
                g.setColour(textColour);
                g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f, juce::Font::bold));
                g.drawText(tag, chip.toNearestInt().reduced(8, 1), juce::Justification::centred, false);
                cursorX += chipWidth + 8.0f;
                if (cursorX > summaryBounds.getRight() - 60.0f)
                    break;
            }
        }
    }

    void resized() override
    {
        noteEditor.setVisible(expanded);
        if (!expanded)
        {
            noteEditor.setBounds({});
            tagChooserButton.setBounds({});
            for (auto& button : tagButtons)
                button->setBounds({});
            return;
        }

        noteEditor.setBounds(getEditorBounds());
        tagChooserButton.setBounds(getTagChooserBounds());

        auto paletteBounds = getTagPaletteBounds();
        if (tagPaletteVisible)
        {
            layoutTagButtons(paletteBounds);
        }
        else
        {
            for (auto& button : tagButtons)
                button->setBounds({});
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dismissEditorIfClickOutside(e.getPosition());
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (getTimeBounds().contains(e.getPosition()))
        {
            if (onNoteChanged)
                onNoteChanged(marker.id, noteEditor.getText());

            expanded = !expanded;
            if (onExpandedChanged)
                onExpandedChanged(marker.id, expanded);

            noteEditor.setVisible(expanded);
            resized();
            repaint();
        }
    }

    std::function<void(const juce::String&, const juce::String&)> onNoteChanged;
    std::function<void(const juce::String&, const juce::StringArray&)> onTagsChanged;
    std::function<void(const juce::String&, bool)> onExpandedChanged;
    std::function<void()> onLayoutChanged;
    std::function<void(bool)> onEditorFocusChanged;

private:
    static juce::Image rotateClockwise(const juce::Image& source)
    {
        juce::Image rotated(source.getFormat(), source.getHeight(), source.getWidth(), true);
        juce::Graphics g(rotated);
        g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi)
                           .translated((float) source.getHeight(), 0.0f));
        g.drawImageAt(source, 0, 0);
        return rotated;
    }

    juce::Rectangle<int> getHeaderBounds() const
    {
        return getLocalBounds().reduced(12, 4).removeFromTop(38);
    }

    juce::Rectangle<int> getTimeBounds() const
    {
        auto header = getHeaderBounds();
        const float digitHeight = ((float) header.getHeight() - 2.0f) * 0.75f;
        const float wantedWidth = GoodMeterDigitalTimecode::preferredWidth(timecode, digitHeight);
        const int width = juce::jlimit(120, juce::jmax(120, header.getWidth() - 24), (int) std::ceil(wantedWidth + 8.0f));
        const int height = juce::jmax(20, (int) std::round(digitHeight));
        return header.withSizeKeepingCentre(width, height);
    }

    juce::Rectangle<int> getThumbnailBounds() const
    {
        auto area = getLocalBounds().reduced(16, 6);
        area.removeFromTop(42);
        const int maxHeight = juce::jmax(120, area.getHeight() - 48);
        const int targetHeight = getDesiredThumbnailHeight();
        const int imageHeight = juce::jlimit(120, maxHeight, targetHeight);
        return area.removeFromTop(imageHeight);
    }

    juce::Rectangle<int> getEditorBounds() const
    {
        auto area = getLocalBounds().reduced(16, 6);
        area.removeFromTop(42);
        if (marker.isVideo)
            area.removeFromTop(getThumbnailBounds().getHeight() + 12);
        area.setHeight(juce::jmax(44, getDesiredNoteHeight()));
        return area;
    }

    juce::Rectangle<int> getTagSummaryBounds() const
    {
        auto editor = getEditorBounds();
        return juce::Rectangle<int>(editor.getX(), editor.getBottom() + 8, editor.getWidth(), 26);
    }

    juce::Rectangle<int> getTagChooserBounds() const
    {
        return juce::Rectangle<int>(getTagSummaryBounds().getX(),
                                    getTagSummaryBounds().getY() - 1,
                                    24,
                                    24);
    }

    juce::Rectangle<int> getTagPaletteBounds() const
    {
        auto summary = getTagSummaryBounds();
        return juce::Rectangle<int>(summary.getX(),
                                    summary.getBottom() + 8,
                                    summary.getWidth(),
                                    getTagPaletteHeight());
    }

    int getContentWidthEstimate() const
    {
        const int currentWidth = getWidth() > 0 ? getWidth() : 360;
        return juce::jmax(180, currentWidth - 32);
    }

    int getDesiredThumbnailHeight() const
    {
        return (int) std::round((float) getContentWidthEstimate() * 9.0f / 16.0f);
    }

    int getDesiredNoteHeight() const
    {
        const auto text = noteEditor.getText().isEmpty() ? juce::String("Write feedback...") : noteEditor.getText();
        const float width = (float) juce::jmax(120, getContentWidthEstimate() - 4);

        juce::AttributedString attr;
        attr.setJustification(juce::Justification::topLeft);
        attr.append(text, noteEditor.getFont(), juce::Colours::white);

        juce::TextLayout layout;
        layout.createLayout(attr, width);

        const int target = (int) std::ceil(layout.getHeight()) + 20;
        return juce::jlimit(44, 220, target);
    }

    int getTagPaletteHeight() const
    {
        if (!tagPaletteVisible)
            return 0;

        const auto availableWidth = juce::jmax(180, getContentWidthEstimate());
        int rows = 1;
        float cursorX = 0.0f;
        for (const auto& tag : getMarkerTagCatalog())
        {
            const float chipWidth = GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f, juce::Font::bold).getStringWidthFloat(tag) + 22.0f;
            if (cursorX > 0.0f && cursorX + chipWidth > (float) availableWidth)
            {
                ++rows;
                cursorX = 0.0f;
            }
            cursorX += chipWidth + 8.0f;
        }
        return rows * 28 + (rows - 1) * 6;
    }

    int getTagSectionHeight() const
    {
        return 34 + (tagPaletteVisible ? getTagPaletteHeight() + 8 : 0);
    }

    void syncTagButtonsFromMarker()
    {
        for (auto& button : tagButtons)
            button->setToggleState(marker.tags.contains(button->getName()), juce::dontSendNotification);
    }

    void layoutTagButtons(juce::Rectangle<int> bounds)
    {
        int x = bounds.getX();
        int y = bounds.getY();
        const int h = 26;

        for (auto& button : tagButtons)
        {
            const int w = juce::roundToInt(GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f, juce::Font::bold)
                                           .getStringWidthFloat(button->getName()) + 22.0f);
            if (x > bounds.getX() && x + w > bounds.getRight())
            {
                x = bounds.getX();
                y += h + 6;
            }

            button->setBounds(x, y, w, h);
            x += w + 8;
        }
    }

    GoodMeterMarkerItem marker;
    juce::String timecode;
    bool isDarkMode = false;
    bool expanded = false;
    bool tagPaletteVisible = false;
    MarkerNoteEditor noteEditor;
    juce::TextButton tagChooserButton;
    std::vector<std::unique_ptr<MarkerTagChipButton>> tagButtons;
    juce::Image thumbnailImage;
    int cachedNoteHeight = 44;
};

class HistoryDeletePrompt : public juce::Component
{
public:
    HistoryDeletePrompt()
    {
        setInterceptsMouseClicks(true, true);

        titleLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        titleLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(18.0f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(titleLabel);
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        bodyLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        bodyLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(13.0f));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(bodyLabel);
        bodyLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(bodyLabel);

        cancelButton.setButtonText("Keep");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(cancelButton);
        cancelButton.onClick = [this]()
        {
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);

        confirmButton.setButtonText("Remove");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(confirmButton);
        confirmButton.onClick = [this]()
        {
            if (onConfirm)
                onConfirm();
        };
        addAndMakeVisible(confirmButton);

        nonoImage = loadPromptImage("nono_icon.PNG");
        guobaImage = loadPromptImage("guoba_prompt.PNG");

        if (guobaImage.isNull())
            guobaImage = juce::ImageCache::getFromMemory(BinaryData::guoba_png, BinaryData::guoba_pngSize);
    }

    void setMessage(const juce::String& title, const juce::String& body)
    {
        titleLabel.setText(title, juce::dontSendNotification);
        bodyLabel.setText(body, juce::dontSendNotification);
        resized();
        repaint();
    }

    void setCurrentSkin(int skinId)
    {
        currentSkinId = skinId;
        repaint();
    }

    std::function<void()> onCancel;
    std::function<void()> onConfirm;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.18f));

        auto card = cardBounds.toFloat();
        g.setColour(GoodMeterLookAndFeel::bgMain.withAlpha(0.98f));
        g.fillRoundedRectangle(card, 22.0f);

        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(card, 22.0f);
            g.reduceClipRegion(clip);

            auto artImage = currentSkinId == 2 ? guobaImage : nonoImage;
            if (!artImage.isNull())
            {
                auto artArea = card.toNearestInt().reduced(10, 10);
                artArea.removeFromLeft(juce::roundToInt(artArea.getWidth() * 0.28f));
                artArea.removeFromTop(8);
                artArea.removeFromBottom(20);

                g.setOpacity(0.115f);
                g.drawImageWithin(artImage,
                                  artArea.getX(), artArea.getY(),
                                  artArea.getWidth(), artArea.getHeight(),
                                  juce::RectanglePlacement(juce::RectanglePlacement::xRight
                                                           | juce::RectanglePlacement::yMid
                                                           | juce::RectanglePlacement::onlyReduceInSize));
                g.setOpacity(1.0f);
            }
        }

        g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.14f));
        g.drawRoundedRectangle(card, 22.0f, 1.4f);

        auto accentDot = card.removeFromTop(16.0f).removeFromLeft(16.0f).withSizeKeepingCentre(8.0f, 8.0f);
        auto accent = currentSkinId == 2 ? GoodMeterLookAndFeel::accentYellow
                                         : GoodMeterLookAndFeel::accentBlue;
        g.setColour(accent.withAlpha(0.9f));
        g.fillEllipse(accentDot);

        auto glow = accentDot.expanded(26.0f, 10.0f);
        juce::ColourGradient gradient(accent.withAlpha(0.10f),
                                      glow.getCentreX(), glow.getCentreY(),
                                      accent.withAlpha(0.0f),
                                      glow.getRight(), glow.getBottom(), true);
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(glow, 18.0f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        cardBounds = bounds.withSizeKeepingCentre(320, 206).reduced(8, 0);

        auto area = cardBounds.reduced(22, 20);
        area.removeFromTop(10);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        bodyLabel.setBounds(area.removeFromTop(56));
        area.removeFromTop(18);

        auto buttonRow = area.removeFromTop(42);
        const int gap = 10;
        const int buttonWidth = (buttonRow.getWidth() - gap) / 2;
        cancelButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(gap);
        confirmButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    }

private:
    static juce::Image loadPromptImage(const juce::String& fileName)
    {
        auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
        auto imageFile = appDir.getChildFile(fileName);

        if (imageFile.existsAsFile())
            return juce::ImageFileFormat::loadFrom(imageFile);

        return {};
    }

    int currentSkinId = 1;
    juce::Rectangle<int> cardBounds;
    juce::Label titleLabel;
    juce::Label bodyLabel;
    juce::TextButton cancelButton;
    juce::TextButton confirmButton;
    juce::Image nonoImage;
    juce::Image guobaImage;
};

class HistoryMarkerSavePrompt : public juce::Component
{
public:
    HistoryMarkerSavePrompt()
    {
        setInterceptsMouseClicks(true, true);

        titleLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        titleLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(18.0f, juce::Font::bold));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        bodyLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        bodyLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(13.0f));
        bodyLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(bodyLabel);

        nameEditor.setColour(juce::TextEditor::backgroundColourId, GoodMeterLookAndFeel::bgPanel);
        nameEditor.setColour(juce::TextEditor::outlineColourId, GoodMeterLookAndFeel::textMain.withAlpha(0.12f));
        nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, GoodMeterLookAndFeel::accentBlue.withAlpha(0.46f));
        nameEditor.setColour(juce::TextEditor::textColourId, GoodMeterLookAndFeel::textMain);
        nameEditor.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(15.0f, juce::Font::plain));
        addAndMakeVisible(nameEditor);

        cancelButton.setButtonText("Cancel");
        cancelButton.onClick = [this]()
        {
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);

        confirmButton.setButtonText("Save");
        confirmButton.onClick = [this]()
        {
            if (onConfirm)
                onConfirm(nameEditor.getText().trim());
        };
        addAndMakeVisible(confirmButton);
    }

    void setPrompt(const juce::String& title, const juce::String& body, const juce::String& initialName)
    {
        titleLabel.setText(title, juce::dontSendNotification);
        bodyLabel.setText(body, juce::dontSendNotification);
        nameEditor.setText(initialName, juce::dontSendNotification);
        resized();
        repaint();
    }

    void grabEditorFocus()
    {
        nameEditor.grabKeyboardFocus();
        nameEditor.selectAll();
    }

    std::function<void()> onCancel;
    std::function<void(const juce::String&)> onConfirm;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.18f));

        auto card = cardBounds.toFloat();
        g.setColour(GoodMeterLookAndFeel::bgMain.withAlpha(0.98f));
        g.fillRoundedRectangle(card, 22.0f);

        g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.14f));
        g.drawRoundedRectangle(card, 22.0f, 1.4f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        cardBounds = bounds.withSizeKeepingCentre(328, 228).reduced(10, 0);

        auto area = cardBounds.reduced(22, 20);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(8);
        bodyLabel.setBounds(area.removeFromTop(40));
        area.removeFromTop(12);
        nameEditor.setBounds(area.removeFromTop(40));
        area.removeFromTop(18);

        auto buttonRow = area.removeFromTop(42);
        const int gap = 10;
        const int buttonWidth = (buttonRow.getWidth() - gap) / 2;
        cancelButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(gap);
        confirmButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
    }

private:
    juce::Rectangle<int> cardBounds;
    juce::Label titleLabel;
    juce::Label bodyLabel;
    juce::TextEditor nameEditor;
    juce::TextButton cancelButton;
    juce::TextButton confirmButton;
};

class HistoryPageComponent : public juce::Component,
                             private juce::Timer
{
public:
    enum class FilterMode
    {
        marker = 0,
        audio = 1,
        video = 2
    };

    std::function<void(const juce::File&)> onFileRequested;
    std::function<void(const juce::File&)> onDeleteFileRequested;
    std::function<juce::String()> getMarkerCurrentFileName;
    std::function<juce::String()> getMarkerCurrentFilePath;
    std::function<juce::String()> getMarkerCurrentMetadataSummary;
    std::function<double()> getMarkerCurrentDurationSeconds;
    std::function<std::vector<GoodMeterMarkerItem>()> getCurrentMarkerItems;
    std::function<void(const juce::String&, const juce::String&)> updateMarkerNote;
    std::function<void(const juce::String&, const juce::StringArray&)> updateMarkerTags;
    std::function<juce::String(double)> formatMarkerTimecode;

    HistoryPageComponent()
    {
#if MARATHON_ART_STYLE
        bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
        randomizeBackground();
#endif

        viewport = std::make_unique<juce::Viewport>();
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(viewport.get());
        viewport->setViewedComponent(contentComponent.get(), false);
        viewport->setScrollBarsShown(false, false, true, false);

        markerButton.setRadioGroupId(4101);
        audioButton.setRadioGroupId(4101);
        videoButton.setRadioGroupId(4101);
        markerButton.setToggleState(true, juce::dontSendNotification);

        markerButton.onClick = [this]()
        {
            if (markerButton.getToggleState())
            {
                filterMode = FilterMode::marker;
                exitSelectionMode();
                refreshList();
            }
        };

        audioButton.onClick = [this]()
        {
            if (audioButton.getToggleState())
            {
                filterMode = FilterMode::audio;
                exitSelectionMode();
                refreshList();
            }
        };

        videoButton.onClick = [this]()
        {
            if (videoButton.getToggleState())
            {
                filterMode = FilterMode::video;
                exitSelectionMode();
                refreshList();
            }
        };

        addAndMakeVisible(markerButton);
        addAndMakeVisible(audioButton);
        addAndMakeVisible(videoButton);

        summaryLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        summaryLabel.setFont(HistoryRowComponent::makeHistoryReadableFont(19.0f, juce::Font::bold));
        summaryLabel.setJustificationType(juce::Justification::centredLeft);
        summaryLabel.setMinimumHorizontalScale(0.92f);
        addAndMakeVisible(summaryLabel);

        markerMetaLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted.withAlpha(0.95f));
        markerMetaLabel.setFont(HistoryRowComponent::makeHistoryReadableFont(17.0f));
        markerMetaLabel.setJustificationType(juce::Justification::centredLeft);
        markerMetaLabel.setMinimumHorizontalScale(0.90f);
        addAndMakeVisible(markerMetaLabel);

        emptyLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        emptyLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(14.0f));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(emptyLabel);
        emptyLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(emptyLabel);

        selectionSummaryLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        selectionSummaryLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(13.0f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(selectionSummaryLabel);
        selectionSummaryLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(selectionSummaryLabel);

        cancelSelectionButton.setButtonText("CANCEL");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(cancelSelectionButton);
        cancelSelectionButton.onClick = [this]() { exitSelectionMode(); };
        addAndMakeVisible(cancelSelectionButton);

        deleteSelectedButton.setButtonText("DELETE");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(deleteSelectedButton);
        deleteSelectedButton.onClick = [this]() { confirmDeleteSelection(); };
        addAndMakeVisible(deleteSelectedButton);

        deletePrompt = std::make_unique<HistoryDeletePrompt>();
        deletePrompt->setCurrentSkin(currentSkinId);
        deletePrompt->onCancel = [this]() { hideDeletePrompt(); };
        deletePrompt->onConfirm = [this]() { performDeleteSelection(); };
        addChildComponent(deletePrompt.get());

        markerSaveButton.onClick = [this]() { beginMarkerSave(false); };
        markerExportButton.onClick = [this]() { beginMarkerExport(); };
        addAndMakeVisible(markerSaveButton);
        addAndMakeVisible(markerExportButton);
        addAndMakeVisible(markerDrawerHandle);

        markerDrawerHandle.onDown = [this](const juce::MouseEvent& e)
        {
            if (filterMode != FilterMode::marker)
                return;
            markerDrawerDragging = true;
            markerDrawerMoved = false;
            dragStartDrawerReveal = markerDrawerReveal;
            dragStartDrawerY = e.getScreenPosition().y;
            markerDrawerTargetReveal = markerDrawerReveal;
        };
        markerDrawerHandle.onDrag = [this](const juce::MouseEvent& e)
        {
            if (!markerDrawerDragging || filterMode != FilterMode::marker)
                return;

            markerDrawerMoved = true;
            const float dragRange = (float) juce::jmax(1, getMarkerDrawerExpandedHeight() - getMarkerDrawerCollapsedHeight());
            const float deltaY = (float) (e.getScreenPosition().y - dragStartDrawerY);
            markerDrawerReveal = juce::jlimit(0.0f, 1.0f, dragStartDrawerReveal - deltaY / dragRange);
            resized();
            repaint();
        };
        markerDrawerHandle.onUp = [this](const juce::MouseEvent&)
        {
            if (!markerDrawerDragging || filterMode != FilterMode::marker)
                return;

            markerDrawerDragging = false;
            if (!markerDrawerMoved)
                markerDrawerTargetReveal = markerDrawerReveal < 0.5f ? 1.0f : 0.0f;
            else
                markerDrawerTargetReveal = markerDrawerReveal < 0.55f ? 0.0f : 1.0f;
            startTimerHz(60);
        };

        savePrompt = std::make_unique<HistoryMarkerSavePrompt>();
        savePrompt->onCancel = [this]() { hideMarkerSavePrompt(); };
        savePrompt->onConfirm = [this](const juce::String& enteredName)
        {
            const auto trimmed = enteredName.trim();
            if (trimmed.isNotEmpty())
            {
                currentMarkerReportName = trimmed;
                saveCurrentMarkerReport(trimmed);
                if (exportAfterPendingSave)
                    exportCurrentMarkerReport();
            }

            exportAfterPendingSave = false;
            hideMarkerSavePrompt();
        };
        addChildComponent(savePrompt.get());

        refreshList();
    }

    void setCurrentSkin(int skinId)
    {
        currentSkinId = skinId;

        if (deletePrompt != nullptr)
            deletePrompt->setCurrentSkin(currentSkinId);
    }

    void setDarkTheme(bool dark)
    {
        isDarkTheme = dark;

        auto textColor = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f) : GoodMeterLookAndFeel::textMain;
        auto mutedColor = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.72f) : GoodMeterLookAndFeel::textMuted;

        audioButton.setDarkMode(isDarkTheme);
        markerButton.setDarkMode(isDarkTheme);
        videoButton.setDarkMode(isDarkTheme);
        markerSaveButton.setDarkMode(isDarkTheme);
        markerExportButton.setDarkMode(isDarkTheme);
        summaryLabel.setColour(juce::Label::textColourId, mutedColor);
        markerMetaLabel.setColour(juce::Label::textColourId, mutedColor.withAlpha(0.95f));
        emptyLabel.setColour(juce::Label::textColourId, mutedColor);
        selectionSummaryLabel.setColour(juce::Label::textColourId, textColor);

        for (auto& row : markerRows)
            row->setDarkMode(isDarkTheme);

        repaint();
    }

    void setExportFeedbackWithMidi(bool enabled)
    {
        exportFeedbackWithMidi = enabled;
    }

    void paint(juce::Graphics& g) override
    {
        auto bgColor = isDarkTheme ? juce::Colours::black : GoodMeterLookAndFeel::bgMain;
        g.fillAll(bgColor);

#if MARATHON_ART_STYLE
        if (bgCanvas != nullptr)
        {
            juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::plain);
            int gridH = bgCanvas->getHeight();
            int gridW = bgCanvas->getWidth();
            auto bounds = getLocalBounds().toFloat();
            float cellW = bounds.getWidth() / gridW;
            float cellH = bounds.getHeight() / gridH;

            for (int y = 0; y < gridH; ++y)
            {
                for (int x = 0; x < gridW; ++x)
                {
                    auto cell = bgCanvas->getCell(x, y);
                    float px = x * cellW;
                    float py = y * cellH;

                    auto drawColour = isDarkTheme
                                          ? cell.color.withMultipliedAlpha(cell.brightness)
                                          : GoodMeterLookAndFeel::textMain.withAlpha(0.040f + cell.brightness * 0.105f);
                    g.setColour(drawColour);
                    g.setFont(monoFont);
                    juce::String str = juce::String::charToString(cell.symbol);
                    g.drawText(str, (int)px, (int)py, (int)cellW, (int)cellH,
                              juce::Justification::centred, false);
                }
            }
        }
#endif

        auto area = getLocalBounds().reduced(20, 0);
        auto sepColor = isDarkTheme ? juce::Colours::white.withAlpha(0.1f)
                                    : GoodMeterLookAndFeel::textMain.withAlpha(0.1f);
        g.setColour(sepColor);
        g.drawHorizontalLine(sectionTabsY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

        if (selectionMode)
        {
            g.drawHorizontalLine(selectionFooterY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
        }

        if (!markerDrawerPlateBounds.isEmpty() && markerDrawerReveal > 0.04f)
        {
            auto plate = markerDrawerPlateBounds.toFloat();
            const auto plateFill = isDarkTheme
                ? juce::Colour(0xFF0B1017).withAlpha(0.16f)
                : juce::Colour(0xFFFFFFFF).withAlpha(0.18f);
            const auto plateOutline = isDarkTheme
                ? juce::Colour(0xFFF6EEE3).withAlpha(0.08f)
                : juce::Colour(0xFF1A1A24).withAlpha(0.06f);
            g.setColour(plateFill);
            g.fillRoundedRectangle(plate, 18.0f);
            g.setColour(plateOutline);
            g.drawRoundedRectangle(plate.reduced(0.5f), 18.0f, 0.95f);
        }

        if (filterMode == FilterMode::marker && markerDrawerReveal < 0.98f)
        {
            const auto hookArea = getMarkerDrawerHookBounds().toFloat();
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

    void resized() override
    {
        const auto oldScrollY = getPreservedScrollY();
        auto area = getLocalBounds().reduced(20, 0);

        area.removeFromTop(52);

        sectionTabsY = area.getY();
        area.removeFromTop(8);
        auto buttonRow = area.removeFromTop(52);
        const int gap = 12;
        const int buttonWidth = (buttonRow.getWidth() - gap * 2) / 3;
        markerButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(gap);
        audioButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(gap);
        videoButton.setBounds(buttonRow.removeFromLeft(buttonWidth));

        area.removeFromTop(8);
        summaryLabel.setBounds(area.removeFromTop(filterMode == FilterMode::marker ? 26 : 24));

        if (filterMode == FilterMode::marker && markerMetaLabel.isVisible())
        {
            markerMetaLabel.setBounds(area.removeFromTop(24));
            area.removeFromTop(8);
        }
        else
        {
            markerMetaLabel.setBounds({});
            area.removeFromTop(10);
        }

        if (filterMode != FilterMode::marker)
        {
            markerSaveButton.setVisible(false);
            markerExportButton.setVisible(false);
            markerSaveButton.setBounds({});
            markerExportButton.setBounds({});
            markerDrawerHandle.setBounds({});
            markerDrawerPlateBounds = {};
        }

        if (selectionMode)
        {
            auto footerArea = area.removeFromBottom(62);
            selectionFooterY = footerArea.getY();
            footerArea.reduce(8, 8);

            deleteSelectedButton.setVisible(true);
            cancelSelectionButton.setVisible(true);
            selectionSummaryLabel.setVisible(true);
            deleteSelectedButton.setBounds(footerArea.removeFromRight(96));
            footerArea.removeFromRight(8);
            cancelSelectionButton.setBounds(footerArea.removeFromRight(96));
            footerArea.removeFromRight(10);
            selectionSummaryLabel.setBounds(footerArea);

            area.removeFromBottom(8);
        }
        else
        {
            selectionFooterY = 0;
            deleteSelectedButton.setVisible(false);
            cancelSelectionButton.setVisible(false);
            selectionSummaryLabel.setVisible(false);
        }

        viewport->setBounds(area);
        emptyLabel.setBounds(area.reduced(8, 24));
        layoutMarkerDrawer();
        layoutRows();
        restoreScrollY(oldScrollY);

        if (deletePrompt != nullptr)
            deletePrompt->setBounds(getLocalBounds());

        if (savePrompt != nullptr)
            savePrompt->setBounds(getLocalBounds());
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (filterMode != FilterMode::marker)
            return;

        const auto localPos = e.getPosition();
        bool dismissed = false;
        for (auto& row : markerRows)
        {
            if (row == nullptr)
                continue;

            dismissed = row->dismissEditorIfClickOutside(row->getLocalPoint(this, localPos)) || dismissed;
        }

        if (dismissed)
            repaint();
    }

    void refreshList()
    {
        const auto oldScrollY = getPreservedScrollY();
        auto oldSelection = selectedPaths;
        auto oldExpandedPath = expandedPath;

        items.clear();
        rows.clear();
        markerRows.clear();
        contentComponent->removeAllChildren();
        markerDrawerHandle.setVisible(filterMode == FilterMode::marker);

        if (filterMode == FilterMode::marker)
        {
            selectionMode = false;
            selectedPaths.clear();
            expandedPath.clear();

            const auto currentFileName = getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String();
            const auto currentMetadata = getMarkerCurrentMetadataSummary != nullptr ? getMarkerCurrentMetadataSummary() : juce::String();
            const auto markers = getCurrentMarkerItems != nullptr ? getCurrentMarkerItems() : std::vector<GoodMeterMarkerItem>{};
            currentMarkerReportName = loadSavedMarkerReportName(getMarkerCurrentFilePath != nullptr ? getMarkerCurrentFilePath() : juce::String());
            markerDrawerTargetReveal = markerDrawerReveal;

            summaryLabel.setText(currentFileName.isNotEmpty() ? currentFileName : "No active file",
                                 juce::dontSendNotification);
            markerMetaLabel.setText(currentMetadata, juce::dontSendNotification);
            markerMetaLabel.setVisible(currentMetadata.isNotEmpty());
            markerSaveButton.setEnabled(currentFileName.isNotEmpty() && !markers.empty());
            markerExportButton.setEnabled(currentFileName.isNotEmpty() && !markers.empty());
            emptyLabel.setText("No markers for current file yet", juce::dontSendNotification);
            emptyLabel.setVisible(markers.empty());

            for (const auto& marker : markers)
            {
                auto tc = formatMarkerTimecode != nullptr
                    ? formatMarkerTimecode(marker.seconds)
                    : juce::String::formatted("%0.2f", marker.seconds);
                auto row = std::make_unique<HistoryMarkerRowComponent>(marker, tc);
                row->setExpanded(expandedMarkerKey == marker.id);
                row->setDarkMode(isDarkTheme);
                row->onNoteChanged = [this](const juce::String& markerId, const juce::String& text)
                {
                    rememberMarkerScrollY();
                    if (updateMarkerNote != nullptr)
                        updateMarkerNote(markerId, text);
                };
                row->onTagsChanged = [this](const juce::String& markerId, const juce::StringArray& tags)
                {
                    rememberMarkerScrollY();
                    if (updateMarkerTags != nullptr)
                        updateMarkerTags(markerId, tags);
                };
                row->onExpandedChanged = [this](const juce::String& markerId, bool shouldExpand)
                {
                    rememberMarkerScrollY();
                    expandedMarkerKey = shouldExpand ? markerId : juce::String();
                    layoutRows();
                    restoreScrollY(markerScrollMemoryY);
                    repaint();
                };
                row->onLayoutChanged = [this]()
                {
                    rememberMarkerScrollY();
                    layoutRows();
                    restoreScrollY(markerScrollMemoryY);
                    repaint();
                };
                row->onEditorFocusChanged = [this](bool focused)
                {
                    if (focused)
                    {
                        rememberMarkerScrollY();
                        return;
                    }

                    markerScrollRestorePending = true;
                    scheduleMarkerScrollRestore();
                };
                contentComponent->addAndMakeVisible(row.get());
                markerRows.push_back(std::move(row));
            }

            updateSelectionFooter();
            layoutRows();
            restoreScrollY(oldScrollY);
            repaint();
            return;
        }

        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto allFiles = docsDir.findChildFiles(juce::File::findFiles, false, "*");

        for (const auto& file : allFiles)
        {
            if (!file.existsAsFile())
                continue;

            if (filterMode == FilterMode::audio && isAudioFile(file) && !isExtractedVideoAudioProxy(file))
                items.push_back(file);
            else if (filterMode == FilterMode::video && isVideoFile(file))
                items.push_back(file);
        }

        std::sort(items.begin(), items.end(),
                  [](const juce::File& a, const juce::File& b)
                  {
                      return a.getLastModificationTime() > b.getLastModificationTime();
                  });

        selectedPaths.clear();
        for (const auto& file : items)
        {
            auto path = file.getFullPathName();
            if (oldSelection.count(path) > 0)
                selectedPaths.insert(path);
        }

        if (selectedPaths.empty())
            selectionMode = false;

        expandedPath.clear();
        for (const auto& file : items)
        {
            if (file.getFullPathName() == oldExpandedPath)
            {
                expandedPath = oldExpandedPath;
                break;
            }
        }

        int64_t totalBytes = 0;
        for (const auto& file : items)
            totalBytes += file.getSize();

        auto kindText = (filterMode == FilterMode::audio) ? "audio" : "video";
        markerMetaLabel.setVisible(false);
        markerMetaLabel.setText({}, juce::dontSendNotification);
        auto countText = juce::String(items.size()) + " " + kindText + (items.size() == 1 ? " file" : " files");
        summaryLabel.setText(countText + "  "
                             + juce::String(juce::CharPointer_UTF8("\xE2\x80\xA2")) + "  "
                             + formatStorage(totalBytes),
                             juce::dontSendNotification);

        emptyLabel.setText(filterMode == FilterMode::audio
                               ? "No saved audio files yet"
                               : "No saved video files yet",
                           juce::dontSendNotification);
        emptyLabel.setVisible(items.empty());

        const auto accent = currentAccent();

        for (const auto& file : items)
        {
            auto row = std::make_unique<HistoryRowComponent>(file, accent);

            row->onLoadRequested = [this](const juce::File& selectedFile)
            {
                if (selectionMode)
                    return;

                if (onFileRequested)
                    onFileRequested(selectedFile);
            };

            row->onPyramidToggleRequested = [this](const juce::File& selectedFile, bool shouldOpen)
            {
                if (selectionMode)
                    return;

                expandedPath = shouldOpen ? selectedFile.getFullPathName() : juce::String();
                syncRows();
            };

            row->onLongPressRequested = [this](const juce::File& selectedFile)
            {
                selectionMode = true;
                expandedPath.clear();
                selectedPaths.insert(selectedFile.getFullPathName());
                syncRows();
                resized();
                repaint();
            };

            row->onSelectionToggleRequested = [this](const juce::File& selectedFile)
            {
                auto path = selectedFile.getFullPathName();
                if (selectedPaths.count(path) > 0)
                    selectedPaths.erase(path);
                else
                    selectedPaths.insert(path);

                if (selectedPaths.empty())
                    selectionMode = false;

                syncRows();
                resized();
                repaint();
            };

            contentComponent->addAndMakeVisible(row.get());
            rows.push_back(std::move(row));
        }

        for (auto& row : rows)
            row->setDarkMode(isDarkTheme);

        updateSelectionFooter();
        layoutRows();
        syncRows();
        restoreScrollY(oldScrollY);
        repaint();
    }

private:
    static bool hasExtension(const juce::File& file, std::initializer_list<const char*> exts)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        for (auto* candidate : exts)
        {
            if (ext == juce::String(candidate))
                return true;
        }

        return false;
    }

    static bool isAudioFile(const juce::File& file)
    {
        return hasExtension(file, { ".wav", ".mp3", ".aiff", ".aif", ".flac", ".ogg", ".m4a", ".caf" });
    }

    static bool isExtractedVideoAudioProxy(const juce::File& file)
    {
        return file.getFileName().startsWith("Extract_") && isAudioFile(file);
    }

    static bool isVideoFile(const juce::File& file)
    {
        return hasExtension(file, { ".mp4", ".mov", ".m4v", ".avi", ".mkv", ".mpg", ".mpeg", ".webm" });
    }

    static juce::String formatStorage(int64_t bytes)
    {
        constexpr double kb = 1024.0;
        constexpr double mb = kb * 1024.0;
        constexpr double gb = mb * 1024.0;

        if (bytes >= static_cast<int64_t>(gb))
            return juce::String(static_cast<double>(bytes) / gb, 2) + " GB";

        if (bytes >= static_cast<int64_t>(mb))
            return juce::String(static_cast<double>(bytes) / mb, 1) + " MB";

        if (bytes >= static_cast<int64_t>(kb))
            return juce::String(static_cast<double>(bytes) / kb, 1) + " KB";

        return juce::String(bytes) + " B";
    }

    juce::Colour currentAccent() const
    {
        if (filterMode == FilterMode::marker)
            return GoodMeterLookAndFeel::accentPink;

        return filterMode == FilterMode::audio
            ? GoodMeterLookAndFeel::accentBlue
            : GoodMeterLookAndFeel::accentYellow;
    }

    void exitSelectionMode()
    {
        if (!selectionMode && selectedPaths.empty())
            return;

        const auto oldScrollY = getScrollY();
        selectionMode = false;
        selectedPaths.clear();
        syncRows();
        resized();
        restoreScrollY(oldScrollY);
        repaint();
    }

    void updateSelectionFooter()
    {
        int64_t selectedBytes = 0;

        for (const auto& file : items)
        {
            if (selectedPaths.count(file.getFullPathName()) > 0)
                selectedBytes += file.getSize();
        }

        if (!selectionMode)
        {
            selectionSummaryLabel.setText({}, juce::dontSendNotification);
            return;
        }

        auto countText = juce::String(selectedPaths.size()) + " selected";
        selectionSummaryLabel.setText(countText + "  "
                                      + juce::String(juce::CharPointer_UTF8("\xE2\x80\xA2")) + "  "
                                      + formatStorage(selectedBytes),
                                      juce::dontSendNotification);
    }

    void syncRows()
    {
        updateSelectionFooter();

        for (auto& row : rows)
        {
            auto path = row->getFilePath();
            row->setSelectionMode(selectionMode);
            row->setSelected(selectedPaths.count(path) > 0);
            row->setActionOpen(!selectionMode && expandedPath == path, true);
        }
    }

    void confirmDeleteSelection()
    {
        if (selectedPaths.empty())
            return;

        std::vector<juce::File> filesToDelete;
        int64_t totalBytes = 0;

        for (const auto& file : items)
        {
            if (selectedPaths.count(file.getFullPathName()) > 0)
            {
                totalBytes += file.getSize();
                filesToDelete.push_back(file);
            }
        }

        if (filesToDelete.empty())
            return;

        pendingDeleteFiles = filesToDelete;

        const auto title = "Remove selected files from this device?";
        const auto message = "You are about to remove "
                             + juce::String(filesToDelete.size())
                             + " item"
                             + (filesToDelete.size() == 1 ? "" : "s")
                             + ". This will free about "
                             + formatStorage(totalBytes)
                             + ". You can import them again later if needed.";

        if (deletePrompt != nullptr)
        {
            deletePrompt->setMessage(title, message);
            deletePrompt->setVisible(true);
            deletePrompt->toFront(true);
        }
    }

    void layoutRows()
    {
        if (contentComponent == nullptr || viewport == nullptr)
            return;

        const int width = juce::jmax(1, viewport->getMaximumVisibleWidth());
        int y = (filterMode == FilterMode::marker) ? 0 : 6;
        const int rowHeight = 86;
        const int markerRowHeight = 72;
        const int gap = 8;

        if (filterMode == FilterMode::marker)
        {
            for (auto& row : markerRows)
            {
                const int h = row->getPreferredHeight();
                row->setBounds(0, y, width, h);
                y += h + gap;
            }

            contentComponent->setBounds(0, 0, width, juce::jmax(viewport->getMaximumVisibleHeight() + 1, y));
            return;
        }

        for (auto& row : rows)
        {
            row->setBounds(0, y, width, rowHeight);
            y += rowHeight + gap;
        }

        contentComponent->setBounds(0, 0, width, juce::jmax(viewport->getMaximumVisibleHeight() + 1, y));
    }

    int getScrollY() const
    {
        return viewport != nullptr ? viewport->getViewPositionY() : 0;
    }

    bool dismissFocusedMarkerEditors(juce::Point<int> localPos, bool dismissAll)
    {
        bool dismissed = false;
        for (auto& row : markerRows)
        {
            if (row == nullptr)
                continue;

            auto rowPoint = dismissAll ? juce::Point<int>(-100000, -100000)
                                       : row->getLocalPoint(this, localPos);
            dismissed = row->dismissEditorIfClickOutside(rowPoint) || dismissed;
        }

        if (dismissed)
            repaint();

        return dismissed;
    }

    int getPreservedScrollY() const
    {
        if (filterMode == FilterMode::marker && markerScrollRestorePending)
            return markerScrollMemoryY;

        return getScrollY();
    }

    void rememberMarkerScrollY()
    {
        if (filterMode == FilterMode::marker)
            markerScrollMemoryY = getScrollY();
    }

    void restoreScrollY(int y)
    {
        if (viewport == nullptr || contentComponent == nullptr)
            return;

        const int maxY = juce::jmax(0, contentComponent->getHeight() - viewport->getHeight());
        viewport->setViewPosition(0, juce::jlimit(0, maxY, y));
    }

    void scheduleMarkerScrollRestore()
    {
        if (filterMode != FilterMode::marker)
            return;

        const auto targetY = markerScrollMemoryY;
        auto safeThis = juce::Component::SafePointer<HistoryPageComponent>(this);
        juce::MessageManager::callAsync([safeThis, targetY]()
        {
            if (safeThis == nullptr)
                return;

            safeThis->restoreScrollY(targetY);
            safeThis->markerScrollRestorePending = false;
            safeThis->repaint();
        });
    }

    int getMarkerDrawerExpandedHeight() const { return 70; }
    int getMarkerDrawerCollapsedHeight() const { return 18; }

    juce::Rectangle<int> getMarkerDrawerHookBounds() const
    {
        const int width = 38;
        const int height = 14;
        return juce::Rectangle<int>(width, height)
            .withCentre({ getWidth() / 2, getHeight() - getMarkerDrawerCollapsedHeight() / 2 });
    }

    juce::Rectangle<int> getMarkerDrawerExpandedGestureBounds() const
    {
        if (markerDrawerPlateBounds.isEmpty())
            return {};

        const auto plate = markerDrawerPlateBounds;
        const int width = juce::jmin(160, plate.getWidth() - 20);
        const int height = 22;
        return juce::Rectangle<int>(width, height)
            .withCentre({ plate.getCentreX(), plate.getY() + height / 2 + 2 });
    }

    void updateMarkerDrawerChildPresentation()
    {
        const float alpha = juce::jlimit(0.0f, 1.0f, (markerDrawerReveal - 0.10f) / 0.90f);
        const bool interactive = alpha > 0.35f;
        for (juce::Component* component : { static_cast<juce::Component*>(&markerSaveButton),
                                            static_cast<juce::Component*>(&markerExportButton) })
        {
            component->setAlpha(alpha);
            component->setVisible(alpha > 0.02f && filterMode == FilterMode::marker);
            component->setEnabled(interactive);
        }
    }

    void layoutMarkerDrawer()
    {
        if (filterMode != FilterMode::marker)
            return;

        const int expandedHeight = getMarkerDrawerExpandedHeight();
        const int collapsedHeight = getMarkerDrawerCollapsedHeight();
        const int hiddenOffset = juce::roundToInt(((float) expandedHeight - (float) collapsedHeight) * (1.0f - markerDrawerReveal));
        auto fullArea = juce::Rectangle<int>(0, getHeight() - expandedHeight + hiddenOffset, getWidth(), expandedHeight);
        auto drawer = fullArea.reduced(16, 4);
        markerDrawerPlateBounds = drawer.reduced(4, 2);

        auto buttonRow = drawer.reduced(12, 10);
        auto left = buttonRow.removeFromLeft((buttonRow.getWidth() - 10) / 2);
        markerSaveButton.setBounds(left);
        buttonRow.removeFromLeft(10);
        markerExportButton.setBounds(buttonRow);

        updateMarkerDrawerChildPresentation();
        markerDrawerHandle.setVisible(true);
        markerDrawerHandle.setBounds(markerDrawerReveal < 0.45f ? getMarkerDrawerHookBounds()
                                                                : getMarkerDrawerExpandedGestureBounds());
        markerDrawerHandle.toFront(false);
        markerSaveButton.toFront(false);
        markerExportButton.toFront(false);
    }

    void timerCallback() override
    {
        if (!markerDrawerDragging && std::abs(markerDrawerReveal - markerDrawerTargetReveal) > 0.001f)
        {
            markerDrawerReveal += (markerDrawerTargetReveal - markerDrawerReveal) * 0.22f;
            if (std::abs(markerDrawerReveal - markerDrawerTargetReveal) < 0.01f)
                markerDrawerReveal = markerDrawerTargetReveal;
            resized();
            repaint();
            return;
        }

        stopTimer();
    }

    void hideDeletePrompt()
    {
        pendingDeleteFiles.clear();

        if (deletePrompt != nullptr)
            deletePrompt->setVisible(false);
    }

    void performDeleteSelection()
    {
        auto filesToDelete = pendingDeleteFiles;
        hideDeletePrompt();

        for (const auto& file : filesToDelete)
        {
            if (onDeleteFileRequested)
                onDeleteFileRequested(file);
        }

        selectionMode = false;
        selectedPaths.clear();
        expandedPath.clear();
        refreshList();
    }

    static juce::String sanitiseReportName(juce::String text)
    {
        text = text.trim();
        if (text.isEmpty())
            text = "Marker_Feedback";

        juce::String out;
        for (auto c : text)
        {
            if (juce::CharacterFunctions::isLetterOrDigit(c)
                || c == '-' || c == '_' || c == ' ' || c == 0x4F8B || c == 0x53CD || c == 0x9988)
                out << c;
            else if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                out << '_';
            else
                out << c;
        }

        return out.replaceCharacters("/", "_").replaceCharacters(":", "_").trim();
    }

    static juce::String makeDefaultMarkerReportName(const juce::String& currentFileName)
    {
        auto base = currentFileName;
        const int dot = base.lastIndexOfChar('.');
        if (dot > 0)
            base = base.substring(0, dot);
        return sanitiseReportName(base + "_反馈");
    }

    juce::File getMarkerReportsDirectory() const
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("MarkerReports");
        dir.createDirectory();
        return dir;
    }

    juce::File getMarkerDraftFile(const juce::String& filePath) const
    {
        const auto hash = juce::String::toHexString((juce::int64) filePath.hashCode64());
        return getMarkerReportsDirectory().getChildFile("marker_" + hash + ".json");
    }

    juce::File getMarkerReportBundleDirectory(const juce::String& reportName) const
    {
        return getMarkerReportsDirectory().getChildFile(sanitiseReportName(reportName));
    }

    static juce::String compactMarkerMidiText(juce::String text)
    {
        text = text.replaceCharacters("\r\n\t", "   ");
        while (text.contains("  "))
            text = text.replace("  ", " ");
        return text.trim();
    }

    juce::String buildMarkerMidiLabel(const GoodMeterMarkerItem& marker) const
    {
        const auto timecode = formatMarkerTimecode != nullptr
            ? formatMarkerTimecode(marker.seconds)
            : juce::String::formatted("%0.2f", marker.seconds);

        juce::StringArray labelParts;
        for (const auto& tag : marker.tags)
        {
            const auto trimmedTag = compactMarkerMidiText(tag).toUpperCase();
            if (trimmedTag.isNotEmpty())
                labelParts.add("[" + trimmedTag + "]");
        }

        auto note = compactMarkerMidiText(marker.note);
        auto label = timecode;
        if (!labelParts.isEmpty())
            label << " " << labelParts.joinIntoString("");
        if (note.isNotEmpty())
            label << "  " << note;

        return label.substring(0, 120);
    }

    bool writeMarkerReportMidiFile(const juce::File& targetFile) const
    {
        const auto markers = getCurrentMarkerItems != nullptr ? getCurrentMarkerItems() : std::vector<GoodMeterMarkerItem>{};
        if (markers.empty())
            return false;

        juce::MidiMessageSequence metaTrack;

        auto trackName = juce::MidiMessage::textMetaEvent(3, "GOODMETER MARKERS");
        trackName.setTimeStamp(0.0);
        metaTrack.addEvent(trackName);

        auto tempo = juce::MidiMessage::tempoMetaEvent(1000000); // 60 BPM -> 1 quarter note = 1 second
        tempo.setTimeStamp(0.0);
        metaTrack.addEvent(tempo);

        auto timeSignature = juce::MidiMessage::timeSignatureMetaEvent(4, 4);
        timeSignature.setTimeStamp(0.0);
        metaTrack.addEvent(timeSignature);

        auto introText = juce::MidiMessage::textMetaEvent(1, "GOODMETER marker export");
        introText.setTimeStamp(0.0);
        metaTrack.addEvent(introText);

        constexpr int ticksPerQuarter = 960;
        constexpr double ticksPerSecond = (double) ticksPerQuarter;
        double sourceDurationSeconds = getMarkerCurrentDurationSeconds != nullptr ? getMarkerCurrentDurationSeconds() : 0.0;
        if (!std::isfinite(sourceDurationSeconds) || sourceDurationSeconds <= 0.0)
            sourceDurationSeconds = markers.back().seconds;

        for (const auto& marker : markers)
        {
            const auto tick = std::round(juce::jmax(0.0, marker.seconds) * ticksPerSecond);
            const auto label = buildMarkerMidiLabel(marker);

            auto markerEvent = juce::MidiMessage::textMetaEvent(0x06, label);
            markerEvent.setTimeStamp(tick);
            metaTrack.addEvent(markerEvent);

            auto cueEvent = juce::MidiMessage::textMetaEvent(0x07, label);
            cueEvent.setTimeStamp(tick);
            metaTrack.addEvent(cueEvent);
        }

        auto end = juce::MidiMessage::endOfTrack();
        const auto endTick = std::round(juce::jmax(0.0, sourceDurationSeconds) * ticksPerSecond) + 1.0;
        end.setTimeStamp(endTick);
        metaTrack.addEvent(end);
        metaTrack.updateMatchedPairs();

        juce::MidiFile midi;
        midi.setTicksPerQuarterNote(ticksPerQuarter);
        midi.addTrack(metaTrack);

        targetFile.getParentDirectory().createDirectory();
        targetFile.deleteFile();
        if (auto output = targetFile.createOutputStream())
            return midi.writeTo(*output, 1);

        return false;
    }

    bool writeMarkerReportZipArchive(const juce::File& zipTarget,
                                     const juce::File& imageFile,
                                     const juce::File& midiFile) const
    {
        juce::ZipFile::Builder builder;

        if (imageFile.existsAsFile())
            builder.addFile(imageFile, 9, imageFile.getFileName());

        if (midiFile.existsAsFile())
            builder.addFile(midiFile, 9, midiFile.getFileName());

        zipTarget.deleteFile();
        if (auto output = zipTarget.createOutputStream())
            return builder.writeToStream(*output, nullptr);

        return false;
    }

    juce::String loadSavedMarkerReportName(const juce::String& filePath) const
    {
        if (filePath.isEmpty())
            return {};

        const auto draft = getMarkerDraftFile(filePath);
        if (!draft.existsAsFile())
            return {};

        const auto parsed = juce::JSON::parse(draft.loadFileAsString());
        if (auto* obj = parsed.getDynamicObject())
            return obj->getProperty("reportName").toString();

        return {};
    }

    bool saveCurrentMarkerReport(const juce::String& reportName)
    {
        const auto filePath = getMarkerCurrentFilePath != nullptr ? getMarkerCurrentFilePath() : juce::String();
        if (filePath.isEmpty() || getCurrentMarkerItems == nullptr)
            return false;

        auto markers = getCurrentMarkerItems();
        if (markers.empty())
            return false;

        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("reportName", reportName);
        root->setProperty("sourceFilePath", filePath);
        root->setProperty("sourceFileName",
                          getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String());
        root->setProperty("sourceTechnicalSummary",
                          getMarkerCurrentMetadataSummary != nullptr ? getMarkerCurrentMetadataSummary() : juce::String());
        root->setProperty("savedAt", juce::Time::getCurrentTime().toISO8601(true));

        juce::Array<juce::var> markerArray;
        for (const auto& marker : markers)
        {
            auto entry = std::make_unique<juce::DynamicObject>();
            entry->setProperty("id", marker.id);
            entry->setProperty("seconds", marker.seconds);
            entry->setProperty("timecode",
                               formatMarkerTimecode != nullptr
                                   ? formatMarkerTimecode(marker.seconds)
                                   : juce::String::formatted("%0.2f", marker.seconds));
            entry->setProperty("colour", marker.colour.toString());
            entry->setProperty("note", marker.note);
            entry->setProperty("frameImagePath", marker.frameImagePath);
            entry->setProperty("framePending", marker.framePending);
            entry->setProperty("isVideo", marker.isVideo);
            juce::Array<juce::var> tagArray;
            for (const auto& tag : marker.tags)
                tagArray.add(tag);
            entry->setProperty("tags", tagArray);
            markerArray.add(juce::var(entry.release()));
        }

        root->setProperty("markers", markerArray);
        const auto target = getMarkerDraftFile(filePath);
        target.getParentDirectory().createDirectory();
        currentMarkerReportName = reportName;
        return target.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
    }

    void showMarkerSavePrompt(bool exportAfterSave)
    {
        exportAfterPendingSave = exportAfterSave;
        if (savePrompt == nullptr)
            return;

        const auto fileName = getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String();
        const auto suggestedName = currentMarkerReportName.isNotEmpty()
            ? currentMarkerReportName
            : makeDefaultMarkerReportName(fileName);

        savePrompt->setPrompt("Save marker feedback",
                              "Give this feedback sheet a name. The first save will remember it, and later saves will overwrite this draft.",
                              suggestedName);
        savePrompt->setVisible(true);
        savePrompt->toFront(true);
        savePrompt->grabEditorFocus();
    }

    void hideMarkerSavePrompt()
    {
        if (savePrompt != nullptr)
            savePrompt->setVisible(false);
    }

    void beginMarkerSave(bool exportAfterSave)
    {
        const auto currentFileName = getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String();
        if (currentFileName.isEmpty() || getCurrentMarkerItems == nullptr || getCurrentMarkerItems().empty())
            return;

        if (currentMarkerReportName.isEmpty())
        {
            showMarkerSavePrompt(exportAfterSave);
            return;
        }

        saveCurrentMarkerReport(currentMarkerReportName);
        if (exportAfterSave)
            exportCurrentMarkerReport();
    }

    void beginMarkerExport()
    {
        beginMarkerSave(true);
    }

    juce::Image renderCurrentMarkerReportImage(const juce::String& reportName) const
    {
        juce::ignoreUnused(reportName);
        const auto fileName = getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String();
        const auto technicalSummary = getMarkerCurrentMetadataSummary != nullptr ? getMarkerCurrentMetadataSummary() : juce::String();
        const auto markers = getCurrentMarkerItems != nullptr ? getCurrentMarkerItems() : std::vector<GoodMeterMarkerItem>{};

        constexpr int exportWidth = 1242;
        constexpr int margin = 54;
        constexpr int sectionGap = 22;
        constexpr int contentWidth = exportWidth - margin * 2;

        auto rotateClockwise = [](const juce::Image& source) -> juce::Image
        {
            juce::Image rotated(source.getFormat(), source.getHeight(), source.getWidth(), true);
            juce::Graphics g(rotated);
            g.addTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::halfPi)
                               .translated((float) source.getHeight(), 0.0f));
            g.drawImageAt(source, 0, 0);
            return rotated;
        };

        auto noteHeightFor = [](const juce::String& note, float width) -> int
        {
            const auto text = note.isEmpty() ? juce::String("No feedback added.") : note;
            juce::AttributedString attr;
            attr.setJustification(juce::Justification::topLeft);
            attr.append(text, juce::Font(22.0f), juce::Colours::white);
            juce::TextLayout layout;
            layout.createLayout(attr, width);
            return juce::jmax(44, (int) std::ceil(layout.getHeight()) + 8);
        };

        auto tagRowsFor = [](const juce::StringArray& tags, float width) -> int
        {
            if (tags.isEmpty())
                return 0;

            int rows = 1;
            float cursorX = 0.0f;
            for (const auto& tag : tags)
            {
                const auto chipWidth = GoodMeterLookAndFeel::iosEnglishMonoFont(18.0f, juce::Font::bold).getStringWidthFloat(tag) + 34.0f;
                if (cursorX > 0.0f && cursorX + chipWidth > width)
                {
                    ++rows;
                    cursorX = 0.0f;
                }
                cursorX += chipWidth + 12.0f;
            }
            return rows;
        };

        auto tagHeightFor = [&](const juce::StringArray& tags, float width) -> int
        {
            const int rows = tagRowsFor(tags, width);
            if (rows == 0)
                return 0;
            return rows * 34 + (rows - 1) * 10;
        };

        int totalHeight = 84 + 44 + 22;
        if (technicalSummary.isNotEmpty())
            totalHeight += 32;
        totalHeight += 78;
        for (const auto& marker : markers)
        {
            totalHeight += 54;
            if (marker.isVideo && !marker.frameImagePath.isEmpty())
                totalHeight += (int) std::round((float) contentWidth * 9.0f / 16.0f) + 16;
            totalHeight += tagHeightFor(marker.tags, (float) contentWidth);
            if (!marker.tags.isEmpty())
                totalHeight += 14;
            totalHeight += noteHeightFor(marker.note, (float) contentWidth) + sectionGap;
        }
        totalHeight += 42;

        juce::Image image(juce::Image::ARGB, exportWidth, juce::jmax(800, totalHeight), true);
        juce::Graphics g(image);
        const auto bg = isDarkTheme ? juce::Colour(0xFF090909) : juce::Colours::white;
        const auto ink = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                                     : GoodMeterLookAndFeel::ink.withAlpha(0.98f);
        const auto muted = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.60f)
                                       : GoodMeterLookAndFeel::textMuted;
        g.fillAll(bg);

        int y = 56;
        g.setColour(ink);
        g.setFont(juce::Font(26.0f, juce::Font::bold));
        g.drawText(fileName, margin, y, contentWidth, 32, juce::Justification::centredLeft, true);
        y += 40;

        if (technicalSummary.isNotEmpty())
        {
            g.setColour(muted.withAlpha(0.96f));
            g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(18.0f));
            g.drawText(technicalSummary, margin, y, contentWidth, 22, juce::Justification::centredLeft, true);
            y += 28;
        }

        g.setColour(muted.withAlpha(0.45f));
        g.drawHorizontalLine((float) y, (float) margin, (float) (margin + contentWidth));
        y += 28;

        for (const auto& marker : markers)
        {
            const auto timecode = formatMarkerTimecode != nullptr
                ? formatMarkerTimecode(marker.seconds)
                : juce::String::formatted("%0.2f", marker.seconds);
            const auto lineY = (float) y + 26.0f;
            const auto digitHeight = 36.0f;
            const auto timeWidth = GoodMeterDigitalTimecode::preferredWidth(timecode, digitHeight) + 10.0f;
            juce::Rectangle<float> timeBounds((float) margin + ((float) contentWidth - timeWidth) * 0.5f,
                                              (float) y + 4.0f,
                                              timeWidth,
                                              digitHeight);

            g.setColour(marker.colour.withAlpha(isDarkTheme ? 0.92f : 0.88f));
            const float lineGap = 18.0f;
            const float leftStart = (float) margin;
            const float rightEnd = (float) (margin + contentWidth);
            if (timeBounds.getX() - lineGap > leftStart)
                g.fillRect(juce::Rectangle<float>(leftStart, lineY - 1.5f, timeBounds.getX() - lineGap - leftStart, 3.0f));
            if (rightEnd > timeBounds.getRight() + lineGap)
                g.fillRect(juce::Rectangle<float>(timeBounds.getRight() + lineGap, lineY - 1.5f,
                                                  rightEnd - (timeBounds.getRight() + lineGap), 3.0f));

            GoodMeterDigitalTimecode::draw(g, timeBounds, timecode, ink, ink.withAlpha(0.08f));
            y += 54;

            if (marker.isVideo && !marker.frameImagePath.isEmpty())
            {
                auto frameImage = juce::ImageFileFormat::loadFrom(juce::File(marker.frameImagePath));
                if (!frameImage.isNull())
                {
                    if (frameImage.getHeight() > frameImage.getWidth())
                        frameImage = rotateClockwise(frameImage);

                    const int frameHeight = (int) std::round((float) contentWidth * 9.0f / 16.0f);
                    g.drawImageWithin(frameImage, margin, y, contentWidth, frameHeight,
                                      juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
                    y += frameHeight + 16;
                }
            }

            if (!marker.tags.isEmpty())
            {
                float chipX = (float) margin;
                float chipY = (float) y;
                const float chipH = 34.0f;
                const auto tagFont = GoodMeterLookAndFeel::iosEnglishMonoFont(18.0f, juce::Font::bold);
                for (const auto& tag : marker.tags)
                {
                    const float chipW = tagFont.getStringWidthFloat(tag) + 34.0f;
                    if (chipX > (float) margin && chipX + chipW > (float) (margin + contentWidth))
                    {
                        chipX = (float) margin;
                        chipY += chipH + 10.0f;
                    }

                    auto chip = juce::Rectangle<float>(chipW, chipH).withPosition(chipX, chipY);
                    const auto accent = HistoryMarkerRowComponent::getMarkerTagColour(tag);
                    g.setColour(accent.withAlpha(isDarkTheme ? 0.24f : 0.14f));
                    g.fillRoundedRectangle(chip, 14.0f);
                    g.setColour(accent.withAlpha(isDarkTheme ? 0.94f : 0.88f));
                    g.drawRoundedRectangle(chip, 14.0f, 1.6f);
                    g.setColour(ink);
                    g.setFont(tagFont);
                    g.drawText(tag, chip.toNearestInt().reduced(12, 2), juce::Justification::centred, false);
                    chipX += chipW + 12.0f;
                }

                y = (int) std::ceil(chipY + chipH + 14.0f);
            }

            const auto note = marker.note.isEmpty() ? juce::String("No feedback added.") : marker.note;
            juce::AttributedString attr;
            attr.setJustification(juce::Justification::topLeft);
            attr.append(note, juce::Font(22.0f), ink);
            juce::TextLayout layout;
            layout.createLayout(attr, (float) contentWidth);
            layout.draw(g, juce::Rectangle<float>((float) margin, (float) y, (float) contentWidth, (float) noteHeightFor(note, (float) contentWidth)));
            y += noteHeightFor(note, (float) contentWidth) + sectionGap;
        }

        return image;
    }

    void exportCurrentMarkerReport()
    {
        const auto fileName = getMarkerCurrentFileName != nullptr ? getMarkerCurrentFileName() : juce::String();
        if (fileName.isEmpty() || getCurrentMarkerItems == nullptr || getCurrentMarkerItems().empty())
            return;

        const auto reportName = currentMarkerReportName.isNotEmpty()
            ? currentMarkerReportName
            : makeDefaultMarkerReportName(fileName);
        const auto image = renderCurrentMarkerReportImage(reportName);
        const auto safeBaseName = sanitiseReportName(reportName);
        juce::PNGImageFormat png;

        if (exportFeedbackWithMidi)
        {
            auto bundleDir = getMarkerReportBundleDirectory(reportName);
            if (bundleDir.exists())
                bundleDir.deleteRecursively();
            bundleDir.createDirectory();

            auto imageTarget = bundleDir.getChildFile(safeBaseName + ".png");
            bool wroteImage = false;
            if (auto output = imageTarget.createOutputStream())
            {
                png.writeImageToStream(image, *output);
                output->flush();
                wroteImage = true;
            }

            auto midiTarget = bundleDir.getChildFile(safeBaseName + ".mid");
            const bool wroteMidi = writeMarkerReportMidiFile(midiTarget);
            auto zipTarget = getMarkerReportsDirectory().getChildFile(safeBaseName + ".zip");

            if (wroteImage && writeMarkerReportZipArchive(zipTarget, imageTarget, wroteMidi ? midiTarget : juce::File()))
            {
                GoodMeterIOSShareHelpers::shareFile(zipTarget);
                return;
            }

            GoodMeterIOSShareHelpers::shareFile(wroteImage ? imageTarget : bundleDir);
            return;
        }

        auto target = getMarkerReportsDirectory().getChildFile(safeBaseName + ".png");
        target.deleteFile();

        if (auto output = target.createOutputStream())
        {
            png.writeImageToStream(image, *output);
            output->flush();
            GoodMeterIOSShareHelpers::shareFile(target);
        }
    }

    FilterMode filterMode = FilterMode::marker;
    int currentSkinId = 1;
    bool selectionMode = false;
    bool isDarkTheme = false;
    int sectionTabsY = 0;
    int selectionFooterY = 0;
    juce::String expandedPath;
    std::set<juce::String> selectedPaths;
    std::vector<juce::File> pendingDeleteFiles;
    juce::String expandedMarkerKey;

#if MARATHON_ART_STYLE
    std::unique_ptr<DotMatrixCanvas> bgCanvas;
#endif

    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<HistoryDeletePrompt> deletePrompt;
    std::unique_ptr<HistoryMarkerSavePrompt> savePrompt;
    std::vector<juce::File> items;
    std::vector<std::unique_ptr<HistoryRowComponent>> rows;
    std::vector<std::unique_ptr<HistoryMarkerRowComponent>> markerRows;

    HistorySegmentButton markerButton { "Marker", GoodMeterLookAndFeel::accentPink };
    HistorySegmentButton audioButton { "Audio", GoodMeterLookAndFeel::accentBlue };
    HistorySegmentButton videoButton { "Video", GoodMeterLookAndFeel::accentYellow };
    HistoryActionButton markerSaveButton { "SAVE", GoodMeterLookAndFeel::accentBlue };
    HistoryActionButton markerExportButton { "EXPORT", GoodMeterLookAndFeel::accentYellow };
    HistoryDrawerHandle markerDrawerHandle;
    juce::Label summaryLabel;
    juce::Label markerMetaLabel;
    juce::Label emptyLabel;
    juce::Label selectionSummaryLabel;
    juce::TextButton cancelSelectionButton;
    juce::TextButton deleteSelectedButton;
    juce::String currentMarkerReportName;
    bool exportFeedbackWithMidi = false;
    bool exportAfterPendingSave = false;
    juce::Rectangle<int> markerDrawerPlateBounds;
    float markerDrawerReveal = 0.0f;
    float markerDrawerTargetReveal = 0.0f;
    bool markerDrawerDragging = false;
    bool markerDrawerMoved = false;
    float dragStartDrawerReveal = 0.0f;
    int dragStartDrawerY = 0;
    int markerScrollMemoryY = 0;
    bool markerScrollRestorePending = false;

#if MARATHON_ART_STYLE
    void randomizeBackground()
    {
        static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
        juce::Random rng;
        const auto preset = MarathonField::Preset::history;

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

                int idx = rng.nextInt(10);
                char32_t sym = symbols[idx];

                if (sym == lastSymbol)
                {
                    consecutiveCount++;
                    if (consecutiveCount >= 3)
                    {
                        do {
                            idx = rng.nextInt(9);
                            sym = symbols[idx];
                        } while (sym == lastSymbol);
                        consecutiveCount = 0;
                    }
                }
                else
                {
                    consecutiveCount = 0;
                }

                if (x % 7 == 0 && (sym == U'.' || sym == U'·'))
                    sym = U'◯';
                else if (y % 6 == 0 && (sym == U'.' || sym == U'·'))
                    sym = U'✕';

                lastSymbol = sym;
                float brightness = MarathonField::brightnessForCell(x, y, bgCanvas->getWidth(), bgCanvas->getHeight(), preset);
                bgCanvas->setCell(x, y, sym, juce::Colours::white, 0, brightness);
            }
        }
    }
#endif
};
