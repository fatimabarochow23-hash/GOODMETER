/*
  ==============================================================================
    PsrMeterComponent.h
    GOODMETER - Peak to Short-Term Ratio (PSR) Dynamic Monitor

    Professional transient dynamics visualization:
    - PSR = Peak dB - Short-Term LUFS
    - Symmetrical mirror waveform (holographic aesthetic)
    - Electro-cyan tech color palette
    - Danger zone at PSR < 8 (over-compressed warning)
    - Rolling history buffer (~400 samples @ 60Hz = ~6.7 seconds)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class PsrMeterComponent : public juce::Component,
                           public juce::Timer
{
public:
    //==========================================================================
    PsrMeterComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        psrHistory.resize(historySize, 0.0f);
        setSize(100, 200);
        startTimerHz(60);
    }

    ~PsrMeterComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty() || bounds.getHeight() < 30.0f)
            return;

        // Reserve readout area at top
        const float readoutH = juce::jlimit(18.0f, 32.0f, bounds.getHeight() * 0.12f);
        auto readoutBounds = bounds.removeFromTop(readoutH);
        // Small padding below readout
        bounds.removeFromTop(2.0f);

        // Waveform area
        auto waveBounds = bounds;

        // 1. Grid
        drawGrid(g, waveBounds);

        // 2. Danger zone
        drawDangerZone(g, waveBounds);

        // 3. Mirror waveform
        drawMirrorWaveform(g, waveBounds);

        // 4. Center line (0 axis)
        drawCenterLine(g, waveBounds);

        // 5. Readout
        drawReadout(g, readoutBounds);
    }

    void resized() override {}

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // History ring buffer
    static constexpr int historySize = 400;
    std::vector<float> psrHistory;
    int historyWriteIndex = 0;

    // Current smoothed PSR value
    float currentPsr = 0.0f;
    float displayPsr = 0.0f;   // Fast lerp for waveform animation
    float textPsr = 0.0f;      // Heavy-damped lerp for stable numeric readout

    // Y axis range
    static constexpr float maxPsrDisplay = 20.0f;
    static constexpr float dangerThreshold = 8.0f;

    // Electro-cyan palette
    static inline const juce::Colour techCyan = juce::Colour(0xFF20C997);

    //==========================================================================
    void timerCallback() override
    {
        // Read peak and short-term LUFS from processor
        const float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
        const float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
        const float peak = juce::jmax(peakL, peakR);
        const float shortTerm = audioProcessor.lufsShortTerm.load(std::memory_order_relaxed);

        // Calculate PSR
        float psr = 0.0f;
        if (peak > -60.0f && shortTerm > -60.0f)
        {
            psr = peak - shortTerm;
            // Clamp to sane range
            psr = juce::jlimit(0.0f, 30.0f, psr);
        }

        currentPsr = psr;

        // Smooth display value (fast — for waveform animation)
        displayPsr += (currentPsr - displayPsr) * 0.3f;

        // Heavy-damped text value (slow — for stable numeric readout)
        textPsr += (currentPsr - textPsr) * 0.08f;

        // Push to history ring buffer
        psrHistory[historyWriteIndex] = currentPsr;
        historyWriteIndex = (historyWriteIndex + 1) % historySize;

        repaint();
    }

    //==========================================================================
    /**
     * Read history in chronological order (oldest first, newest last)
     */
    float getHistoryAt(int index) const
    {
        // historyWriteIndex points to the NEXT write position = oldest data
        return psrHistory[(historyWriteIndex + index) % historySize];
    }

    //==========================================================================
    /**
     * Grid: ultra-faint horizontal lines at PSR = 3, 6, 9, 12, 15, 18
     * Mirrored above and below center
     */
    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float centerY = bounds.getCentreY();
        const float halfH = bounds.getHeight() / 2.0f;

        g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(0.15f));

        const float gridValues[] = { 3.0f, 6.0f, 9.0f, 12.0f, 15.0f, 18.0f };
        const float labelFontSize = juce::jlimit(7.0f, 10.0f, halfH * 0.08f);

        for (float val : gridValues)
        {
            float yOffset = juce::jmap(val, 0.0f, maxPsrDisplay, 0.0f, halfH);
            float yTop = centerY - yOffset;
            float yBot = centerY + yOffset;

            // Horizontal lines
            g.drawHorizontalLine(static_cast<int>(yTop), bounds.getX(), bounds.getRight());
            g.drawHorizontalLine(static_cast<int>(yBot), bounds.getX(), bounds.getRight());

            // Labels on left edge (top half only)
            g.setFont(juce::Font(labelFontSize));
            g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(0.3f));
            g.drawText(juce::String(static_cast<int>(val)),
                       static_cast<int>(bounds.getX() + 2),
                       static_cast<int>(yTop - 6),
                       20, 12,
                       juce::Justification::centredLeft, false);

            // Reset colour for next grid line
            g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(0.15f));
        }
    }

    //==========================================================================
    /**
     * Danger zone: red dashed lines at PSR = 8 (over-compression warning)
     */
    void drawDangerZone(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float centerY = bounds.getCentreY();
        const float halfH = bounds.getHeight() / 2.0f;

        float dangerOffset = juce::jmap(dangerThreshold, 0.0f, maxPsrDisplay, 0.0f, halfH);
        float dangerTop = centerY - dangerOffset;
        float dangerBot = centerY + dangerOffset;

        g.setColour(GoodMeterLookAndFeel::accentPink.withAlpha(0.5f));

        // Top danger line (dashed)
        {
            juce::Path dashPath;
            dashPath.startNewSubPath(bounds.getX(), dangerTop);
            dashPath.lineTo(bounds.getRight(), dangerTop);
            float dashLengths[2] = { 6.0f, 4.0f };
            juce::PathStrokeType strokeType(1.0f);
            strokeType.createDashedStroke(dashPath, dashPath, dashLengths, 2);
            g.strokePath(dashPath, strokeType);
        }

        // Bottom danger line (dashed)
        {
            juce::Path dashPath;
            dashPath.startNewSubPath(bounds.getX(), dangerBot);
            dashPath.lineTo(bounds.getRight(), dangerBot);
            float dashLengths[2] = { 6.0f, 4.0f };
            juce::PathStrokeType strokeType(1.0f);
            strokeType.createDashedStroke(dashPath, dashPath, dashLengths, 2);
            g.strokePath(dashPath, strokeType);
        }

        // Faint danger zone fill (area INSIDE the threshold = over-compressed)
        g.setColour(GoodMeterLookAndFeel::accentPink.withAlpha(0.04f));
        g.fillRect(bounds.getX(), dangerTop, bounds.getWidth(), dangerBot - dangerTop);
    }

    //==========================================================================
    /**
     * Mirror waveform: symmetrical closed path from PSR history
     * Top half = positive, bottom half = mirrored negative
     */
    void drawMirrorWaveform(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float centerY = bounds.getCentreY();
        const float halfH = bounds.getHeight() / 2.0f;
        const float stepX = bounds.getWidth() / static_cast<float>(historySize - 1);

        juce::Path psrPath;

        // Pass 1: top half (left to right)
        for (int i = 0; i < historySize; ++i)
        {
            float x = bounds.getX() + (i * stepX);
            float val = getHistoryAt(i);
            float mappedY = juce::jmap(juce::jmin(val, maxPsrDisplay), 0.0f, maxPsrDisplay, 0.0f, halfH);
            float yTop = centerY - mappedY;

            if (i == 0)
                psrPath.startNewSubPath(x, yTop);
            else
                psrPath.lineTo(x, yTop);
        }

        // Pass 2: bottom half (right to left, mirrored)
        for (int i = historySize - 1; i >= 0; --i)
        {
            float x = bounds.getX() + (i * stepX);
            float val = getHistoryAt(i);
            float mappedY = juce::jmap(juce::jmin(val, maxPsrDisplay), 0.0f, maxPsrDisplay, 0.0f, halfH);
            float yBottom = centerY + mappedY;

            psrPath.lineTo(x, yBottom);
        }

        psrPath.closeSubPath();

        // Fill: semi-transparent electro-cyan
        g.setColour(techCyan.withAlpha(0.25f));
        g.fillPath(psrPath);

        // Stroke: sharp laser edge
        g.setColour(techCyan);
        g.strokePath(psrPath, juce::PathStrokeType(1.0f));
    }

    //==========================================================================
    /**
     * Center reference line (PSR = 0)
     */
    void drawCenterLine(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float centerY = bounds.getCentreY();
        g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(0.25f));
        g.drawHorizontalLine(static_cast<int>(centerY), bounds.getX(), bounds.getRight());
    }

    //==========================================================================
    /**
     * Real-time PSR readout: right-aligned, red if below danger threshold
     */
    void drawReadout(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float fontSize = juce::jlimit(12.0f, 22.0f, bounds.getHeight() * 0.75f);
        const float labelFontSize = juce::jlimit(8.0f, 12.0f, bounds.getHeight() * 0.45f);

        // Label "PSR" on left
        g.setColour(GoodMeterLookAndFeel::textMuted);
        g.setFont(juce::Font(labelFontSize, juce::Font::bold));
        g.drawText("PEAK-TO-SHORT",
                   static_cast<int>(bounds.getX() + 4),
                   static_cast<int>(bounds.getY()),
                   static_cast<int>(bounds.getWidth() * 0.5f),
                   static_cast<int>(bounds.getHeight()),
                   juce::Justification::centredLeft, false);

        // Value on right — red warning if below threshold (use heavy-damped textPsr)
        bool isDanger = textPsr < dangerThreshold && textPsr > 0.1f;
        g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : techCyan);
        g.setFont(juce::Font(fontSize, juce::Font::bold));

        juce::String valueStr;
        if (textPsr < 0.1f)
            valueStr = "---";
        else
            valueStr = juce::String(textPsr, 1) + " dB";

        g.drawText(valueStr,
                   static_cast<int>(bounds.getX()),
                   static_cast<int>(bounds.getY()),
                   static_cast<int>(bounds.getWidth() - 4),
                   static_cast<int>(bounds.getHeight()),
                   juce::Justification::centredRight, false);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PsrMeterComponent)
};
