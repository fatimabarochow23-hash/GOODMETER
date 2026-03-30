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
#include <set>
#include "../../JuceLibraryCode/BinaryData.h"
#include "../GoodMeterLookAndFeel.h"

class HistorySegmentButton : public juce::Button
{
public:
    HistorySegmentButton(const juce::String& nameToUse, juce::Colour accentToUse)
        : juce::Button(nameToUse), accent(accentToUse)
    {
        setClickingTogglesState(true);
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.0f);

        auto fill = GoodMeterLookAndFeel::bgMain;
        auto outline = getToggleState() ? accent : GoodMeterLookAndFeel::textMain.withAlpha(0.16f);

        if (getToggleState())
            fill = accent.withAlpha(0.08f);
        else if (isHovered)
            fill = GoodMeterLookAndFeel::textMain.withAlpha(0.03f);

        if (isPressed)
            fill = accent.withAlpha(0.14f);

        g.setColour(fill);
        g.fillRoundedRectangle(area, 14.0f);

        g.setColour(outline);
        g.drawRoundedRectangle(area, 14.0f, getToggleState() ? 2.5f : 1.2f);

        auto dotArea = area.removeFromTop(20.0f).removeFromLeft(20.0f).reduced(6.0f);
        g.setColour(accent.withAlpha(getToggleState() ? 1.0f : 0.35f));
        g.fillEllipse(dotArea);

        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(juce::FontOptions(15.0f)));
        g.drawText(getName(), getLocalBounds().reduced(12, 8), juce::Justification::centred, false);
    }

private:
    juce::Colour accent;
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

class HistoryRowComponent : public juce::Component,
                            private juce::Timer
{
public:
    HistoryRowComponent(const juce::File& fileToUse, juce::Colour accentToUse)
        : file(fileToUse), accent(accentToUse), pyramidButton(accentToUse)
    {
        setInterceptsMouseClicks(true, true);

        nameLabel.setText(file.getFileName(), juce::dontSendNotification);
        nameLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        nameLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        nameLabel.setJustificationType(juce::Justification::centredLeft);
        nameLabel.setMinimumHorizontalScale(0.72f);
        addAndMakeVisible(nameLabel);

        metaLabel.setText(buildMetaText(file), juce::dontSendNotification);
        metaLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        metaLabel.setFont(juce::Font(juce::FontOptions(10.5f)));
        metaLabel.setJustificationType(juce::Justification::centredLeft);
        metaLabel.setMinimumHorizontalScale(0.72f);
        addAndMakeVisible(metaLabel);

        loadButton.setButtonText("LOAD");
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

        auto fill = GoodMeterLookAndFeel::textMain.withAlpha(0.035f);
        auto outline = GoodMeterLookAndFeel::textMain.withAlpha(0.14f);

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
            g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.88f));
            g.drawEllipse(circle, 1.8f);

            if (isSelected)
            {
                g.setColour(accent);
                g.fillEllipse(circle.reduced(2.8f));
                g.setColour(GoodMeterLookAndFeel::bgMain.withAlpha(0.95f));
                g.fillEllipse(circle.withSizeKeepingCentre(circle.getWidth() * 0.26f,
                                                           circle.getHeight() * 0.26f));
            }
        }
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

            pyramidButton.setBounds(rightArea.removeFromRight(28).withTrimmedTop(6).withTrimmedBottom(6));
        }
        else
        {
            pyramidButton.setBounds({});
            loadButton.setBounds({});
        }

        nameLabel.setBounds(area.removeFromTop(22));
        metaLabel.setBounds(area.removeFromTop(17));
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

    juce::File file;
    juce::Colour accent;
    bool selectionMode = false;
    bool isSelected = false;

    bool pointerDown = false;
    bool pointerMoved = false;
    bool longPressTriggered = false;
    juce::Point<int> pressStartPos;
    double pressStartMs = 0.0;

    float loadAlpha = 0.0f;
    float targetLoadAlpha = 0.0f;

    juce::Rectangle<int> selectionCircleArea;
    juce::Label nameLabel;
    juce::Label metaLabel;
    juce::TextButton loadButton;
    HistoryPyramidButton pyramidButton;
};

class HistoryDeletePrompt : public juce::Component
{
public:
    HistoryDeletePrompt()
    {
        setInterceptsMouseClicks(true, true);

        titleLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        titleLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
        titleLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(titleLabel);

        bodyLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        bodyLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        bodyLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(bodyLabel);

        cancelButton.setButtonText("Keep");
        cancelButton.onClick = [this]()
        {
            if (onCancel)
                onCancel();
        };
        addAndMakeVisible(cancelButton);

        confirmButton.setButtonText("Remove");
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

class HistoryPageComponent : public juce::Component
{
public:
    enum class FilterMode
    {
        audio = 0,
        video = 1
    };

    std::function<void(const juce::File&)> onFileRequested;
    std::function<void(const juce::File&)> onDeleteFileRequested;

    HistoryPageComponent()
    {
        viewport = std::make_unique<juce::Viewport>();
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(viewport.get());
        viewport->setViewedComponent(contentComponent.get(), false);
        viewport->setScrollBarsShown(false, false, true, false);

        audioButton.setRadioGroupId(4101);
        videoButton.setRadioGroupId(4101);
        audioButton.setToggleState(true, juce::dontSendNotification);

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

        addAndMakeVisible(audioButton);
        addAndMakeVisible(videoButton);

        summaryLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        summaryLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
        summaryLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(summaryLabel);

        emptyLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        emptyLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
        emptyLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(emptyLabel);

        selectionSummaryLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMain);
        selectionSummaryLabel.setFont(juce::Font(juce::FontOptions(12.0f)));
        selectionSummaryLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(selectionSummaryLabel);

        cancelSelectionButton.setButtonText("CANCEL");
        cancelSelectionButton.onClick = [this]() { exitSelectionMode(); };
        addAndMakeVisible(cancelSelectionButton);

        deleteSelectedButton.setButtonText("DELETE");
        deleteSelectedButton.onClick = [this]() { confirmDeleteSelection(); };
        addAndMakeVisible(deleteSelectedButton);

        deletePrompt = std::make_unique<HistoryDeletePrompt>();
        deletePrompt->setCurrentSkin(currentSkinId);
        deletePrompt->onCancel = [this]() { hideDeletePrompt(); };
        deletePrompt->onConfirm = [this]() { performDeleteSelection(); };
        addChildComponent(deletePrompt.get());

        refreshList();
    }

    void setCurrentSkin(int skinId)
    {
        currentSkinId = skinId;

        if (deletePrompt != nullptr)
            deletePrompt->setCurrentSkin(currentSkinId);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);

        auto area = getLocalBounds().reduced(20, 0);
        g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.1f));
        g.drawHorizontalLine(sectionTabsY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));

        if (selectionMode)
        {
            g.drawHorizontalLine(selectionFooterY, static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
        }
    }

    void resized() override
    {
        const auto oldScrollY = getScrollY();
        auto area = getLocalBounds().reduced(20, 0);

        area.removeFromTop(52);

        sectionTabsY = area.getY();
        area.removeFromTop(8);
        auto buttonRow = area.removeFromTop(52);
        const int gap = 12;
        const int buttonWidth = (buttonRow.getWidth() - gap) / 2;
        audioButton.setBounds(buttonRow.removeFromLeft(buttonWidth));
        buttonRow.removeFromLeft(gap);
        videoButton.setBounds(buttonRow.removeFromLeft(buttonWidth));

        area.removeFromTop(8);
        summaryLabel.setBounds(area.removeFromTop(18));
        area.removeFromTop(8);

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
        layoutRows();
        restoreScrollY(oldScrollY);

        if (deletePrompt != nullptr)
            deletePrompt->setBounds(getLocalBounds());
    }

    void refreshList()
    {
        const auto oldScrollY = getScrollY();
        auto oldSelection = selectedPaths;
        auto oldExpandedPath = expandedPath;

        items.clear();
        rows.clear();
        contentComponent->removeAllChildren();

        auto docsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        auto allFiles = docsDir.findChildFiles(juce::File::findFiles, false, "*");

        for (const auto& file : allFiles)
        {
            if (!file.existsAsFile())
                continue;

            if (filterMode == FilterMode::audio && isAudioFile(file))
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
        int y = 0;
        const int rowHeight = 74;
        const int gap = 8;

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

    void restoreScrollY(int y)
    {
        if (viewport == nullptr || contentComponent == nullptr)
            return;

        const int maxY = juce::jmax(0, contentComponent->getHeight() - viewport->getHeight());
        viewport->setViewPosition(0, juce::jlimit(0, maxY, y));
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

    FilterMode filterMode = FilterMode::audio;
    int currentSkinId = 1;
    bool selectionMode = false;
    int sectionTabsY = 0;
    int selectionFooterY = 0;
    juce::String expandedPath;
    std::set<juce::String> selectedPaths;
    std::vector<juce::File> pendingDeleteFiles;

    std::unique_ptr<juce::Viewport> viewport;
    std::unique_ptr<juce::Component> contentComponent;
    std::unique_ptr<HistoryDeletePrompt> deletePrompt;
    std::vector<juce::File> items;
    std::vector<std::unique_ptr<HistoryRowComponent>> rows;

    HistorySegmentButton audioButton { "Audio", GoodMeterLookAndFeel::accentBlue };
    HistorySegmentButton videoButton { "Video", GoodMeterLookAndFeel::accentYellow };
    juce::Label summaryLabel;
    juce::Label emptyLabel;
    juce::Label selectionSummaryLabel;
    juce::TextButton cancelSelectionButton;
    juce::TextButton deleteSelectedButton;
};
