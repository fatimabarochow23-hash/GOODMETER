/*
  ==============================================================================
    SettingsPageComponent.h
    GOODMETER iOS - Page 3: Settings

    Neo-Brutalism white style settings page:
      - Nono/Guoba skin selector
      - Show/Hide IMPORT AUDIO button toggle (default OFF)

    Changes are broadcast via callbacks to iOSMainComponent.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../GoodMeterLookAndFeel.h"

class CharacterOptionButton : public juce::Button
{
public:
    CharacterOptionButton(const juce::String& nameToUse, juce::Colour accentToUse)
        : juce::Button(nameToUse), accent(accentToUse)
    {
        setClickingTogglesState(true);
    }

    void paintButton(juce::Graphics& g, bool isHovered, bool isPressed) override
    {
        auto area = getLocalBounds().toFloat().reduced(1.5f);

        auto baseFill = GoodMeterLookAndFeel::bgMain;
        auto outline = getToggleState() ? accent : GoodMeterLookAndFeel::textMain.withAlpha(0.16f);
        auto textCol = GoodMeterLookAndFeel::textMain;

        if (getToggleState())
            baseFill = accent.withAlpha(0.08f);
        else if (isHovered)
            baseFill = GoodMeterLookAndFeel::textMain.withAlpha(0.035f);

        if (isPressed)
            baseFill = accent.withAlpha(0.14f);

        g.setColour(baseFill);
        g.fillRoundedRectangle(area, 14.0f);

        g.setColour(outline);
        g.drawRoundedRectangle(area, 14.0f, getToggleState() ? 3.0f : 1.5f);

        auto dotArea = area.removeFromTop(20.0f).removeFromLeft(20.0f).reduced(6.0f);
        g.setColour(accent.withAlpha(getToggleState() ? 1.0f : 0.4f));
        g.fillEllipse(dotArea);

        g.setColour(textCol);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(getName(), getLocalBounds().reduced(12, 10),
                   juce::Justification::centred, false);
    }

private:
    juce::Colour accent;
};

class SettingsPageComponent : public juce::Component
{
public:
    //==========================================================================
    // Callbacks — wired by iOSMainComponent
    //==========================================================================
    std::function<void(int skinId)> onSkinChanged;           // 1=Nono, 2=Guoba
    std::function<void(int displayMode)> onMeterDisplayModeChanged; // 0=Single, 1=4-Up, 2=8-Up
    std::function<void(bool show)> onShowImportButtonChanged;

    SettingsPageComponent()
    {
        // ── Section: Character ──
        characterLabel.setText("CHARACTER", juce::dontSendNotification);
        characterLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        characterLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(characterLabel);

        nonoButton.setRadioGroupId(2001);
        guobaButton.setRadioGroupId(2001);
        nonoButton.setToggleState(true, juce::dontSendNotification);

        nonoButton.onClick = [this]()
        {
            if (nonoButton.getToggleState() && onSkinChanged)
                onSkinChanged(1);
            updateButtonStyles();
        };
        guobaButton.onClick = [this]()
        {
            if (guobaButton.getToggleState() && onSkinChanged)
                onSkinChanged(2);
            updateButtonStyles();
        };

        addAndMakeVisible(nonoButton);
        addAndMakeVisible(guobaButton);

        // ── Section: Meter display ──
        displaySectionLabel.setText("DISPLAY", juce::dontSendNotification);
        displaySectionLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        displaySectionLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(displaySectionLabel);

        singleModeButton.setRadioGroupId(3001);
        fourUpModeButton.setRadioGroupId(3001);
        eightUpModeButton.setRadioGroupId(3001);
        singleModeButton.setToggleState(true, juce::dontSendNotification);

        singleModeButton.onClick = [this]()
        {
            if (singleModeButton.getToggleState() && onMeterDisplayModeChanged)
                onMeterDisplayModeChanged(0);
            updateButtonStyles();
        };

        fourUpModeButton.onClick = [this]()
        {
            if (fourUpModeButton.getToggleState() && onMeterDisplayModeChanged)
                onMeterDisplayModeChanged(1);
            updateButtonStyles();
        };

        eightUpModeButton.onClick = [this]()
        {
            if (eightUpModeButton.getToggleState() && onMeterDisplayModeChanged)
                onMeterDisplayModeChanged(2);
            updateButtonStyles();
        };

        addAndMakeVisible(singleModeButton);
        addAndMakeVisible(fourUpModeButton);
        addAndMakeVisible(eightUpModeButton);

        // ── Section: Import ──
        importSectionLabel.setText("IMPORT", juce::dontSendNotification);
        importSectionLabel.setFont(juce::Font(11.0f, juce::Font::bold));
        importSectionLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(importSectionLabel);

        showImportToggle.setButtonText("Show IMPORT AUDIO button");
        showImportToggle.setToggleState(false, juce::dontSendNotification);
        showImportToggle.setColour(juce::ToggleButton::textColourId, GoodMeterLookAndFeel::textMain);
        showImportToggle.setColour(juce::ToggleButton::tickColourId, GoodMeterLookAndFeel::textMain);
        showImportToggle.onClick = [this]()
        {
            if (onShowImportButtonChanged)
                onShowImportButtonChanged(showImportToggle.getToggleState());
        };
        addAndMakeVisible(showImportToggle);

        importHintLabel.setText("When OFF, double-tap the character to import audio",
                                juce::dontSendNotification);
        importHintLabel.setFont(juce::Font(11.0f));
        importHintLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(importHintLabel);

        updateButtonStyles();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);

        auto bounds = getLocalBounds().reduced(20, 0);

        // Separator lines between sections
        auto drawSep = [&](int y)
        {
            g.setColour(GoodMeterLookAndFeel::textMain.withAlpha(0.1f));
            g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                                 static_cast<float>(bounds.getRight()));
        };

        drawSep(sectionCharacterY);
        drawSep(sectionDisplayY);
        drawSep(sectionImportY);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto area = bounds.reduced(20, 0);

        // Keep the same top breathing room without a visible title
        area.removeFromTop(52);

        // ── Character section ──
        sectionCharacterY = area.getY();
        area.removeFromTop(8);
        characterLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        auto skinRow = area.removeFromTop(88);
        int btnW = (skinRow.getWidth() - 12) / 2;
        nonoButton.setBounds(skinRow.removeFromLeft(btnW));
        skinRow.removeFromLeft(12);
        guobaButton.setBounds(skinRow.removeFromLeft(btnW));

        area.removeFromTop(16);

        // ── Display section ──
        sectionDisplayY = area.getY();
        area.removeFromTop(8);
        displaySectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        auto displayRow = area.removeFromTop(78);
        const int displayGap = 8;
        int displayBtnW = (displayRow.getWidth() - displayGap * 2) / 3;
        singleModeButton.setBounds(displayRow.removeFromLeft(displayBtnW));
        displayRow.removeFromLeft(displayGap);
        fourUpModeButton.setBounds(displayRow.removeFromLeft(displayBtnW));
        displayRow.removeFromLeft(displayGap);
        eightUpModeButton.setBounds(displayRow.removeFromLeft(displayBtnW));

        area.removeFromTop(16);

        // ── Import section ──
        sectionImportY = area.getY();
        area.removeFromTop(8);
        importSectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);
        showImportToggle.setBounds(area.removeFromTop(36));
        area.removeFromTop(4);
        importHintLabel.setBounds(area.removeFromTop(20));
    }

    /** Sync current skin to button state (called from outside) */
    void setCurrentSkin(int skinId)
    {
        nonoButton.setToggleState(skinId == 1, juce::dontSendNotification);
        guobaButton.setToggleState(skinId == 2, juce::dontSendNotification);
        updateButtonStyles();
    }

    void setShowImportButton(bool show)
    {
        showImportToggle.setToggleState(show, juce::dontSendNotification);
    }

    void setMeterDisplayMode(int mode)
    {
        singleModeButton.setToggleState(mode == 0, juce::dontSendNotification);
        fourUpModeButton.setToggleState(mode == 1, juce::dontSendNotification);
        eightUpModeButton.setToggleState(mode == 2, juce::dontSendNotification);
        updateButtonStyles();
    }

private:
    void updateButtonStyles()
    {
        nonoButton.repaint();
        guobaButton.repaint();
        singleModeButton.repaint();
        fourUpModeButton.repaint();
        eightUpModeButton.repaint();
    }

    // Layout tracking
    int sectionCharacterY = 0;
    int sectionDisplayY = 0;
    int sectionImportY = 0;

    // UI components
    juce::Label characterLabel;
    CharacterOptionButton nonoButton { "Nono", GoodMeterLookAndFeel::accentBlue };
    CharacterOptionButton guobaButton { "Guoba", GoodMeterLookAndFeel::accentYellow };

    juce::Label displaySectionLabel;
    CharacterOptionButton singleModeButton { "Single", GoodMeterLookAndFeel::accentBlue };
    CharacterOptionButton fourUpModeButton { "1x4", GoodMeterLookAndFeel::accentCyan };
    CharacterOptionButton eightUpModeButton { "8-Up", GoodMeterLookAndFeel::accentPurple };

    juce::Label importSectionLabel;
    juce::ToggleButton showImportToggle;
    juce::Label importHintLabel;
};
