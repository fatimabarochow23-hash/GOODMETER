/*
  ==============================================================================
    GoodMeterLookAndFeel.h
    GOODMETER - Neo-Brutalism Look and Feel

    Design language: Neo-Brutalism (New Brutalist)
    - Pure white / light grey backgrounds
    - Pure black (#2A2A35) borders, text, lines
    - Extra-thick borders (2.5-3.0f) on ALL controls
    - No rounded corners (max radius 2.0f)
    - No gradients, no soft shadows, no glow
    - Press feedback = color inversion (black bg, white text)
    - Feels like thick marker pen on white paper
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class GoodMeterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoodMeterLookAndFeel()
    {
        // ── Window backgrounds ──
        setColour(juce::ResizableWindow::backgroundColourId, bgPanel);
        setColour(juce::DocumentWindow::backgroundColourId, bgPanel);

        // ── Buttons ──
        setColour(juce::TextButton::buttonColourId, bgPanel);
        setColour(juce::TextButton::buttonOnColourId, ink);
        setColour(juce::TextButton::textColourOffId, ink);
        setColour(juce::TextButton::textColourOnId, bgPanel);

        // ── ComboBox ──
        setColour(juce::ComboBox::backgroundColourId, bgPaper);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, ink);
        setColour(juce::ComboBox::arrowColourId, ink);

        // ── Labels ──
        setColour(juce::Label::textColourId, ink);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        // ── TextEditor ──
        setColour(juce::TextEditor::backgroundColourId, bgPaper);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::outlineColourId, ink);
        setColour(juce::TextEditor::focusedOutlineColourId, ink);
        setColour(juce::TextEditor::highlightColourId, ink.withAlpha(0.15f));

        // ── ListBox ──
        setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::ListBox::textColourId, ink);
        setColour(juce::ListBox::outlineColourId, ink.withAlpha(0.3f));

        // ── ToggleButton / TickBox ──
        setColour(juce::ToggleButton::textColourId, ink);
        setColour(juce::ToggleButton::tickColourId, ink);
        setColour(juce::ToggleButton::tickDisabledColourId, textMuted);

        // ── Slider ──
        setColour(juce::Slider::thumbColourId, ink);
        setColour(juce::Slider::trackColourId, ink.withAlpha(0.2f));
        setColour(juce::Slider::backgroundColourId, bgPanel);

        // ── ScrollBar ──
        setColour(juce::ScrollBar::thumbColourId, ink.withAlpha(0.35f));
        setColour(juce::ScrollBar::trackColourId, bgMain);

        // ── GroupComponent ──
        setColour(juce::GroupComponent::outlineColourId, ink);
        setColour(juce::GroupComponent::textColourId, ink);

        // ── AlertWindow / DialogWindow ──
        setColour(juce::AlertWindow::backgroundColourId, bgPanel);
        setColour(juce::AlertWindow::textColourId, ink);
        setColour(juce::AlertWindow::outlineColourId, ink);

        // ── PopupMenu ──
        setColour(juce::PopupMenu::backgroundColourId, bgPaper);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, ink.withAlpha(0.08f));
        setColour(juce::PopupMenu::highlightedTextColourId, ink);
    }

    //==========================================================================
    // Color Palette — Neo-Brutalism: ink on paper
    //==========================================================================

    // Core duo
    static inline const juce::Colour ink      = juce::Colour(0xFF2A2A35);  // near-black
    static inline const juce::Colour bgPanel  = juce::Colour(0xFFFFFFFF);  // pure white
    static inline const juce::Colour bgMain   = juce::Colour(0xFFF4F4F6);  // off-white
    static inline const juce::Colour bgPaper  = juce::Colour(0xFFE8E4DD);  // warm paper

    // Text
    static inline const juce::Colour textMain  = ink;
    static inline const juce::Colour textMuted = juce::Colour(0xFF8A8A9D);

    // Border (alias)
    static inline const juce::Colour border = ink;

    // Accent colors (kept for meter cards — they are content colors, not chrome)
    static inline const juce::Colour accentPink     = juce::Colour(0xFFE6335F);
    static inline const juce::Colour accentPurple   = juce::Colour(0xFF8C52FF);
    static inline const juce::Colour accentGreen    = juce::Colour(0xFF00D084);
    static inline const juce::Colour accentYellow   = juce::Colour(0xFFFFD166);
    static inline const juce::Colour accentCyan     = juce::Colour(0xFF06D6A0);
    static inline const juce::Colour accentBlue     = juce::Colour(0xFF22D3EE);
    static inline const juce::Colour accentSoftPink = juce::Colour(0xFFf0a5c2);

    // Dark theme colors (Flux-style professional dark mode)
    static inline const juce::Colour bgMainDark   = juce::Colour(0xFF0F1419);  // deep blue-black
    static inline const juce::Colour bgPanelDark  = juce::Colour(0xFF1A2332);  // card/meter background (deep blue)
    static inline const juce::Colour textMainDark = juce::Colour(0xFFB0B8C0);  // light grey-white text
    static inline const juce::Colour borderDark   = juce::Colour(0xFF2A3545);  // dark blue-grey grid lines

    //==========================================================================
    // Mobile chart readability helpers
    //==========================================================================
    static constexpr bool isMobileCharts()
    {
       #if JUCE_IOS
        return true;
       #else
        return false;
       #endif
    }

    static constexpr bool preferDirectChartText()
    {
       #if JUCE_IOS
        return true;
       #else
        return false;
       #endif
    }

    static float chartFont(float desktopSize, float mobileMultiplier = 1.0f)
    {
        return isMobileCharts() ? desktopSize * mobileMultiplier : desktopSize;
    }

    static const juce::Identifier& iosEnglishMonoProperty()
    {
        static const juce::Identifier id("goodmeter_ios_english_mono");
        return id;
    }

    static void markAsIOSEnglishMono(juce::Component& component)
    {
        component.getProperties().set(iosEnglishMonoProperty(), true);
    }

    static bool shouldUseIOSEnglishMono(const juce::Component& component)
    {
        return static_cast<bool>(component.getProperties().getWithDefault(iosEnglishMonoProperty(), false));
    }

    static bool isAsciiEnglishLike(const juce::String& text)
    {
        bool hasAsciiLetterOrDigit = false;

        for (auto ch : text)
        {
            const auto c = static_cast<juce::juce_wchar>(ch);

            if (c >= 0x4E00)
                return false;

            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                hasAsciiLetterOrDigit = true;
        }

        return hasAsciiLetterOrDigit;
    }

    static juce::Font iosEnglishMonoFont(float size, int styleFlags = juce::Font::plain)
    {
        return juce::Font(juce::Font::getDefaultMonospacedFontName(), size, styleFlags);
    }

    static juce::Font iosEnglishMonoFrom(const juce::Font& base, bool forceBold = false)
    {
        int styleFlags = juce::Font::plain;

        if (forceBold || base.isBold())
            styleFlags |= juce::Font::bold;
        if (base.isItalic())
            styleFlags |= juce::Font::italic;

        return iosEnglishMonoFont(base.getHeight(), styleFlags);
    }

    static void setEditorialPopupMode(bool enabled, bool darkTheme)
    {
        editorialPopupMode = enabled ? (darkTheme ? 2 : 1) : 0;
    }

    static juce::Colour liquidGlassReadableText(bool darkTheme)
    {
        return darkTheme ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                         : juce::Colour(0xFF1B1A22).withAlpha(0.94f);
    }

    static juce::Colour liquidGlassFill(bool darkTheme, juce::Colour contentTint, float alpha = -1.0f)
    {
        auto base = darkTheme ? juce::Colour(0xFF090D14)
                              : juce::Colour(0xFFF6F2EA);
        auto tintMix = darkTheme ? 0.15f : 0.08f;
        auto fill = base.interpolatedWith(contentTint, tintMix);
        return fill.withAlpha(alpha > 0.0f ? alpha : (darkTheme ? 0.62f : 0.82f));
    }

    static void drawLiquidGlassPlate(juce::Graphics& g,
                                     juce::Rectangle<float> panel,
                                     bool darkTheme,
                                     juce::Colour contentTint,
                                     float radius,
                                     float fillAlpha = -1.0f)
    {
        juce::Path panelPath;
        panelPath.addRoundedRectangle(panel, radius);

        auto fill = liquidGlassFill(darkTheme, contentTint, fillAlpha);
        auto outline = (darkTheme ? juce::Colour(0xFFF6EEE3)
                                  : juce::Colour(0xFF1A1A24))
                           .withAlpha(darkTheme ? 0.10f : 0.08f);
        auto topHighlight = juce::Colours::white.withAlpha(darkTheme ? 0.18f : 0.32f);
        auto innerGlow = juce::Colours::white.withAlpha(darkTheme ? 0.07f : 0.11f);
        auto bottomShade = juce::Colours::black.withAlpha(darkTheme ? 0.12f : 0.05f);

        g.setColour(juce::Colours::black.withAlpha(darkTheme ? 0.10f : 0.06f));
        g.fillRoundedRectangle(panel.translated(0.0f, 2.0f), radius);

        g.setColour(fill);
        g.fillPath(panelPath);

        juce::ColourGradient verticalSheen(topHighlight,
                                           panel.getCentreX(), panel.getY(),
                                           juce::Colours::white.withAlpha(0.0f),
                                           panel.getCentreX(), panel.getBottom(),
                                           false);
        verticalSheen.addColour(0.28, innerGlow);
        verticalSheen.addColour(0.80, bottomShade.withAlpha(0.0f));
        verticalSheen.addColour(1.00, bottomShade);
        g.setGradientFill(verticalSheen);
        g.fillPath(panelPath);

        g.setColour(outline);
        g.drawRoundedRectangle(panel, radius, 0.9f);

        g.setColour(topHighlight.withAlpha(darkTheme ? 0.22f : 0.34f));
        g.drawLine(panel.getX() + 12.0f, panel.getY() + 1.15f,
                   panel.getRight() - 12.0f, panel.getY() + 1.15f, 1.0f);
    }

    static float chartStroke(float desktopWidth, float mobileMultiplier = 1.22f, float mobileMinimum = 0.0f)
    {
        if (!isMobileCharts())
            return desktopWidth;

        const float boosted = desktopWidth * mobileMultiplier;
        return mobileMinimum > 0.0f ? juce::jmax(boosted, mobileMinimum) : boosted;
    }

    static juce::Colour chartMuted(float alpha = 1.0f)
    {
       #if JUCE_IOS
        auto c = juce::Colour(0xFF68687A);
        if (alpha >= 0.999f)
            return c;
        return c.withAlpha(juce::jmax(alpha, 0.82f));
       #else
        return alpha >= 0.999f ? textMuted : textMuted.withAlpha(alpha);
       #endif
    }

    static juce::Colour chartInk(float alpha)
    {
       #if JUCE_IOS
        return ink.withAlpha(juce::jmax(alpha, 0.24f));
       #else
        return ink.withAlpha(alpha);
       #endif
    }

    // Scrollbar
    static inline const juce::Colour scrollTrack     = bgMain;
    static inline const juce::Colour scrollThumb     = juce::Colour(0xFFD1D1D6);
    static inline const juce::Colour scrollThumbHover = juce::Colour(0xFFA1A1AA);

    // Cross-component state: AudioLabContent sets these for title bar modes
    static inline bool holoTitleBar = false;
    static inline bool spectroTitleBar = false;
    static inline bool spectroProcessed = false;
    static inline int editorialPopupMode = 0;

    //==========================================================================
    // Typography
    //==========================================================================
    static inline const juce::String fontSans = "-apple-system";
    static inline const juce::String fontMono = "JetBrains Mono";

    //==========================================================================
    // Layout Constants — Neo-Brutalism: thick, sharp, blunt
    //==========================================================================
    static constexpr float brutalBorder   = 2.5f;    // thick control borders
    static constexpr float brutalCorner   = 2.0f;    // near-zero radius
    static constexpr float borderThickness = 4.0f;   // card-level borders (kept for MeterCard)
    static constexpr float cornerRadius   = 2.0f;    // global max radius
    static constexpr float cardPadding    = 16.0f;
    static constexpr float cardSpacing    = 12.0f;

    //==========================================================================
    // Button — ink border on press, ghost on paper normally (for AudioLab)
    // Normal state: transparent bg, no border (text only)
    // Hover: faint ink border appears
    // Press: full inversion (ink fill, white text)
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g,
                             juce::Button& button,
                             const juce::Colour& /*backgroundColour*/,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        auto textCol = button.findColour(juce::TextButton::textColourOffId);

        if (shouldDrawButtonAsDown)
        {
            // PRESS: full fill in text colour (inversion)
            g.setColour(textCol);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            // HOVER: subtle tint + border in text colour
            g.setColour(textCol.withAlpha(0.08f));
            g.fillRoundedRectangle(bounds, brutalCorner);
            g.setColour(textCol.withAlpha(0.6f));
            g.drawRoundedRectangle(bounds, brutalCorner, 1.5f);
        }
        // Normal: completely transparent — no fill, no border
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        auto font = shouldUseIOSEnglishMono(button) && isAsciiEnglishLike(button.getButtonText())
                        ? iosEnglishMonoFont(15.5f, juce::Font::bold)
                        : juce::Font(14.0f, juce::Font::bold);
        g.setFont(font);
        // Use per-component colours (supports dark/light mode)
        g.setColour(isButtonDown
            ? button.findColour(juce::TextButton::textColourOnId)
            : button.findColour(juce::TextButton::textColourOffId));
        g.drawText(button.getButtonText(), button.getLocalBounds(),
                   juce::Justification::centred, true);
    }

    //==========================================================================
    // ToggleButton / TickBox — square box, thick border, ink fill when ticked
    //==========================================================================
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused(shouldDrawButtonAsDown);

        auto tickCol = button.findColour(juce::ToggleButton::tickColourId);
        auto textCol = button.findColour(juce::ToggleButton::textColourId);

        const float boxSize = 16.0f;
        const float boxX = 4.0f;
        const float boxY = (static_cast<float>(button.getHeight()) - boxSize) * 0.5f;

        // Box background — transparent normally, faint on hover
        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(tickCol.withAlpha(0.06f));
            g.fillRoundedRectangle(boxX, boxY, boxSize, boxSize, 1.0f);
        }

        // Border — thin normally, thicker on hover
        float borderW = shouldDrawButtonAsHighlighted ? 2.0f : 1.5f;
        g.setColour(tickCol.withAlpha(shouldDrawButtonAsHighlighted ? 0.7f : 0.4f));
        g.drawRoundedRectangle(boxX, boxY, boxSize, boxSize, 1.0f, borderW);

        // Tick: solid fill
        if (button.getToggleState())
        {
            g.setColour(tickCol);
            g.fillRoundedRectangle(boxX + 2.0f, boxY + 2.0f,
                                   boxSize - 4.0f, boxSize - 4.0f, 1.0f);
        }

        // Label text
        g.setColour(textCol);
        auto toggleFont = shouldUseIOSEnglishMono(button) && isAsciiEnglishLike(button.getButtonText())
                              ? iosEnglishMonoFont(14.0f, juce::Font::bold)
                              : juce::Font(13.0f, juce::Font::bold);
        g.setFont(toggleFont);
        auto textArea = button.getLocalBounds()
                          .withLeft(static_cast<int>(boxX + boxSize + 6.0f));
        g.drawText(button.getButtonText(), textArea,
                   juce::Justification::centredLeft, true);
    }

    //==========================================================================
    // ComboBox — thick border box, ink arrow, invert on open
    //==========================================================================
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        auto bounds = juce::Rectangle<float>(0, 0,
                          static_cast<float>(width), static_cast<float>(height));
        auto textCol = box.findColour(juce::ComboBox::textColourId);
        auto bgCol   = box.findColour(juce::ComboBox::backgroundColourId);
        const bool editorialGlass = editorialPopupMode != 0 && shouldUseIOSEnglishMono(box);

        if (editorialGlass)
        {
            auto panel = bounds.reduced(1.2f, 3.0f);
            const bool darkTheme = editorialPopupMode == 2;
            const float radius = 15.5f;
            auto contentTint = darkTheme
                                   ? bgCol.interpolatedWith(accentBlue, 0.28f)
                                   : bgCol.interpolatedWith(accentBlue, 0.08f);
            drawLiquidGlassPlate(g, panel, darkTheme, contentTint, radius,
                                 darkTheme ? 0.56f : 0.84f);

            if (box.isMouseOver() || isButtonDown)
            {
                auto hoverPath = juce::Path();
                hoverPath.addRoundedRectangle(panel.reduced(1.0f), radius - 1.0f);
                auto hoverCol = liquidGlassReadableText(darkTheme).withAlpha(isButtonDown ? 0.09f : 0.05f);
                g.setColour(hoverCol);
                g.fillPath(hoverPath);
            }

            textCol = liquidGlassReadableText(darkTheme);
            auto comboFont = juce::Font(14.5f, juce::Font::bold);
            g.setFont(comboFont);
            g.drawText(box.getText().trim(),
                       juce::Rectangle<int>(12, 0, width - 36, height),
                       juce::Justification::centredLeft, true);

            juce::Path arrow;
            float ax = static_cast<float>(width) - 17.0f;
            float ay = static_cast<float>(height) * 0.5f - 2.0f;
            arrow.startNewSubPath(ax - 4.5f, ay);
            arrow.lineTo(ax, ay + 4.8f);
            arrow.lineTo(ax + 4.5f, ay);
            g.setColour(textCol.withAlpha(0.92f));
            g.strokePath(arrow, juce::PathStrokeType(1.35f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            return;
        }

        if (isButtonDown)
        {
            // PRESS: invert (fill with text colour)
            g.setColour(textCol);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }
        else if (box.isMouseOver())
        {
            // HOVER: subtle border in text colour
            g.setColour(textCol.withAlpha(0.06f));
            g.fillRoundedRectangle(bounds, brutalCorner);
            g.setColour(textCol.withAlpha(0.6f));
            g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, 1.5f);
        }
        // Normal: no border, no fill — just text on background

        // Text
        g.setColour(isButtonDown ? bgCol : textCol);
        auto comboFont = shouldUseIOSEnglishMono(box) && isAsciiEnglishLike(box.getText())
                             ? iosEnglishMonoFont(14.5f, juce::Font::bold)
                             : juce::Font(13.0f, juce::Font::bold);
        g.setFont(comboFont);
        g.drawText(box.getText(),
                   juce::Rectangle<int>(8, 0, width - 28, height),
                   juce::Justification::centredLeft, true);

        // Arrow — bold triangle
        juce::Path arrow;
        float ax = static_cast<float>(width) - 16.0f;
        float ay = static_cast<float>(height) * 0.5f - 3.0f;
        arrow.addTriangle(ax - 5.0f, ay, ax + 5.0f, ay, ax, ay + 6.0f);
        g.setColour(isButtonDown ? bgCol : textCol);
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        // Hide default label — we draw text ourselves
        label.setBounds(0, 0, 0, 0);
        label.setVisible(false);
        juce::ignoreUnused(box);
    }

    //==========================================================================
    // Label — ink on white, ALWAYS bold
    //==========================================================================
    juce::Font getLabelFont(juce::Label& label) override
    {
        if (shouldUseIOSEnglishMono(label) && isAsciiEnglishLike(label.getText()))
            return iosEnglishMonoFrom(label.getFont(), true);

        // Force bold on all labels for Neo-Brutalism weight
        auto f = label.getFont();
        return f.withStyle(juce::Font::bold);
    }

    juce::Font getTextButtonFont(juce::TextButton& button, int buttonHeight) override
    {
        if (shouldUseIOSEnglishMono(button) && isAsciiEnglishLike(button.getButtonText()))
        {
            const float fontSize = juce::jmin(16.0f, static_cast<float>(buttonHeight) * 0.48f);
            return iosEnglishMonoFont(fontSize, juce::Font::bold);
        }

        return juce::LookAndFeel_V4::getTextButtonFont(button, buttonHeight);
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        if (shouldUseIOSEnglishMono(box) && isAsciiEnglishLike(box.getText()))
            return editorialPopupMode != 0
                       ? juce::Font(14.5f, juce::Font::bold)
                       : iosEnglishMonoFont(13.0f, juce::Font::bold);

        return juce::LookAndFeel_V4::getComboBoxFont(box);
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        g.fillAll(label.findColour(juce::Label::backgroundColourId));

        auto textArea = getLabelBorderSize(label).subtractedFrom(label.getLocalBounds());

        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(getLabelFont(label));
        g.drawText(label.getText(), textArea, label.getJustificationType(), true);
    }

    //==========================================================================
    // TextEditor — thick border, no rounded
    //==========================================================================
    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& editor) override
    {
        auto bounds = juce::Rectangle<float>(0, 0,
                          static_cast<float>(width), static_cast<float>(height));
        auto col = editor.findColour(editor.hasKeyboardFocus(true)
            ? juce::TextEditor::focusedOutlineColourId
            : juce::TextEditor::outlineColourId);
        g.setColour(col);
        g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, brutalBorder);
    }

    //==========================================================================
    // GroupComponent — thin border, bold title, paper punch-out
    //==========================================================================
    void drawGroupComponentOutline(juce::Graphics& g, int width, int height,
                                    const juce::String& text,
                                    const juce::Justification& /*position*/,
                                    juce::GroupComponent& group) override
    {
        juce::ignoreUnused(group);
        auto font = juce::Font(13.0f, juce::Font::bold);
        float textW = font.getStringWidthFloat(text) + 12.0f;
        float textH = font.getHeight();
        float top = textH * 0.5f;

        auto bounds = juce::Rectangle<float>(0, top,
                          static_cast<float>(width), static_cast<float>(height) - top);

        // Thin border rect (paper-friendly)
        g.setColour(ink.withAlpha(0.25f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, 1.0f);

        // Title background punch-out (matches paper)
        g.setColour(bgPaper);
        g.fillRect(8.0f, 0.0f, textW, textH);

        // Title text
        g.setColour(ink);
        g.setFont(font);
        g.drawText(text, juce::Rectangle<float>(14.0f, 0.0f, textW - 12.0f, textH),
                   juce::Justification::centredLeft, true);
    }

    //==========================================================================
    // Scrollbar — ink thumb on off-white track, square
    //==========================================================================
    void drawScrollbar(juce::Graphics& g,
                      juce::ScrollBar& scrollbar,
                      int x, int y, int width, int height,
                      bool isScrollbarVertical,
                      int thumbStartPosition,
                      int thumbSize,
                      bool isMouseOver,
                      bool isMouseDown) override
    {
        juce::ignoreUnused(scrollbar, isMouseDown);

        g.setColour(bgMain);
        g.fillRect(x, y, width, height);

        if (thumbSize > 0)
        {
            juce::Rectangle<int> thumbBounds;
            if (isScrollbarVertical)
                thumbBounds = { x + 1, thumbStartPosition, width - 2, thumbSize };
            else
                thumbBounds = { thumbStartPosition, y + 1, thumbSize, height - 2 };

            g.setColour(isMouseOver ? ink.withAlpha(0.55f) : ink.withAlpha(0.35f));
            g.fillRect(thumbBounds);

            // Thin border on thumb
            g.setColour(ink.withAlpha(0.6f));
            g.drawRect(thumbBounds, 1);
        }
    }

    //==========================================================================
    // PopupMenu — mode-aware: paper (default) / holo grid / spectro black
    //==========================================================================
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        auto area = juce::Rectangle<float>(0, 0,
                        static_cast<float>(width), static_cast<float>(height));

        if (editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar)
        {
            const bool darkTheme = editorialPopupMode == 2;
            auto panel = area.reduced(0.8f);
            const float radius = 16.0f;
            auto contentTint = darkTheme ? accentBlue.interpolatedWith(accentPink, 0.18f)
                                         : accentBlue;
            drawLiquidGlassPlate(g, panel, darkTheme, contentTint, radius,
                                 darkTheme ? 0.72f : 0.88f);
        }
        else if (holoTitleBar)
        {
            // Holo mode: dark bg + holographic grid
            static const juce::Colour holoDark(0xFF1A1A24);
            static const juce::Colour holoGrey(0xFFD5D3DE);
            g.setColour(holoDark);
            g.fillRect(0, 0, width, height);

            // Grid
            float cellW = 3.0f, cellH = 2.0f;
            g.setColour(holoGrey.withAlpha(0.06f));
            for (float y = 0.0f; y < area.getBottom(); y += cellH)
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, area.getRight());
            for (float x = 0.0f; x < area.getRight(); x += cellW)
                g.drawVerticalLine(static_cast<int>(x), 0.0f, area.getBottom());

            g.setColour(holoGrey.withAlpha(0.2f));
            g.drawRect(0, 0, width, height, 1);
        }
        else if (spectroTitleBar)
        {
            // Spectro mode: pure black
            g.setColour(juce::Colour(0xFF0A0A18));
            g.fillRect(0, 0, width, height);

            g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.15f));
            g.drawRect(0, 0, width, height, 1);
        }
        else
        {
            // Default: warm paper + blueprint grid
            g.setColour(bgPaper);
            g.fillRect(0, 0, width, height);
            drawBlueprintGrid(g, area);
            g.setColour(ink.withAlpha(0.2f));
            g.drawRect(0, 0, width, height, 1);
        }
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                           const juce::String& /*shortcutKeyText*/,
                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/) override
    {
        // Determine text colour based on mode
        juce::Colour textCol = ink;
        juce::Colour mutedCol = textMuted;
        juce::Colour highlightBg = ink.withAlpha(0.08f);

        if (editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar)
        {
            const bool darkTheme = editorialPopupMode == 2;
            if (editorialPopupMode == 2)
            {
                textCol = juce::Colour(0xFFF6EEE3).withAlpha(0.96f);
                mutedCol = juce::Colour(0xFFF6EEE3).withAlpha(0.58f);
                highlightBg = liquidGlassFill(true, accentBlue.interpolatedWith(accentPink, 0.20f), 0.26f);
            }
            else
            {
                textCol = juce::Colour(0xFF1A1A24).withAlpha(0.96f);
                mutedCol = juce::Colour(0xFF1A1A24).withAlpha(0.56f);
                highlightBg = liquidGlassFill(false, accentBlue, 0.40f);
            }

            if (isHighlighted && isActive)
            {
                auto highlightArea = area.toFloat().reduced(5.5f, 2.0f);
                juce::Path itemPath;
                itemPath.addRoundedRectangle(highlightArea, 9.5f);
                g.setColour(highlightBg);
                g.fillPath(itemPath);

                juce::ColourGradient pillSheen(juce::Colours::white.withAlpha(darkTheme ? 0.14f : 0.22f),
                                               highlightArea.getCentreX(), highlightArea.getY(),
                                               juce::Colours::white.withAlpha(0.0f),
                                               highlightArea.getCentreX(), highlightArea.getBottom(),
                                               false);
                g.setGradientFill(pillSheen);
                g.fillPath(itemPath);

                g.setColour(textCol.withAlpha(darkTheme ? 0.12f : 0.09f));
                g.drawRoundedRectangle(highlightArea, 9.5f, 0.8f);
            }
        }
        else if (holoTitleBar)
        {
            textCol = juce::Colour(0xFFD5D3DE);   // white/grey
            mutedCol = juce::Colour(0xFF8A8A9D);
            highlightBg = textCol.withAlpha(0.10f);
        }
        else if (spectroTitleBar)
        {
            if (spectroProcessed)
                textCol = juce::Colour(0xFFFFB840);  // yellow
            else
                textCol = juce::Colour(0xFF4A9EFF);  // blue
            mutedCol = textCol.withAlpha(0.4f);
            highlightBg = textCol.withAlpha(0.10f);
        }

        if (isSeparator)
        {
            g.setColour(textCol.withAlpha(0.15f));
            g.fillRect(area.getX() + 12, area.getCentreY(), area.getWidth() - 24, 1);
            return;
        }

        if (isHighlighted && isActive && !(editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar))
        {
            g.setColour(highlightBg);
            g.fillRoundedRectangle(area.toFloat().reduced(5.0f, 1.5f), 8.0f);
        }

        g.setColour(isActive ? textCol : mutedCol);
        g.setFont(editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar
                      ? juce::Font(14.25f, juce::Font::plain)
                      : juce::Font(13.0f, juce::Font::bold));
        g.drawText(text, area.reduced(20, 0), juce::Justification::centredLeft, true);

        if (isTicked)
        {
            g.setColour(textCol);
            float ty = static_cast<float>(area.getCentreY()) - 2.5f;
            g.fillEllipse(static_cast<float>(area.getX()) + 7.0f, ty, 5.0f, 5.0f);
        }
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        juce::Colour headerCol = textMuted;
        if (editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar)
            headerCol = editorialPopupMode == 2
                            ? juce::Colour(0xFFF6EEE3).withAlpha(0.58f)
                            : juce::Colour(0xFF1A1A24).withAlpha(0.52f);
        else if (holoTitleBar)
            headerCol = juce::Colour(0xFF8A8A9D);
        else if (spectroTitleBar)
            headerCol = spectroProcessed ? juce::Colour(0xFFFFB840).withAlpha(0.5f)
                                         : juce::Colour(0xFF4A9EFF).withAlpha(0.5f);

        g.setColour(headerCol);
        g.setFont(editorialPopupMode != 0 && !holoTitleBar && !spectroTitleBar
                      ? juce::Font(12.0f, juce::Font::bold)
                      : juce::Font(11.0f, juce::Font::bold));
        g.drawText(sectionName.toUpperCase(), area.reduced(20, 0),
                   juce::Justification::centredLeft, true);

        g.setColour(headerCol.withAlpha(0.25f));
        g.fillRect(area.getX() + 12, area.getBottom() - 1, area.getWidth() - 24, 1);
    }

    //==========================================================================
    // TabbedButtonBar — paper background with grid, ink text
    //==========================================================================
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar& bar, juce::Graphics& g) override
    {
        auto bounds = bar.getLocalBounds().toFloat();
        g.setColour(bgPaper);
        g.fillRect(bounds);
        drawBlueprintGrid(g, bounds);
    }

    void drawTabButton(juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override
    {
        auto area = button.getActiveArea().toFloat();
        bool isFront = button.isFrontTab();

        if (isFront)
        {
            // Active tab: slightly brighter paper + bottom ink line
            g.setColour(bgPaper.brighter(0.05f));
            g.fillRect(area);
            g.setColour(ink);
            g.fillRect(area.getX(), area.getBottom() - 2.0f, area.getWidth(), 2.0f);
        }
        else if (isMouseOver || isMouseDown)
        {
            g.setColour(ink.withAlpha(0.06f));
            g.fillRect(area);
        }

        // Tab text
        g.setColour(isFront ? ink : textMuted);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), area, juce::Justification::centred);
    }

    //==========================================================================
    // Slider (Linear) — for sample rate / buffer size if used
    //==========================================================================
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        juce::ignoreUnused(style);
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(width), static_cast<float>(height));
        auto thumbCol = slider.findColour(juce::Slider::thumbColourId);

        // Track
        float trackH = 4.0f;
        float trackY = bounds.getCentreY() - trackH * 0.5f;
        g.setColour(thumbCol.withAlpha(0.2f));
        g.fillRect(bounds.getX(), trackY, bounds.getWidth(), trackH);

        // Filled portion
        g.setColour(thumbCol);
        g.fillRect(bounds.getX(), trackY, sliderPos - bounds.getX(), trackH);

        // Thumb: square block
        float thumbW = 14.0f, thumbH = 20.0f;
        float thumbX = sliderPos - thumbW * 0.5f;
        float thumbY = bounds.getCentreY() - thumbH * 0.5f;

        g.setColour(slider.isMouseOverOrDragging() ? thumbCol : thumbCol.withAlpha(0.85f));
        g.fillRect(thumbX, thumbY, thumbW, thumbH);
    }

    //==========================================================================
    // DocumentWindow title bar — Paper style (default) or Holo torn-paper
    //==========================================================================
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int w, int h, int /*titleSpaceX*/, int /*titleSpaceW*/,
                                    const juce::Image* /*icon*/, bool /*drawTitleTextOnLeft*/) override
    {
        if (spectroTitleBar)
        {
            drawSpectroTitleBar(g, w, h, window.getName());
            return;
        }
        if (holoTitleBar)
        {
            drawHoloTitleBar(g, w, h, window.getName());
            return;
        }

        // ── Default: Blueprint paper title bar ──
        static const juce::Colour paperColour(0xFFE8E4DD);
        g.setColour(paperColour);
        g.fillRect(0, 0, w, h);

        // Subtle blueprint grid on title bar too
        g.setColour(juce::Colour(0x0C000000));
        for (int x = 0; x < w; x += 16)
            g.drawVerticalLine(x, 0.0f, static_cast<float>(h));

        // Bottom border line (faint)
        g.setColour(ink.withAlpha(0.15f));
        g.fillRect(0, h - 1, w, 1);

        // Title text — bold, ink-black, centered
        g.setColour(ink);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(window.getName(), 0, 0, w, h, juce::Justification::centred, true);
    }

    //==========================================================================
    // Holo torn-paper title bar — grey-white holographic grid with wavy bottom
    // The title bar is dark holo at the bottom, with a grey-white zone on top
    // that has a torn/wavy bottom edge (like ripped paper over holographic grid)
    //==========================================================================
    void drawHoloTitleBar(juce::Graphics& g, int w, int h, const juce::String& title)
    {
        static const juce::Colour holoDark(0xFF1A1A24);
        static const juce::Colour holoGrey(0xFFD5D3DE);

        const float cellW = 3.0f;
        const float cellH = 2.0f;
        float fW = static_cast<float>(w);
        float fH = static_cast<float>(h);

        // Fill entire title bar with dark holographic background
        g.setColour(holoDark);
        g.fillRect(0, 0, w, h);

        // Draw dark holographic grid across entire title bar
        g.setColour(holoGrey.withAlpha(0.06f));
        for (float y = 0.0f; y < fH; y += cellH)
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, fW);
        for (float x = 0.0f; x < fW; x += cellW)
            g.drawVerticalLine(static_cast<int>(x), 0.0f, fH);

        // ── Grey-white torn paper zone (top portion) ──
        // The tear line sits ~6px above the bottom of the title bar
        // with a multi-sine wave for irregular torn paper feel
        float tearBaseY = fH - 6.0f;  // base tear line position

        // Paint grey-white cells above the wavy tear line
        // Use combined sine waves for organic torn-paper feel
        for (float x = 0.0f; x < fW; x += cellW)
        {
            // Multi-frequency wave: primary + secondary + tertiary
            float phase = x * 0.025f;
            float wave = std::sin(phase) * 2.0f
                       + std::sin(phase * 2.7f + 1.3f) * 1.2f
                       + std::sin(phase * 5.1f + 0.7f) * 0.6f;
            // wave range ≈ [-3.8, +3.8], scale to ≈ [-1.5, +1.5] cells
            float tearY = tearBaseY + wave;
            // Snap to grid
            tearY = std::floor(tearY / cellH) * cellH;

            // Fill grey-white cells from top down to the tear line
            for (float y = 0.0f; y < tearY; y += cellH)
            {
                g.setColour(holoGrey.withAlpha(0.25f));
                g.fillRect(x, y, cellW - 0.5f, cellH - 0.5f);
            }
        }

        // ── Grey-white grid lines on the torn paper zone ──
        // (slightly brighter lines over the grey-white cells)
        g.setColour(holoGrey.withAlpha(0.10f));
        for (float y = 0.0f; y < tearBaseY - 4.0f; y += cellH)
            g.drawHorizontalLine(static_cast<int>(y), 0.0f, fW);
        for (float x = 0.0f; x < fW; x += cellW)
            g.drawVerticalLine(static_cast<int>(x), 0.0f, tearBaseY - 4.0f);

        // Title text — light on dark holo zone, centered vertically in upper area
        g.setColour(holoGrey);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(title, 0, 0, w, static_cast<int>(tearBaseY - 2.0f),
                   juce::Justification::centred, true);
    }

    //==========================================================================
    // Spectrogram title bar — black bg + blue/yellow meteor decorations
    // Blue meteors (left): visible before processing
    // Yellow meteors (right): visible after processing
    //==========================================================================
    void drawSpectroTitleBar(juce::Graphics& g, int w, int h, const juce::String& title)
    {
        static const juce::Colour spectroBg(0xFF0A0A18);
        static const juce::Colour meteorBlue(0xFF4A9EFF);
        static const juce::Colour meteorYellow(0xFFFFB840);

        float fW = static_cast<float>(w);
        float fH = static_cast<float>(h);

        // Pure black background — matches spectrogram area below
        g.setColour(spectroBg);
        g.fillRect(0, 0, w, h);

        // ── Left: blue meteors (pre-process state indicator) ──
        if (!spectroProcessed)
            drawMeteorGroup(g, 8.0f, fW * 0.38f, fH, meteorBlue, false);

        // ── Right: yellow meteors (post-process state indicator) ──
        if (spectroProcessed)
            drawMeteorGroup(g, fW * 0.62f, fW - 8.0f, fH, meteorYellow, true);

        // ── Title text: white, centered ──
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(title, 0, 0, w, h, juce::Justification::centred, true);
    }

    /** Draw a group of meteor streaks in a horizontal region */
    static void drawMeteorGroup(juce::Graphics& g, float left, float right,
                                 float height, const juce::Colour& colour,
                                 bool tailGoesLeft)
    {
        // 7 deterministic meteor positions (fractions within the region)
        struct MeteorDef { float xFrac, yFrac, len, angleDeg, alpha; };
        static const MeteorDef defs[] = {
            { 0.12f, 0.28f, 24.0f, 8.0f,  0.75f },
            { 0.32f, 0.58f, 17.0f, 5.0f,  0.45f },
            { 0.52f, 0.20f, 30.0f, 12.0f, 0.65f },
            { 0.78f, 0.68f, 19.0f, 7.0f,  0.38f },
            { 0.22f, 0.76f, 22.0f, 10.0f, 0.55f },
            { 0.68f, 0.40f, 15.0f, 4.0f,  0.30f },
            { 0.45f, 0.84f, 26.0f, 9.0f,  0.50f },
        };

        float regionW = right - left;

        for (const auto& m : defs)
        {
            float headX = left + m.xFrac * regionW;
            float headY = m.yFrac * height;
            float angleRad = m.angleDeg * (3.14159265f / 180.0f);

            // Tail extends away from center; slight downward angle
            float tailDirX = tailGoesLeft ? -std::cos(angleRad) : std::cos(angleRad);
            float tailDirY = std::abs(std::sin(angleRad));

            // Draw tail as fading segments (head → tail)
            int segments = 6;
            float segLen = m.len / static_cast<float>(segments);

            for (int i = 0; i < segments; ++i)
            {
                float t0 = static_cast<float>(i) * segLen;
                float t1 = static_cast<float>(i + 1) * segLen;
                float fade = 1.0f - (static_cast<float>(i) / static_cast<float>(segments));

                float x0 = headX + tailDirX * t0;
                float y0 = headY + tailDirY * t0;
                float x1 = headX + tailDirX * t1;
                float y1 = headY + tailDirY * t1;

                float thickness = 1.8f * fade + 0.3f;
                g.setColour(colour.withAlpha(m.alpha * fade * fade));
                g.drawLine(x0, y0, x1, y1, thickness);
            }

            // Bright head dot
            g.setColour(colour.withAlpha(juce::jmin(m.alpha + 0.15f, 1.0f)));
            g.fillEllipse(headX - 1.5f, headY - 1.5f, 3.0f, 3.0f);

            // Soft glow around head
            g.setColour(colour.withAlpha(m.alpha * 0.25f));
            g.fillEllipse(headX - 3.5f, headY - 3.5f, 7.0f, 7.0f);
        }
    }

    void drawResizableWindowBorder(juce::Graphics& g, int w, int h,
                                    const juce::BorderSize<int>& border,
                                    juce::ResizableWindow& /*window*/) override
    {
        juce::ignoreUnused(border);
        // Thin subtle border instead of thick ink (blends with paper)
        g.setColour(ink.withAlpha(0.25f));
        g.drawRect(0, 0, w, h, 1);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        if (buttonType == juce::DocumentWindow::closeButton)
        {
            // Close button — ink X on paper, subtle until hover
            class PaperCloseButton : public juce::Button
            {
            public:
                PaperCloseButton() : juce::Button("close") {}
                void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
                {
                    auto b = getLocalBounds().toFloat().reduced(1.0f);
                    static const juce::Colour inkC(0xFF2A2A35);
                    bool darkMode = GoodMeterLookAndFeel::holoTitleBar
                                 || GoodMeterLookAndFeel::spectroTitleBar;
                    auto fgCol = darkMode ? juce::Colour(0xFFD5D3DE) : inkC;

                    if (isDown)
                    {
                        g.setColour(juce::Colour(0xFFE6335F));
                        g.fillRect(b);
                    }
                    else if (isOver)
                    {
                        g.setColour(fgCol.withAlpha(0.1f));
                        g.fillRect(b);
                        g.setColour(fgCol.withAlpha(0.5f));
                        g.drawRect(b, 1.5f);
                    }

                    // X mark
                    auto inner = b.reduced(5.0f);
                    float alpha = isOver || isDown ? 1.0f : (darkMode ? 0.6f : 0.4f);
                    g.setColour(isDown ? juce::Colours::white : fgCol.withAlpha(alpha));
                    g.drawLine(inner.getX(), inner.getY(),
                               inner.getRight(), inner.getBottom(), 2.0f);
                    g.drawLine(inner.getRight(), inner.getY(),
                               inner.getX(), inner.getBottom(), 2.0f);
                }
            };
            return new PaperCloseButton();
        }
        return juce::LookAndFeel_V4::createDocumentWindowButton(buttonType);
    }

    void positionDocumentWindowButtons(juce::DocumentWindow& /*window*/,
                                        int titleBarX, int titleBarY,
                                        int titleBarW, int titleBarH,
                                        juce::Button* minimiseButton,
                                        juce::Button* maximiseButton,
                                        juce::Button* closeButton,
                                        bool /*positionTitleBarButtonsOnLeft*/) override
    {
        const int btnSize = titleBarH - 8;
        if (closeButton != nullptr)
            closeButton->setBounds(titleBarX + titleBarW - btnSize - 6,
                                    titleBarY + 4, btnSize, btnSize);
        if (minimiseButton != nullptr) minimiseButton->setVisible(false);
        if (maximiseButton != nullptr) maximiseButton->setVisible(false);
    }

    //==========================================================================
    // Helper: drawBlueprintGrid — reusable paper grid lines (fine 16px + major 80px)
    //==========================================================================
    static void drawBlueprintGrid(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float gridSmall = 16.0f;
        float gridLarge = 80.0f;

        // Fine grid
        g.setColour(juce::Colour(0x0C000000));
        for (float x = area.getX(); x < area.getRight(); x += gridSmall)
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        for (float y = area.getY(); y < area.getBottom(); y += gridSmall)
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());

        // Major grid
        g.setColour(juce::Colour(0x18000000));
        for (float x = area.getX(); x < area.getRight(); x += gridLarge)
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        for (float y = area.getY(); y < area.getBottom(); y += gridLarge)
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
    }

    //==========================================================================
    // Helper: drawCard — Neo-Brutalism card with hard offset shadow
    //==========================================================================
    static void drawCard(juce::Graphics& g,
                        const juce::Rectangle<int>& bounds,
                        float hoverOffset = 4.0f,
                        const juce::Colour& backgroundColor = bgPanel,
                        const juce::Colour& borderColour = juce::Colour(0xFF1A1A24))
    {
        auto b = bounds.toFloat();
        const float cr = brutalCorner;
        const float maxShadow = 8.0f;

        if (b.getWidth() <= maxShadow || b.getHeight() <= maxShadow)
            return;

        float cardX = b.getX() + (maxShadow - hoverOffset);
        float cardY = b.getY() + (maxShadow - hoverOffset);
        float cardW = b.getWidth() - maxShadow;
        float cardH = b.getHeight() - maxShadow;
        juce::Rectangle<float> cardRect(cardX, cardY, cardW, cardH);

        // 1. Hard drop shadow
        auto shadowRect = cardRect.translated(hoverOffset, hoverOffset);
        g.setColour(borderColour);
        g.fillRoundedRectangle(shadowRect, cr);

        // 2. Card body
        g.setColour(backgroundColor);
        g.fillRoundedRectangle(cardRect, cr);

        // 3. Thick border
        g.setColour(borderColour);
        g.drawRoundedRectangle(cardRect, cr, brutalBorder);
    }

    //==========================================================================
    // Helper: drawStatusDot
    //==========================================================================
    static void drawStatusDot(juce::Graphics& g,
                             float x, float y,
                             float diameter,
                             const juce::Colour& colour)
    {
        g.setColour(colour);
        g.fillEllipse(x, y, diameter, diameter);

        g.setColour(colour.darker(0.3f));
        g.drawEllipse(x, y, diameter, diameter, 1.0f);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoodMeterLookAndFeel)
};
