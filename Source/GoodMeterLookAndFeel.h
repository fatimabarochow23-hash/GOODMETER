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
    // Helper Methods for Custom Components
    //==========================================================================

    /**
     * Draw a thick-bordered card background
     */
    static void drawCard(juce::Graphics& g,
                        const juce::Rectangle<int>& bounds,
                        const juce::Colour& backgroundColor = bgPanel,
                        const juce::Colour& borderColour = border)
    {
        auto b = bounds.toFloat();

        // Fill background
        g.setColour(backgroundColor);
        g.fillRoundedRectangle(b, cornerRadius);

        // Draw thick border
        g.setColour(borderColour);
        g.drawRoundedRectangle(b.reduced(borderThickness * 0.5f),
                               cornerRadius,
                               borderThickness);
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
