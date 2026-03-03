/*
  ==============================================================================
    GoodMeterLookAndFeel.h
    GOODMETER - Custom Look and Feel

    Color palette extracted from Gemini's index.css
    Professional audio industry aesthetic
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
 * Custom LookAndFeel for GOODMETER
 * Implements Goodhertz/FabFilter inspired flat, bold aesthetic
 */
class GoodMeterLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GoodMeterLookAndFeel()
    {
        // Set default colors
        setColour(juce::ResizableWindow::backgroundColourId, bgMain);
        setColour(juce::DocumentWindow::backgroundColourId, bgMain);
        setColour(juce::TextButton::buttonColourId, accentPink);
        setColour(juce::TextButton::textColourOffId, bgPanel);
    }

    //==========================================================================
    // Color Palette (from index.css)
    //==========================================================================

    // Background colors
    static inline const juce::Colour bgMain    = juce::Colour(0xFFF4F4F6);  // #F4F4F6
    static inline const juce::Colour bgPanel   = juce::Colour(0xFFFFFFFF);  // #FFFFFF

    // Text colors
    static inline const juce::Colour textMain  = juce::Colour(0xFF2A2A35);  // #2A2A35
    static inline const juce::Colour textMuted = juce::Colour(0xFF8A8A9D);  // #8A8A9D

    // Border color
    static inline const juce::Colour border    = juce::Colour(0xFF2A2A35);  // #2A2A35

    // Accent colors
    static inline const juce::Colour accentPink   = juce::Colour(0xFFE6335F);  // #E6335F
    static inline const juce::Colour accentPurple = juce::Colour(0xFF8C52FF);  // #8C52FF
    static inline const juce::Colour accentGreen  = juce::Colour(0xFF00D084);  // #00D084
    static inline const juce::Colour accentYellow = juce::Colour(0xFFFFD166);  // #FFD166
    static inline const juce::Colour accentCyan   = juce::Colour(0xFF06D6A0);  // #06D6A0

    // Scrollbar colors
    static inline const juce::Colour scrollTrack = bgMain;
    static inline const juce::Colour scrollThumb = juce::Colour(0xFFD1D1D6);  // #D1D1D6
    static inline const juce::Colour scrollThumbHover = juce::Colour(0xFFA1A1AA);  // #A1A1AA

    //==========================================================================
    // Typography
    //==========================================================================
    static inline const juce::String fontSans = "-apple-system";  // System font
    static inline const juce::String fontMono = "JetBrains Mono";

    //==========================================================================
    // Layout Constants
    //==========================================================================
    static constexpr float borderThickness = 4.0f;   // Thick borders
    static constexpr float cornerRadius = 8.0f;      // Rounded corners
    static constexpr float cardPadding = 16.0f;      // Internal padding
    static constexpr float cardSpacing = 12.0f;      // Spacing between cards

    //==========================================================================
    // Custom Drawing Methods
    //==========================================================================

    void drawButtonBackground(juce::Graphics& g,
                             juce::Button& button,
                             const juce::Colour& backgroundColour,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        // Flat design - no gradients
        g.setColour(shouldDrawButtonAsDown ? backgroundColour.darker(0.2f) :
                   shouldDrawButtonAsHighlighted ? backgroundColour.brighter(0.1f) :
                   backgroundColour);

        g.fillRoundedRectangle(bounds, cornerRadius);

        // Thick border
        g.setColour(border);
        g.drawRoundedRectangle(bounds, cornerRadius, borderThickness);
    }

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

        // Draw track
        g.setColour(scrollTrack);
        g.fillRect(x, y, width, height);

        // Draw thumb
        if (thumbSize > 0)
        {
            juce::Rectangle<int> thumbBounds;
            if (isScrollbarVertical)
                thumbBounds = { x, thumbStartPosition, width, thumbSize };
            else
                thumbBounds = { thumbStartPosition, y, thumbSize, height };

            g.setColour(isMouseOver ? scrollThumbHover : scrollThumb);
            g.fillRoundedRectangle(thumbBounds.toFloat(), 4.0f);
        }
    }

    //==========================================================================
    // ComboBox + PopupMenu Overrides (Industrial Minimal Skin)
    //==========================================================================

    void drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                      int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                      juce::ComboBox& box) override
    {
        // Invisible background — no border, no fill
        // Text
        g.setColour(textMain);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        juce::Rectangle<int> textRect(0, 0, width - 14, height);
        g.drawText(box.getText(), textRect, juce::Justification::centredRight, false);

        // Mini triangle arrow
        juce::Path arrow;
        float ax = static_cast<float>(width) - 7.0f;
        float ay = static_cast<float>(height) / 2.0f - 2.0f;
        arrow.addTriangle(ax - 3.5f, ay, ax + 3.5f, ay, ax, ay + 4.0f);
        g.setColour(textMuted);
        g.fillPath(arrow);
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        // Hide the default label — we draw text ourselves in drawComboBox
        label.setBounds(0, 0, 0, 0);
        label.setVisible(false);
        juce::ignoreUnused(box);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        // Deep industrial dark panel
        g.setColour(juce::Colour(0xFF2A2A35));
        g.fillRoundedRectangle(0.0f, 0.0f, static_cast<float>(width),
                               static_cast<float>(height), 6.0f);

        // Subtle inner glow border
        g.setColour(juce::Colour(0xFF3A3A45));
        g.drawRoundedRectangle(0.5f, 0.5f, static_cast<float>(width) - 1.0f,
                               static_cast<float>(height) - 1.0f, 6.0f, 1.0f);
    }

    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool /*hasSubMenu*/, const juce::String& text,
                           const juce::String& /*shortcutKeyText*/,
                           const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            g.setColour(juce::Colour(0xFF3A3A45));
            g.fillRect(area.withSizeKeepingCentre(area.getWidth() - 10, 1));
            return;
        }

        // Hover highlight — soft dark tile with rounded feel
        if (isHighlighted && isActive)
        {
            g.setColour(juce::Colour(0xFF3A3A45));
            g.fillRoundedRectangle(area.reduced(3, 1).toFloat(), 4.0f);
        }

        // Text: highlighted = pink, normal = white
        g.setColour(isHighlighted ? accentPink : juce::Colours::white.withAlpha(0.9f));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText(text, area.reduced(20, 0), juce::Justification::centredLeft, true);

        // Ticked indicator — pink dot (not the ugly checkmark)
        if (isTicked)
        {
            g.setColour(accentPink);
            g.fillEllipse(static_cast<float>(area.getX()) + 7.0f,
                          static_cast<float>(area.getCentreY()) - 2.5f,
                          5.0f, 5.0f);
        }
    }

    //==========================================================================
    // Helper Methods for Custom Components
    //==========================================================================

    /**
     * Draw a thick-bordered card background
     */
    static void drawCard(juce::Graphics& g,
                        const juce::Rectangle<int>& bounds,
                        float hoverOffset = 4.0f,
                        const juce::Colour& backgroundColor = bgPanel,
                        const juce::Colour& borderColour = juce::Colour(0xFF1A1A24))
    {
        auto b = bounds.toFloat();
        const float cr = cornerRadius;
        const float maxShadow = 8.0f;

        // Card body shifts upper-left as hoverOffset grows
        float cardX = b.getX() + (maxShadow - hoverOffset);
        float cardY = b.getY() + (maxShadow - hoverOffset);
        float cardW = b.getWidth() - maxShadow;
        float cardH = b.getHeight() - maxShadow;
        juce::Rectangle<float> cardRect(cardX, cardY, cardW, cardH);

        // 1. Hard drop shadow (offset, no blur, pure dark)
        auto shadowRect = cardRect.translated(hoverOffset, hoverOffset);
        g.setColour(borderColour);
        g.fillRoundedRectangle(shadowRect, cr);

        // 2. Card body (clinical white)
        g.setColour(backgroundColor);
        g.fillRoundedRectangle(cardRect, cr);

        // 3. Thick brutalist border
        g.setColour(borderColour);
        g.drawRoundedRectangle(cardRect, cr, 2.0f);
    }

    /**
     * Draw a status indicator dot (colored circle)
     */
    static void drawStatusDot(juce::Graphics& g,
                             float x, float y,
                             float diameter,
                             const juce::Colour& colour)
    {
        g.setColour(colour);
        g.fillEllipse(x, y, diameter, diameter);

        // Subtle outer ring
        g.setColour(colour.darker(0.3f));
        g.drawEllipse(x, y, diameter, diameter, 1.0f);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoodMeterLookAndFeel)
};
