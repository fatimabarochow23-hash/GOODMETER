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
        setColour(juce::ComboBox::backgroundColourId, bgPanel);
        setColour(juce::ComboBox::textColourId, ink);
        setColour(juce::ComboBox::outlineColourId, ink);
        setColour(juce::ComboBox::arrowColourId, ink);

        // ── Labels ──
        setColour(juce::Label::textColourId, ink);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        // ── TextEditor ──
        setColour(juce::TextEditor::backgroundColourId, bgPanel);
        setColour(juce::TextEditor::textColourId, ink);
        setColour(juce::TextEditor::outlineColourId, ink);
        setColour(juce::TextEditor::focusedOutlineColourId, ink);
        setColour(juce::TextEditor::highlightColourId, ink.withAlpha(0.15f));

        // ── ListBox ──
        setColour(juce::ListBox::backgroundColourId, bgPanel);
        setColour(juce::ListBox::textColourId, ink);
        setColour(juce::ListBox::outlineColourId, ink);

        // ── ToggleButton / TickBox ──
        setColour(juce::ToggleButton::textColourId, ink);
        setColour(juce::ToggleButton::tickColourId, ink);
        setColour(juce::ToggleButton::tickDisabledColourId, textMuted);

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
        setColour(juce::PopupMenu::backgroundColourId, bgPanel);
        setColour(juce::PopupMenu::textColourId, ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, ink);
        setColour(juce::PopupMenu::highlightedTextColourId, bgPanel);
    }

    //==========================================================================
    // Color Palette — Neo-Brutalism: ink on paper
    //==========================================================================

    // Core duo
    static inline const juce::Colour ink      = juce::Colour(0xFF2A2A35);  // near-black
    static inline const juce::Colour bgPanel  = juce::Colour(0xFFFFFFFF);  // pure white
    static inline const juce::Colour bgMain   = juce::Colour(0xFFF4F4F6);  // off-white

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

    // Scrollbar
    static inline const juce::Colour scrollTrack     = bgMain;
    static inline const juce::Colour scrollThumb     = juce::Colour(0xFFD1D1D6);
    static inline const juce::Colour scrollThumbHover = juce::Colour(0xFFA1A1AA);

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
    // Button — ink border, color-invert on press
    //==========================================================================
    void drawButtonBackground(juce::Graphics& g,
                             juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);

        if (shouldDrawButtonAsDown)
        {
            // PRESS: full inversion — ink fill
            g.setColour(ink);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }
        else
        {
            // Normal / hover
            g.setColour(shouldDrawButtonAsHighlighted
                            ? backgroundColour.interpolatedWith(ink, 0.06f)
                            : backgroundColour);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }

        // Thick brutalist border
        g.setColour(ink);
        g.drawRoundedRectangle(bounds, brutalCorner, brutalBorder);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*isMouseOverButton*/, bool isButtonDown) override
    {
        auto font = juce::Font(14.0f, juce::Font::bold);
        g.setFont(font);
        // Invert text on press
        g.setColour(isButtonDown ? bgPanel : ink);
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

        const float boxSize = 16.0f;
        const float boxX = 4.0f;
        const float boxY = (static_cast<float>(button.getHeight()) - boxSize) * 0.5f;

        // Box background
        g.setColour(shouldDrawButtonAsHighlighted ? bgMain : bgPanel);
        g.fillRoundedRectangle(boxX, boxY, boxSize, boxSize, 1.0f);

        // Thick border
        g.setColour(ink);
        g.drawRoundedRectangle(boxX, boxY, boxSize, boxSize, 1.0f, brutalBorder);

        // Tick: solid ink fill with white checkmark
        if (button.getToggleState())
        {
            g.setColour(ink);
            g.fillRoundedRectangle(boxX + 2.0f, boxY + 2.0f,
                                   boxSize - 4.0f, boxSize - 4.0f, 1.0f);
        }

        // Label text
        g.setColour(ink);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
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

        if (isButtonDown)
        {
            // PRESS: invert
            g.setColour(ink);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }
        else
        {
            g.setColour(bgPanel);
            g.fillRoundedRectangle(bounds, brutalCorner);
        }

        // Thick border
        g.setColour(ink);
        g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, brutalBorder);

        // Text
        g.setColour(isButtonDown ? bgPanel : ink);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(box.getText(),
                   juce::Rectangle<int>(8, 0, width - 28, height),
                   juce::Justification::centredLeft, true);

        // Arrow — bold triangle
        juce::Path arrow;
        float ax = static_cast<float>(width) - 16.0f;
        float ay = static_cast<float>(height) * 0.5f - 3.0f;
        arrow.addTriangle(ax - 5.0f, ay, ax + 5.0f, ay, ax, ay + 6.0f);
        g.setColour(isButtonDown ? bgPanel : ink);
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
        // Force bold on all labels for Neo-Brutalism weight
        auto f = label.getFont();
        return f.withStyle(juce::Font::bold);
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
        g.setColour(editor.hasKeyboardFocus(true) ? ink : ink.withAlpha(0.7f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, brutalBorder);
    }

    //==========================================================================
    // GroupComponent — thick border, bold title, square corners
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

        // Thick border rect
        g.setColour(ink);
        g.drawRoundedRectangle(bounds.reduced(0.5f), brutalCorner, brutalBorder);

        // Title background punch-out
        g.setColour(bgPanel);
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
    // PopupMenu — Neo-Brutalism: white bg, ink text, ink-fill on hover
    //==========================================================================
    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        // White panel
        g.setColour(bgPanel);
        g.fillRect(0, 0, width, height);

        // Thick outer border
        g.setColour(ink);
        g.drawRect(0, 0, width, height, 3);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                           const juce::String& /*shortcutKeyText*/,
                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            g.setColour(ink);
            g.fillRect(area.getX() + 8, area.getCentreY(), area.getWidth() - 16, 2);
            return;
        }

        // Hover: full ink fill (color inversion)
        if (isHighlighted && isActive)
        {
            g.setColour(ink);
            g.fillRect(area);
        }

        // Text
        g.setColour(isHighlighted && isActive ? bgPanel : ink);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(text, area.reduced(20, 0), juce::Justification::centredLeft, true);

        // Ticked: ink square (not circle, not checkmark)
        if (isTicked)
        {
            auto tickColour = isHighlighted && isActive ? bgPanel : ink;
            g.setColour(tickColour);
            float ty = static_cast<float>(area.getCentreY()) - 3.0f;
            g.fillRect(static_cast<float>(area.getX()) + 6.0f, ty, 6.0f, 6.0f);
        }
    }

    void drawPopupMenuSectionHeader(juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        g.setColour(ink);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText(sectionName.toUpperCase(), area.reduced(20, 0),
                   juce::Justification::centredLeft, true);

        g.setColour(ink.withAlpha(0.3f));
        g.fillRect(area.getX() + 8, area.getBottom() - 1, area.getWidth() - 16, 1);
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

        // Track
        float trackH = 4.0f;
        float trackY = bounds.getCentreY() - trackH * 0.5f;
        g.setColour(ink.withAlpha(0.2f));
        g.fillRect(bounds.getX(), trackY, bounds.getWidth(), trackH);

        // Filled portion
        g.setColour(ink);
        g.fillRect(bounds.getX(), trackY, sliderPos - bounds.getX(), trackH);

        // Thumb: square ink block
        float thumbW = 14.0f, thumbH = 20.0f;
        float thumbX = sliderPos - thumbW * 0.5f;
        float thumbY = bounds.getCentreY() - thumbH * 0.5f;

        g.setColour(slider.isMouseOverOrDragging() ? ink : ink.withAlpha(0.85f));
        g.fillRect(thumbX, thumbY, thumbW, thumbH);
        g.setColour(bgPanel);
        g.drawRect(thumbX, thumbY, thumbW, thumbH, 1.0f);
    }

    //==========================================================================
    // DocumentWindow title bar — Neo-Brutalism: ink bar, white text, no gradient
    //==========================================================================
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window, juce::Graphics& g,
                                    int w, int h, int /*titleSpaceX*/, int /*titleSpaceW*/,
                                    const juce::Image* /*icon*/, bool /*drawTitleTextOnLeft*/) override
    {
        // Ink-black title bar
        g.setColour(ink);
        g.fillRect(0, 0, w, h);

        // Bottom border accent
        g.setColour(bgPanel.withAlpha(0.15f));
        g.fillRect(0, h - 1, w, 1);

        // Title text — bold, white, centered
        g.setColour(bgPanel);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(window.getName(), 0, 0, w, h, juce::Justification::centred, true);
    }

    void drawResizableWindowBorder(juce::Graphics& g, int w, int h,
                                    const juce::BorderSize<int>& border,
                                    juce::ResizableWindow& /*window*/) override
    {
        juce::ignoreUnused(border);
        g.setColour(ink);
        g.drawRect(0, 0, w, h, 3);
    }

    juce::Button* createDocumentWindowButton(int buttonType) override
    {
        if (buttonType == juce::DocumentWindow::closeButton)
        {
            // Custom painted close button — white border + white X on ink title bar
            class BrutalCloseButton : public juce::Button
            {
            public:
                BrutalCloseButton() : juce::Button("close") {}
                void paintButton(juce::Graphics& g, bool isOver, bool isDown) override
                {
                    auto b = getLocalBounds().toFloat().reduced(1.0f);

                    // Background: transparent normally, white fill on hover/press
                    if (isDown)
                    {
                        g.setColour(juce::Colour(0xFFE6335F)); // accentPink on press
                        g.fillRect(b);
                    }
                    else if (isOver)
                    {
                        g.setColour(juce::Colours::white.withAlpha(0.2f));
                        g.fillRect(b);
                    }

                    // White border — always visible on ink title bar
                    g.setColour(juce::Colours::white);
                    g.drawRect(b, 2.0f);

                    // White X mark — thick strokes
                    auto inner = b.reduced(5.0f);
                    g.setColour(juce::Colours::white);
                    g.drawLine(inner.getX(), inner.getY(),
                               inner.getRight(), inner.getBottom(), 2.5f);
                    g.drawLine(inner.getRight(), inner.getY(),
                               inner.getX(), inner.getBottom(), 2.5f);
                }
            };
            return new BrutalCloseButton();
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
