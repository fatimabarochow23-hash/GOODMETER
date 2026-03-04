/*
  ==============================================================================
    LevelsMeterComponent.h
    GOODMETER - Peak and LUFS Level Meters

    Translated from Levels.tsx
    Features: Peak bars with gradient, peak hold indicators, LUFS readout
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class LevelsMeterComponent : public juce::Component,
                             public juce::Timer
{
public:
    //==========================================================================
    LevelsMeterComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        setSize(100, 200);
        startTimer(16);  // ~60Hz
    }

    ~LevelsMeterComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    /**
     * Get the target ComboBox so MeterCard can embed it in the header.
     * Ownership stays with this component — card just positions it.
     */
    juce::ComboBox& getTargetMenu() { return targetMenu; }

    /**
     * Initialize the target menu (call after addAndMakeVisible)
     */
    void setupTargetMenu()
    {
        targetMenu.addItem("Streaming", 1);   // -14.0
        targetMenu.addItem("EBU R128", 2);    // -23.0
        targetMenu.addItem("ATSC A/85", 3);   // -24.0
        targetMenu.addItem("Netflix", 4);      // -27.0
        targetMenu.setSelectedId(2, juce::dontSendNotification);

        // Invisible style — our LookAndFeel draws custom
        targetMenu.setJustificationType(juce::Justification::centredRight);
        targetMenu.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        targetMenu.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        targetMenu.setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);

        targetMenu.onChange = [this]() {
            switch (targetMenu.getSelectedId())
            {
                case 1: currentTargetLUFS = -14.0f; standard = "Streaming"; break;
                case 2: currentTargetLUFS = -23.0f; standard = "EBU R128"; break;
                case 3: currentTargetLUFS = -24.0f; standard = "ATSC A/85"; break;
                case 4: currentTargetLUFS = -27.0f; standard = "Netflix"; break;
                default: break;
            }
            repaint();
        };
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Safety check
        if (bounds.isEmpty())
            return;

        // Layout mode threshold: horizontal bars need ~120px minimum
        // (16px top pad + 28px barL + 12px gap + 28px barR + 14px tick labels + spacing + info panel)
        const bool useVerticalLayout = bounds.getHeight() < 140;

        if (useVerticalLayout)
        {
            // === Vertical layout: bars stand up on left, info panel on right ===
            const int spacing = juce::jlimit(5, 12, static_cast<int>(bounds.getWidth() * 0.02f));
            const int barsWidth = juce::jlimit(60, 160, static_cast<int>(bounds.getWidth() * 0.25f));

            auto barsBounds = bounds.removeFromLeft(barsWidth);
            bounds.removeFromLeft(spacing);
            auto infoBounds = bounds;

            drawVerticalPeakBars(g, barsBounds);
            drawLUFSInfo(g, infoBounds);
        }
        else
        {
            // === Horizontal layout: bars on top, info below (original) ===
            const int totalHeight = bounds.getHeight();
            const int barsHeight = static_cast<int>(totalHeight * 0.55f);
            const int spacing = 10;

            auto barsBounds = bounds.removeFromTop(barsHeight);
            drawPeakBars(g, barsBounds);

            bounds.removeFromTop(spacing);
            auto infoBounds = bounds;
            drawLUFSInfo(g, infoBounds);
        }
    }

    void resized() override
    {
        // No child components to layout
    }

    //==========================================================================
    /**
     * Update meter values from processor (called from PluginEditor::timerCallback)
     */
    void updateMetrics(float peakL_dB, float peakR_dB, float momentaryLUFS,
                       float shortTermLUFS, float integratedLUFS, float luRangeVal)
    {
        currentPeakL = peakL_dB;
        currentPeakR = peakR_dB;
        currentLUFS = momentaryLUFS;
        currentShortTerm = shortTermLUFS;
        currentIntegrated = integratedLUFS;
        currentLURange = luRangeVal;

        // Smooth interpolation (fast — for bar/graph animation)
        displayPeakL += (currentPeakL - displayPeakL) * 0.3f;
        displayPeakR += (currentPeakR - displayPeakR) * 0.3f;
        displayLUFS += (currentLUFS - displayLUFS) * 0.3f;
        displayShortTerm += (currentShortTerm - displayShortTerm) * 0.3f;
        displayIntegrated += (currentIntegrated - displayIntegrated) * 0.3f;
        displayLURange += (currentLURange - displayLURange) * 0.2f;

        // Heavy-damped interpolation (slow — for stable numeric text readout)
        const float textSmoothing = 0.08f;
        textPeakL += (currentPeakL - textPeakL) * textSmoothing;
        textPeakR += (currentPeakR - textPeakR) * textSmoothing;
        textLUFS += (currentLUFS - textLUFS) * textSmoothing;
        textShortTerm += (currentShortTerm - textShortTerm) * textSmoothing;
        textIntegrated += (currentIntegrated - textIntegrated) * textSmoothing;
        textLURange += (currentLURange - textLURange) * 0.06f;

        // Update peak holds (logic from Levels.tsx lines 41-56)
        auto now = juce::Time::getMillisecondCounterHiRes();

        if (currentPeakL > peakHoldL || (now - peakHoldTimeL) > 1000.0)
        {
            if (currentPeakL > peakHoldL)
            {
                peakHoldL = currentPeakL;
                peakHoldTimeL = now;
            }
            else
            {
                peakHoldL -= 0.5f;  // Decay
            }
        }

        if (currentPeakR > peakHoldR || (now - peakHoldTimeR) > 1000.0)
        {
            if (currentPeakR > peakHoldR)
            {
                peakHoldR = currentPeakR;
                peakHoldTimeR = now;
            }
            else
            {
                peakHoldR -= 0.5f;  // Decay
            }
        }

        repaint();
    }

    /**
     * Set loudness standard for target reference line
     */
    void setStandard(const juce::String& standardName)
    {
        standard = standardName;
        repaint();
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // Target menu (ComboBox, positioned by MeterCard in header)
    juce::ComboBox targetMenu;
    float currentTargetLUFS = -23.0f;  // EBU R128 default

    // Current values (updated every frame from processor)
    float currentPeakL = -90.0f;
    float currentPeakR = -90.0f;
    float currentLUFS = -70.0f;
    float currentShortTerm = -70.0f;
    float currentIntegrated = -70.0f;
    float currentLURange = 0.0f;

    // Smooth display values (fast lerp for bar animation)
    float displayPeakL = -90.0f;
    float displayPeakR = -90.0f;
    float displayLUFS = -70.0f;
    float displayShortTerm = -70.0f;
    float displayIntegrated = -70.0f;
    float displayLURange = 0.0f;

    // Heavy-damped text display values (slow lerp for stable numeric readout)
    float textPeakL = -90.0f;
    float textPeakR = -90.0f;
    float textLUFS = -70.0f;
    float textShortTerm = -70.0f;
    float textIntegrated = -70.0f;
    float textLURange = 0.0f;

    // Peak hold state (Levels.tsx lines 25-28)
    float peakHoldL = -60.0f;
    float peakHoldR = -60.0f;
    double peakHoldTimeL = 0.0;
    double peakHoldTimeR = 0.0;

    // Loudness standard
    juce::String standard = "EBU R128";

    // Constants (from Levels.tsx)
    static constexpr float minDb = -60.0f;
    static constexpr float maxDb = 0.0f;
    static constexpr int barHeight = 28;
    static constexpr int barGap = 12;

    //==========================================================================
    void timerCallback() override
    {
        // Peak hold decay is handled in updateMetrics()
        // This timer just ensures smooth repaints
    }

    //==========================================================================
    /**
     * Convert dB value to pixel X position (Levels.tsx lines 91-94)
     */
    float dbToX(float db, float width) const
    {
        const float clamped = juce::jlimit(minDb, maxDb, db);
        return ((clamped - minDb) / (maxDb - minDb)) * width;
    }

    //==========================================================================
    /**
     * Draw a single peak bar (Levels.tsx lines 76-129)
     */
    void drawPeakBar(juce::Graphics& g,
                     const juce::Rectangle<int>& bounds,
                     float currentPeak,
                     float holdPeak)
    {
        auto b = bounds.toFloat();
        const float width = b.getWidth();

        // Background (Levels.tsx line 80-81)
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRect(b);

        // Border (Levels.tsx line 83-86)
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(b, 2.0f);

        // Calculate X positions
        const float currentX = dbToX(currentPeak, width);
        const float holdX = dbToX(holdPeak, width);

        // Gradient fill (Levels.tsx lines 98-108)
        juce::ColourGradient gradient(
            GoodMeterLookAndFeel::accentGreen,  // #00D084
            0.0f, b.getCentreY(),
            GoodMeterLookAndFeel::accentPink,   // #E6335F
            width, b.getCentreY(),
            false
        );

        // Add color stops (green → yellow → red)
        gradient.addColour(dbToX(-18.0f, width) / width, GoodMeterLookAndFeel::accentGreen);
        gradient.addColour(dbToX(-18.0f, width) / width, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(dbToX(-6.0f, width) / width, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(dbToX(-6.0f, width) / width, GoodMeterLookAndFeel::accentPink);

        g.setGradientFill(gradient);
        g.fillRect(b.withWidth(currentX));

        // Peak hold line (恢复原始黑色)
        g.setColour(GoodMeterLookAndFeel::border);
        g.fillRect(b.withX(holdX).withWidth(4.0f));

        // Target loudness reference — 荧光橙 + 过载脉冲发光
        {
            const float targetX = dbToX(currentTargetLUFS, width);
            float peakNorm = juce::jlimit(0.0f, 1.0f, (currentPeak - minDb) / (maxDb - minDb));

            juce::Colour baseOrange(0xFFFF6600);
            float overloadFactor = juce::jlimit(0.0f, 1.0f, (peakNorm - 0.85f) * 6.0f);
            float flashPulse = 0.5f + 0.5f * std::abs(std::sin(
                juce::Time::getMillisecondCounterHiRes() * 0.025));
            float glowIntensity = overloadFactor * flashPulse;

            auto targetBounds = juce::Rectangle<float>(targetX - 2.0f, b.getY(), 4.0f, b.getHeight());

            if (glowIntensity > 0.01f)
            {
                g.setColour(baseOrange.withAlpha(glowIntensity * 0.6f));
                g.fillRoundedRectangle(targetBounds.expanded(3.0f + 6.0f * glowIntensity), 2.0f);
            }

            juce::Colour coreColour = baseOrange.interpolatedWith(juce::Colours::white, glowIntensity * 0.5f);
            g.setColour(coreColour);

            juce::Path dashPath;
            dashPath.startNewSubPath(targetX, b.getY());
            dashPath.lineTo(targetX, b.getBottom());
            float dashLengths[2] = { 8.0f, 8.0f };
            juce::PathStrokeType strokeType(4.0f);
            strokeType.createDashedStroke(dashPath, dashPath, dashLengths, 2);
            g.strokePath(dashPath, strokeType);
        }
    }

    //==========================================================================
    /**
     * Draw both peak bars and scale ticks (Levels.tsx lines 131-162)
     */
    void drawPeakBars(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        auto area = bounds.reduced(20, 0).withTrimmedTop(16);

        // 确保两根柱子等高: 总高减去间隙后平分
        int bh = juce::jmin(barHeight, (area.getHeight() - barGap) / 2);

        auto barL = area.removeFromTop(bh);
        drawPeakBar(g, barL, displayPeakL, peakHoldL);

        area.removeFromTop(barGap);

        auto barR = area.removeFromTop(bh);
        drawPeakBar(g, barR, displayPeakR, peakHoldR);

        // Draw scale ticks (Levels.tsx lines 154-161)
        g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.1f));
        g.setFont(10.0f);

        // ✅ 获取准确的上下边界（相对于 area）
        float lineTop = static_cast<float>(barL.getY());
        float lineBottom = static_cast<float>(barR.getBottom() + 4);

        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            // ✅ 使用 area 的宽度和 X 起点，而非原始 bounds
            float x = static_cast<float>(barL.getX()) + dbToX(static_cast<float>(db), static_cast<float>(barL.getWidth()));

            // Tick line (从 L 通道顶部画到 R 通道底部)
            g.drawVerticalLine(static_cast<int>(x), lineTop, lineBottom);

            // Label (贴在竖线底部)
            juce::String label = juce::String(db);
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.drawText(label,
                      static_cast<int>(x - 15), static_cast<int>(lineBottom + 2),
                      30, 12,
                      juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * Draw vertical peak bars (rotated 90°, bottom = -60dB, top = 0dB)
     * Used in compact vertical layout mode
     */
    void drawVerticalPeakBars(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        auto area = bounds.toFloat().reduced(8.0f, 10.0f);

        if (area.getWidth() < 10.0f || area.getHeight() < 20.0f)
            return;

        // Two vertical bars side by side with gap
        const float barW = juce::jmin(20.0f, (area.getWidth() - 8.0f) / 2.0f);
        const float gap = juce::jlimit(4.0f, 12.0f, area.getWidth() * 0.08f);
        const float totalBarsW = barW * 2.0f + gap;
        const float startX = area.getCentreX() - totalBarsW / 2.0f;

        // Reserve space for tick labels at bottom
        const float tickLabelH = juce::jlimit(10.0f, 16.0f, area.getHeight() * 0.08f);
        const float barHeight = area.getHeight() - tickLabelH;

        auto barL = juce::Rectangle<float>(startX, area.getY(), barW, barHeight);
        auto barR = juce::Rectangle<float>(startX + barW + gap, area.getY(), barW, barHeight);

        drawVerticalBar(g, barL, displayPeakL, peakHoldL);
        drawVerticalBar(g, barR, displayPeakR, peakHoldR);

        // Draw horizontal tick marks and labels
        g.setFont(juce::jlimit(7.0f, 10.0f, tickLabelH * 0.7f));
        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            float norm = (static_cast<float>(db) - minDb) / (maxDb - minDb);
            float tickY = barL.getBottom() - norm * barHeight;

            // Tick line across both bars
            g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.15f));
            g.drawHorizontalLine(static_cast<int>(tickY),
                                barL.getX(), barR.getRight());

            // Label to the right of bars
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.drawText(juce::String(db),
                      static_cast<int>(barR.getRight() + 2),
                      static_cast<int>(tickY - 5),
                      30, 10,
                      juce::Justification::centredLeft, false);
        }
    }

    //==========================================================================
    /**
     * Draw a single vertical bar (bottom-up fill)
     */
    void drawVerticalBar(juce::Graphics& g,
                         const juce::Rectangle<float>& barBounds,
                         float currentPeak, float holdPeak)
    {
        const float h = barBounds.getHeight();
        const float w = barBounds.getWidth();

        // Background
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRect(barBounds);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(barBounds, 1.5f);

        // Fill height (bottom-up)
        float currentNorm = juce::jlimit(0.0f, 1.0f, (currentPeak - minDb) / (maxDb - minDb));
        float fillH = currentNorm * h;

        // Vertical gradient (green at bottom → yellow → pink at top)
        juce::ColourGradient gradient(
            GoodMeterLookAndFeel::accentGreen,
            barBounds.getCentreX(), barBounds.getBottom(),
            GoodMeterLookAndFeel::accentPink,
            barBounds.getCentreX(), barBounds.getY(),
            false
        );

        float yellowStart = (-18.0f - minDb) / (maxDb - minDb);
        float yellowEnd = (-6.0f - minDb) / (maxDb - minDb);
        gradient.addColour(yellowStart, GoodMeterLookAndFeel::accentGreen);
        gradient.addColour(yellowStart, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(yellowEnd, GoodMeterLookAndFeel::accentYellow);
        gradient.addColour(yellowEnd, GoodMeterLookAndFeel::accentPink);

        g.setGradientFill(gradient);
        g.fillRect(barBounds.getX(), barBounds.getBottom() - fillH, w, fillH);

        // Peak hold marker (恢复原始黑色)
        {
            float holdNorm = juce::jlimit(0.0f, 1.0f, (holdPeak - minDb) / (maxDb - minDb));
            float holdY = barBounds.getBottom() - holdNorm * h;
            g.setColour(GoodMeterLookAndFeel::border);
            g.fillRect(barBounds.getX(), holdY - 1.5f, w, 3.0f);
        }

        // Target loudness reference — 荧光橙 + 过载脉冲发光
        {
            float targetNorm = juce::jlimit(0.0f, 1.0f, (currentTargetLUFS - minDb) / (maxDb - minDb));
            float targetY = barBounds.getBottom() - targetNorm * h;
            float peakNorm = juce::jlimit(0.0f, 1.0f, (currentPeak - minDb) / (maxDb - minDb));

            juce::Colour baseOrange(0xFFFF6600);
            float overloadFactor = juce::jlimit(0.0f, 1.0f, (peakNorm - 0.85f) * 6.0f);
            float flashPulse = 0.5f + 0.5f * std::abs(std::sin(
                juce::Time::getMillisecondCounterHiRes() * 0.025));
            float glowIntensity = overloadFactor * flashPulse;

            auto targetBounds = juce::Rectangle<float>(barBounds.getX(), targetY - 2.0f, w, 4.0f);

            if (glowIntensity > 0.01f)
            {
                g.setColour(baseOrange.withAlpha(glowIntensity * 0.6f));
                g.fillRoundedRectangle(targetBounds.expanded(3.0f + 6.0f * glowIntensity), 2.0f);
            }

            juce::Colour coreColour = baseOrange.interpolatedWith(juce::Colours::white, glowIntensity * 0.5f);
            g.setColour(coreColour);

            juce::Path dashPath;
            dashPath.startNewSubPath(barBounds.getX(), targetY);
            dashPath.lineTo(barBounds.getRight(), targetY);
            float dashLengths[2] = { 4.0f, 4.0f };
            juce::PathStrokeType strokeType(4.0f);
            strokeType.createDashedStroke(dashPath, dashPath, dashLengths, 2);
            g.strokePath(dashPath, strokeType);
        }
    }

    //==========================================================================
    /**
     * Draw LUFS info grid - adapts font size to available space
     */
    void drawLUFSInfo(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        if (bounds.getWidth() < 30 || bounds.getHeight() < 20)
            return;

        // Background box
        g.setColour(juce::Colour(0xFFEAEAEA));
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, 2.0f);

        // Grid padding proportional
        const int padX = juce::jlimit(4, 16, static_cast<int>(bounds.getWidth() * 0.02f));
        const int padY = juce::jlimit(3, 12, static_cast<int>(bounds.getHeight() * 0.05f));
        auto gridBounds = bounds.reduced(padX, padY);
        const int colWidth = gridBounds.getWidth() / 3;

        // Font sizes scale with BOTH width and height (take the smaller constraint)
        const float valueFontByH = bounds.getHeight() * 0.18f;
        const float valueFontByW = static_cast<float>(colWidth) * 0.35f;
        const float valueFontSize = juce::jlimit(13.0f, 22.0f, juce::jmin(valueFontByH, valueFontByW));

        const float labelFontByH = bounds.getHeight() * 0.1f;
        const float labelFontByW = static_cast<float>(colWidth) * 0.12f;
        const float labelFontSize = juce::jlimit(6.0f, 12.0f, juce::jmin(labelFontByH, labelFontByW));

        // Narrow panel: shrink label ratio to give more space to numbers
        const float labelRatio = (colWidth < 120) ? 0.25f : 0.4f;

        // Hide units when too narrow
        const bool showUnit = colWidth > 180;

        auto drawMetric = [&](int col, int row, const juce::String& label, float value, const juce::String& unit, bool highlight = false)
        {
            auto colBounds = juce::Rectangle<int>(
                gridBounds.getX() + col * colWidth,
                gridBounds.getY(),
                colWidth,
                gridBounds.getHeight()
            );

            juce::Rectangle<int> cellBounds;
            const int cellPad = juce::jlimit(1, 6, static_cast<int>(gridBounds.getHeight() * 0.03f));
            if (row == 0)
                cellBounds = colBounds.removeFromTop(colBounds.getHeight() / 2).reduced(0, cellPad);
            else
                cellBounds = colBounds.removeFromBottom(colBounds.getHeight() / 2).reduced(0, cellPad);

            // Label/value split (adaptive ratio)
            auto labelArea = cellBounds.removeFromLeft(static_cast<int>(cellBounds.getWidth() * labelRatio));
            auto valueArea = cellBounds;

            // Label (abbreviated when narrow)
            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.setFont(juce::Font(labelFontSize, juce::Font::bold));
            g.drawText(label.toLowerCase(), labelArea,
                      juce::Justification::centredLeft, false);

            // Value - scale decimal places based on available width
            juce::String valueStr;
            if (value <= -60.0f)
                valueStr = juce::String(juce::CharPointer_UTF8(u8"\u2013\u221e"));
            else if (valueFontSize < 11.0f)
                valueStr = juce::String(static_cast<int>(std::round(value)));  // No decimals when tiny
            else
                valueStr = juce::String(value, 1);

            if (showUnit) valueStr += " " + unit;

            g.setColour(highlight ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(valueFontSize, juce::Font::bold));
            g.drawText(valueStr, valueArea,
                      juce::Justification::centredLeft, false);
        };

        drawMetric(0, 0, "momentary", textLUFS, "LUFS", currentLUFS > currentTargetLUFS);
        drawMetric(0, 1, "true peak l", textPeakL, "dBTP", currentPeakL > -1.0f);
        drawMetric(1, 0, "short-term", textShortTerm, "LUFS", currentShortTerm > currentTargetLUFS);
        drawMetric(1, 1, "true peak r", textPeakR, "dBTP", currentPeakR > -1.0f);
        drawMetric(2, 0, "integrated", textIntegrated, "LUFS", currentIntegrated > currentTargetLUFS);
        drawMetric(2, 1, "lu range", textLURange, "LU");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelsMeterComponent)
};
