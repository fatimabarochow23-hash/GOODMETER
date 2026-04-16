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

#define MARATHON_ART_STYLE 1

#if MARATHON_ART_STYLE
    #include "MarathonRenderer.h"
#endif

class CharacterOptionButton : public juce::Button
{
public:
    CharacterOptionButton(const juce::String& nameToUse, juce::Colour accentToUse)
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
        auto area = getLocalBounds().toFloat().reduced(1.2f);

        auto baseFill = isDarkMode ? juce::Colours::black : GoodMeterLookAndFeel::bgMain;
        auto outline = getToggleState() ? accent : (isDarkMode ? juce::Colours::white.withAlpha(0.16f) : GoodMeterLookAndFeel::textMain.withAlpha(0.16f));
        auto textCol = isDarkMode ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f) : GoodMeterLookAndFeel::textMain;

        if (getToggleState())
            baseFill = accent.withAlpha(isDarkMode ? 0.18f : 0.08f);
        else if (isHovered)
            baseFill = (isDarkMode ? juce::Colours::white : GoodMeterLookAndFeel::textMain).withAlpha(0.035f);

        if (isPressed)
            baseFill = accent.withAlpha(isDarkMode ? 0.24f : 0.14f);

        const float radius = isDarkMode ? 9.0f : 12.0f;
        g.setColour(baseFill);
        g.fillRoundedRectangle(area, radius);

        g.setColour(outline);
        g.drawRoundedRectangle(area, radius, getToggleState() ? 2.2f : 1.2f);

        auto guideColour = getToggleState() ? accent.withAlpha(isDarkMode ? 0.55f : 0.35f)
                                            : textCol.withAlpha(isDarkMode ? 0.16f : 0.10f);
        g.setColour(guideColour);
        g.drawLine(area.getX() + 12.0f, area.getY() + 10.0f,
                   area.getX() + 30.0f, area.getY() + 10.0f, 1.0f);

        auto dotArea = area.removeFromTop(20.0f).removeFromLeft(20.0f).reduced(6.0f);
        g.setColour(accent.withAlpha(getToggleState() ? 1.0f : 0.4f));
        g.fillEllipse(dotArea);

        g.setColour(textCol);
        g.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(17.5f, juce::Font::bold));
        g.drawText(getName(), getLocalBounds().reduced(12, 10),
                   juce::Justification::centred, false);
    }

private:
    juce::Colour accent;
    bool isDarkMode = false;
};

class SettingsPageComponent : public juce::Component
{
public:
    //==========================================================================
    // Callbacks — wired by iOSMainComponent
    //==========================================================================
    std::function<void(int skinId)> onSkinChanged;           // 1=Nono, 2=Guoba
    std::function<void(int renderMode)> onCharacterRenderModeChanged; // 0=PNG, 1=ASCII
    std::function<void(int displayMode)> onMeterDisplayModeChanged; // 0=Single, 1=4-Up, 2=8-Up
    std::function<void(int standardId)> onLoudnessStandardChanged;  // 1..7
    std::function<void(bool show)> onShowImportButtonChanged;
    std::function<void(bool isDark)> onThemeChanged;         // false=Light, true=Dark

    SettingsPageComponent()
    {
#if MARATHON_ART_STYLE
        bgCanvas = std::make_unique<DotMatrixCanvas>(21, 24);
        randomizeBackground();
#endif

        // ── Section: Theme ──
        themeLabel.setText("THEME", juce::dontSendNotification);
        themeLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(themeLabel);
        themeLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(themeLabel);

        lightThemeButton.setRadioGroupId(5001);
        darkThemeButton.setRadioGroupId(5001);
        lightThemeButton.setToggleState(true, juce::dontSendNotification);

        lightThemeButton.onClick = [this]()
        {
            if (lightThemeButton.getToggleState())
            {
                isDarkTheme = false;
                if (onThemeChanged)
                    onThemeChanged(false);
                updateThemeColors();
            }
        };
        darkThemeButton.onClick = [this]()
        {
            if (darkThemeButton.getToggleState())
            {
                isDarkTheme = true;
                if (onThemeChanged)
                    onThemeChanged(true);
                updateThemeColors();
            }
        };

        addAndMakeVisible(lightThemeButton);
        addAndMakeVisible(darkThemeButton);

        // ── Section: Character ──
        characterLabel.setText("CHARACTER", juce::dontSendNotification);
        characterLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(characterLabel);
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

        // ── Section: Character render ──
        renderSectionLabel.setText("RENDER", juce::dontSendNotification);
        renderSectionLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(renderSectionLabel);
        renderSectionLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(renderSectionLabel);

        pngRenderButton.setRadioGroupId(2501);
        asciiRenderButton.setRadioGroupId(2501);
        pngRenderButton.setToggleState(true, juce::dontSendNotification);

        pngRenderButton.onClick = [this]()
        {
            if (pngRenderButton.getToggleState() && onCharacterRenderModeChanged)
                onCharacterRenderModeChanged(0);
            updateButtonStyles();
        };
        asciiRenderButton.onClick = [this]()
        {
            if (asciiRenderButton.getToggleState() && onCharacterRenderModeChanged)
                onCharacterRenderModeChanged(1);
            updateButtonStyles();
        };

        addAndMakeVisible(pngRenderButton);
        addAndMakeVisible(asciiRenderButton);

        // ── Section: Meter display ──
        displaySectionLabel.setText("DISPLAY", juce::dontSendNotification);
        displaySectionLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(displaySectionLabel);
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

        // ── Section: Loudness ──
        loudnessSectionLabel.setText("LOUDNESS", juce::dontSendNotification);
        loudnessSectionLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(loudnessSectionLabel);
        loudnessSectionLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(loudnessSectionLabel);

        auto setupStandardButton = [this](CharacterOptionButton& button, int id)
        {
            button.setRadioGroupId(4001);
            button.onClick = [this, &button, id]()
            {
                if (button.getToggleState() && onLoudnessStandardChanged)
                    onLoudnessStandardChanged(id);
                updateButtonStyles();
            };
            addAndMakeVisible(button);
        };

        setupStandardButton(streamingButton, 1);
        setupStandardButton(ebuButton, 2);
        setupStandardButton(atscButton, 3);
        setupStandardButton(netflixButton, 4);
        setupStandardButton(youtubeButton, 5);
        setupStandardButton(douyinButton, 6);
        setupStandardButton(bilibiliButton, 7);
        ebuButton.setToggleState(true, juce::dontSendNotification);

        // ── Section: Import ──
        importSectionLabel.setText("IMPORT", juce::dontSendNotification);
        importSectionLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.5f, juce::Font::bold));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(importSectionLabel);
        importSectionLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(importSectionLabel);

        showImportToggle.setButtonText("Show IMPORT AUDIO button");
        GoodMeterLookAndFeel::markAsIOSEnglishMono(showImportToggle);
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
        importHintLabel.setFont(GoodMeterLookAndFeel::iosEnglishMonoFont(12.0f));
        GoodMeterLookAndFeel::markAsIOSEnglishMono(importHintLabel);
        importHintLabel.setColour(juce::Label::textColourId, GoodMeterLookAndFeel::textMuted);
        addAndMakeVisible(importHintLabel);

        updateButtonStyles();
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
                                          : GoodMeterLookAndFeel::textMain.withAlpha(0.045f + cell.brightness * 0.110f);
                    g.setColour(drawColour);
                    g.setFont(monoFont);
                    juce::String str = juce::String::charToString(cell.symbol);
                    g.drawText(str, (int)px, (int)py, (int)cellW, (int)cellH,
                              juce::Justification::centred, false);
                }
            }
        }
#endif

        auto bounds = getLocalBounds().reduced(20, 0);
        auto sepColor = isDarkTheme ? juce::Colours::white.withAlpha(0.1f)
                                    : GoodMeterLookAndFeel::textMain.withAlpha(0.1f);

        auto drawSep = [&](int y)
        {
            g.setColour(sepColor);
            g.drawHorizontalLine(y, static_cast<float>(bounds.getX()),
                                 static_cast<float>(bounds.getRight()));
        };

        drawSep(sectionThemeY);
        drawSep(sectionCharacterY);
        drawSep(sectionRenderY);
        drawSep(sectionDisplayY);
        drawSep(sectionLoudnessY);
        drawSep(sectionImportY);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto area = bounds.reduced(20, 0);

        area.removeFromTop(52);

        // ── Theme section ──
        sectionThemeY = area.getY();
        area.removeFromTop(8);
        themeLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        auto themeRow = area.removeFromTop(52);
        int themeBtnW = (themeRow.getWidth() - 12) / 2;
        lightThemeButton.setBounds(themeRow.removeFromLeft(themeBtnW));
        themeRow.removeFromLeft(12);
        darkThemeButton.setBounds(themeRow.removeFromLeft(themeBtnW));

        area.removeFromTop(16);

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

        // ── Render section ──
        sectionRenderY = area.getY();
        area.removeFromTop(8);
        renderSectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        auto renderRow = area.removeFromTop(52);
        int renderBtnW = (renderRow.getWidth() - 12) / 2;
        pngRenderButton.setBounds(renderRow.removeFromLeft(renderBtnW));
        renderRow.removeFromLeft(12);
        asciiRenderButton.setBounds(renderRow.removeFromLeft(renderBtnW));

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

        // ── Loudness section ──
        sectionLoudnessY = area.getY();
        area.removeFromTop(8);
        loudnessSectionLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(8);

        const int standardGap = 8;
        const int standardBtnH = 42;
        const int standardBtnW = (area.getWidth() - standardGap) / 2;

        auto row1 = area.removeFromTop(standardBtnH);
        streamingButton.setBounds(row1.removeFromLeft(standardBtnW));
        row1.removeFromLeft(standardGap);
        ebuButton.setBounds(row1);
        area.removeFromTop(8);

        auto row2 = area.removeFromTop(standardBtnH);
        atscButton.setBounds(row2.removeFromLeft(standardBtnW));
        row2.removeFromLeft(standardGap);
        netflixButton.setBounds(row2);
        area.removeFromTop(8);

        auto row3 = area.removeFromTop(standardBtnH);
        youtubeButton.setBounds(row3.removeFromLeft(standardBtnW));
        row3.removeFromLeft(standardGap);
        douyinButton.setBounds(row3);
        area.removeFromTop(8);

        auto row4 = area.removeFromTop(standardBtnH);
        bilibiliButton.setBounds(row4.removeFromLeft(standardBtnW));

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

    void setCharacterRenderMode(int mode)
    {
        pngRenderButton.setToggleState(mode == 0, juce::dontSendNotification);
        asciiRenderButton.setToggleState(mode == 1, juce::dontSendNotification);
        updateButtonStyles();
    }

    void setLoudnessStandard(int standardId)
    {
        streamingButton.setToggleState(standardId == 1, juce::dontSendNotification);
        ebuButton.setToggleState(standardId == 2, juce::dontSendNotification);
        atscButton.setToggleState(standardId == 3, juce::dontSendNotification);
        netflixButton.setToggleState(standardId == 4, juce::dontSendNotification);
        youtubeButton.setToggleState(standardId == 5, juce::dontSendNotification);
        douyinButton.setToggleState(standardId == 6, juce::dontSendNotification);
        bilibiliButton.setToggleState(standardId == 7, juce::dontSendNotification);
        updateButtonStyles();
    }

    void setDarkTheme(bool dark)
    {
        isDarkTheme = dark;
        lightThemeButton.setToggleState(!dark, juce::dontSendNotification);
        darkThemeButton.setToggleState(dark, juce::dontSendNotification);
        updateThemeColors();
    }

    bool isDark() const { return isDarkTheme; }

private:
    void updateThemeColors()
    {
        auto textColor = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f) : GoodMeterLookAndFeel::textMain;
        auto mutedColor = isDarkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.72f) : GoodMeterLookAndFeel::textMuted;

        themeLabel.setColour(juce::Label::textColourId, mutedColor);
        characterLabel.setColour(juce::Label::textColourId, mutedColor);
        renderSectionLabel.setColour(juce::Label::textColourId, mutedColor);
        displaySectionLabel.setColour(juce::Label::textColourId, mutedColor);
        loudnessSectionLabel.setColour(juce::Label::textColourId, mutedColor);
        importSectionLabel.setColour(juce::Label::textColourId, mutedColor);
        showImportToggle.setColour(juce::ToggleButton::textColourId, textColor);
        showImportToggle.setColour(juce::ToggleButton::tickColourId, textColor);
        importHintLabel.setColour(juce::Label::textColourId, mutedColor);

        lightThemeButton.setDarkMode(isDarkTheme);
        darkThemeButton.setDarkMode(isDarkTheme);
        nonoButton.setDarkMode(isDarkTheme);
        guobaButton.setDarkMode(isDarkTheme);
        pngRenderButton.setDarkMode(isDarkTheme);
        asciiRenderButton.setDarkMode(isDarkTheme);
        singleModeButton.setDarkMode(isDarkTheme);
        fourUpModeButton.setDarkMode(isDarkTheme);
        eightUpModeButton.setDarkMode(isDarkTheme);
        streamingButton.setDarkMode(isDarkTheme);
        ebuButton.setDarkMode(isDarkTheme);
        atscButton.setDarkMode(isDarkTheme);
        netflixButton.setDarkMode(isDarkTheme);
        youtubeButton.setDarkMode(isDarkTheme);
        douyinButton.setDarkMode(isDarkTheme);
        bilibiliButton.setDarkMode(isDarkTheme);

        updateButtonStyles();
        repaint();
    }

private:
    void updateButtonStyles()
    {
        nonoButton.repaint();
        guobaButton.repaint();
        pngRenderButton.repaint();
        asciiRenderButton.repaint();
        singleModeButton.repaint();
        fourUpModeButton.repaint();
        eightUpModeButton.repaint();
        streamingButton.repaint();
        ebuButton.repaint();
        atscButton.repaint();
        netflixButton.repaint();
        youtubeButton.repaint();
        douyinButton.repaint();
        bilibiliButton.repaint();
    }

#if MARATHON_ART_STYLE
    void randomizeBackground()
    {
        static const char32_t symbols[] = {U'.', U'·', U'/', U'\\', U'✕', U'+', U'□', U'■', U'◢', U'◯'};
        juce::Random rng;
        const auto preset = MarathonField::Preset::settings;

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
                    sym = U'□';
                else if (y % 6 == 0 && (sym == U'.' || sym == U'·'))
                    sym = U'+';

                lastSymbol = sym;
                float brightness = MarathonField::brightnessForCell(x, y, bgCanvas->getWidth(), bgCanvas->getHeight(), preset);
                bgCanvas->setCell(x, y, sym, juce::Colours::white, 0, brightness);
            }
        }
    }
#endif

    // Layout tracking
    int sectionThemeY = 0;
    int sectionCharacterY = 0;
    int sectionRenderY = 0;
    int sectionDisplayY = 0;
    int sectionLoudnessY = 0;
    int sectionImportY = 0;

    // Theme state
    bool isDarkTheme = false;

#if MARATHON_ART_STYLE
    std::unique_ptr<DotMatrixCanvas> bgCanvas;
#endif

    // UI components
    juce::Label themeLabel;
    CharacterOptionButton lightThemeButton { "Light", GoodMeterLookAndFeel::accentCyan };
    CharacterOptionButton darkThemeButton { "Dark", GoodMeterLookAndFeel::accentPurple };

    juce::Label characterLabel;
    CharacterOptionButton nonoButton { "Nono", GoodMeterLookAndFeel::accentBlue };
    CharacterOptionButton guobaButton { "Guoba", GoodMeterLookAndFeel::accentYellow };

    juce::Label renderSectionLabel;
    CharacterOptionButton pngRenderButton { "PNG", GoodMeterLookAndFeel::accentGreen };
    CharacterOptionButton asciiRenderButton { "ASCII", GoodMeterLookAndFeel::accentSoftPink };

    juce::Label displaySectionLabel;
    CharacterOptionButton singleModeButton { "Single", GoodMeterLookAndFeel::accentBlue };
    CharacterOptionButton fourUpModeButton { "1x4", GoodMeterLookAndFeel::accentCyan };
    CharacterOptionButton eightUpModeButton { "8-Up", GoodMeterLookAndFeel::accentPurple };

    juce::Label loudnessSectionLabel;
    CharacterOptionButton streamingButton { "Streaming", GoodMeterLookAndFeel::accentCyan };
    CharacterOptionButton ebuButton { "EBU R128", GoodMeterLookAndFeel::accentSoftPink };
    CharacterOptionButton atscButton { "ATSC A/85", GoodMeterLookAndFeel::accentYellow };
    CharacterOptionButton netflixButton { "Netflix", GoodMeterLookAndFeel::accentPurple };
    CharacterOptionButton youtubeButton { "YouTube", GoodMeterLookAndFeel::accentPink };
    CharacterOptionButton douyinButton { "Douyin", GoodMeterLookAndFeel::accentBlue };
    CharacterOptionButton bilibiliButton { "Bilibili", GoodMeterLookAndFeel::accentGreen };

    juce::Label importSectionLabel;
    juce::ToggleButton showImportToggle;
    juce::Label importHintLabel;
};
