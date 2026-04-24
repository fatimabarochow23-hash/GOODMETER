#pragma once

#include <JuceHeader.h>

namespace GoodMeterDigitalTimecode
{
    inline bool hasSegment(char c, int segment)
    {
        switch (c)
        {
            case '0': return segment != 6;
            case '1': return segment == 1 || segment == 2;
            case '2': return segment == 0 || segment == 1 || segment == 6 || segment == 4 || segment == 3;
            case '3': return segment == 0 || segment == 1 || segment == 6 || segment == 2 || segment == 3;
            case '4': return segment == 5 || segment == 6 || segment == 1 || segment == 2;
            case '5': return segment == 0 || segment == 5 || segment == 6 || segment == 2 || segment == 3;
            case '6': return segment == 0 || segment == 5 || segment == 6 || segment == 4 || segment == 2 || segment == 3;
            case '7': return segment == 0 || segment == 1 || segment == 2;
            case '8': return true;
            case '9': return segment != 4;
            default:  return false;
        }
    }

    inline float glyphAdvance(char c, float height)
    {
        const float digitWidth = height * 0.72f;
        const float colonWidth = height * 0.24f;
        const float gap = height * 0.12f;
        return (c == ':') ? (colonWidth + gap) : (digitWidth + gap);
    }

    inline float preferredWidth(const juce::String& text, float height)
    {
        float total = 0.0f;
        for (int i = 0; i < text.length(); ++i)
            total += glyphAdvance((char) text[i], height);

        return juce::jmax(0.0f, total - height * 0.12f);
    }

    inline void drawGlyph(juce::Graphics& g, char c, juce::Rectangle<float> bounds, juce::Colour colour)
    {
        const float t = juce::jmax(1.4f, bounds.getHeight() * 0.12f);
        const float w = bounds.getWidth();
        const float h = bounds.getHeight();
        const float midY = bounds.getY() + h * 0.5f - t * 0.5f;
        const float upperH = h * 0.5f - t * 1.1f;
        const float lowerY = bounds.getY() + h * 0.5f + t * 0.6f;
        const float vertH = juce::jmax(2.0f, upperH);

        auto horiz = [&] (float y)
        {
            return juce::Rectangle<float>(bounds.getX() + t * 0.78f, y, w - t * 1.56f, t);
        };
        auto leftUpper = juce::Rectangle<float>(bounds.getX(), bounds.getY() + t * 0.64f, t, vertH);
        auto rightUpper = juce::Rectangle<float>(bounds.getRight() - t, bounds.getY() + t * 0.64f, t, vertH);
        auto leftLower = juce::Rectangle<float>(bounds.getX(), lowerY, t, vertH);
        auto rightLower = juce::Rectangle<float>(bounds.getRight() - t, lowerY, t, vertH);

        g.setColour(colour);

        if (c == ':')
        {
            const float dot = juce::jmax(1.8f, t * 0.92f);
            const float cx = bounds.getCentreX() - dot * 0.5f;
            g.fillRoundedRectangle(cx, bounds.getY() + h * 0.30f, dot, dot, dot * 0.25f);
            g.fillRoundedRectangle(cx, bounds.getY() + h * 0.66f, dot, dot, dot * 0.25f);
            return;
        }

        if (!juce::CharacterFunctions::isDigit(c))
            return;

        if (hasSegment(c, 0)) g.fillRoundedRectangle(horiz(bounds.getY()), t * 0.18f);
        if (hasSegment(c, 1)) g.fillRoundedRectangle(rightUpper, t * 0.18f);
        if (hasSegment(c, 2)) g.fillRoundedRectangle(rightLower, t * 0.18f);
        if (hasSegment(c, 3)) g.fillRoundedRectangle(horiz(bounds.getBottom() - t), t * 0.18f);
        if (hasSegment(c, 4)) g.fillRoundedRectangle(leftLower, t * 0.18f);
        if (hasSegment(c, 5)) g.fillRoundedRectangle(leftUpper, t * 0.18f);
        if (hasSegment(c, 6)) g.fillRoundedRectangle(horiz(midY), t * 0.18f);
    }

    inline void draw(juce::Graphics& g,
                     juce::Rectangle<float> area,
                     const juce::String& text,
                     juce::Colour colour,
                     juce::Colour glowColour = juce::Colours::transparentBlack)
    {
        if (text.isEmpty() || area.getWidth() <= 4.0f || area.getHeight() <= 4.0f)
            return;

        const float desiredHeight = area.getHeight();
        const float baseWidth = preferredWidth(text, desiredHeight);
        if (baseWidth <= 0.0f)
            return;

        const float scale = juce::jmin(1.0f, area.getWidth() / baseWidth);
        const float glyphHeight = desiredHeight * scale;
        const float totalWidth = preferredWidth(text, glyphHeight);
        float x = area.getCentreX() - totalWidth * 0.5f;
        const float y = area.getCentreY() - glyphHeight * 0.5f;

        for (int i = 0; i < text.length(); ++i)
        {
            const char c = (char) text[i];
            const float adv = glyphAdvance(c, glyphHeight);
            const float glyphWidth = adv - glyphHeight * 0.12f;
            auto bounds = juce::Rectangle<float>(x, y, glyphWidth, glyphHeight);

            if (glowColour.getFloatAlpha() > 0.0f)
                drawGlyph(g, c, bounds.translated(0.65f, 0.65f), glowColour);

            drawGlyph(g, c, bounds, colour);
            x += adv;
        }
    }
}
