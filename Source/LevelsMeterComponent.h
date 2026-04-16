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
        startTimerHz(60);  // 60Hz visual refresh
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
    void setForceVerticalMiniLayout(bool shouldForce) { forceVerticalMiniLayout = shouldForce; }
    void setMarathonDarkStyle(bool shouldUse)
    {
        if (marathonDarkStyle == shouldUse)
            return;

        marathonDarkStyle = shouldUse;
        targetMenu.setColour(juce::ComboBox::backgroundColourId,
                             shouldUse ? juce::Colour(0xFF0B1018).interpolatedWith(GoodMeterLookAndFeel::accentPink, 0.06f).withAlpha(0.68f)
                                       : juce::Colour(0xFFF7F3EC).interpolatedWith(GoodMeterLookAndFeel::accentPink, 0.025f).withAlpha(0.78f));
        targetMenu.setColour(juce::ComboBox::outlineColourId,
                             shouldUse ? juce::Colour(0xFFF6EEE3).withAlpha(0.12f)
                                       : juce::Colour(0xFF1A1A24).withAlpha(0.08f));
        targetMenu.setColour(juce::ComboBox::textColourId,
                             shouldUse ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                                       : juce::Colour(0xFF1A1A24).withAlpha(0.94f));
        targetMenu.setColour(juce::ComboBox::arrowColourId,
                             shouldUse ? juce::Colour(0xFFF6EEE3).withAlpha(0.92f)
                                       : juce::Colour(0xFF1A1A24).withAlpha(0.92f));
        lufsTextCache = {};
        tickTextCache = {};
        repaint();
    }

    /**
     * Initialize the target menu (call after addAndMakeVisible)
     */
    void setupTargetMenu()
    {
        targetMenu.addItem("Streaming", 1);   // -14.0
        targetMenu.addItem("EBU R128", 2);    // -23.0
        targetMenu.addItem("ATSC A/85", 3);   // -24.0
        targetMenu.addItem("Netflix", 4);      // -27.0
        targetMenu.addItem("YouTube", 5);      // -14.0
        targetMenu.addItem("Douyin", 6);       // -14.0
        targetMenu.addItem("Bilibili", 7);     // -16.0
        targetMenu.setSelectedId(2, juce::dontSendNotification);

        // Invisible style — our LookAndFeel draws custom
        targetMenu.setJustificationType(juce::Justification::centredRight);
        targetMenu.setColour(juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        targetMenu.setColour(juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        targetMenu.setColour(juce::ComboBox::textColourId, GoodMeterLookAndFeel::textMain);
        targetMenu.setColour(juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);

        targetMenu.onChange = [this]() {
            switch (targetMenu.getSelectedId())
            {
                case 1: currentTargetLUFS = -14.0f; standard = "Streaming"; break;
                case 2: currentTargetLUFS = -23.0f; standard = "EBU R128"; break;
                case 3: currentTargetLUFS = -24.0f; standard = "ATSC A/85"; break;
                case 4: currentTargetLUFS = -27.0f; standard = "Netflix"; break;
                case 5: currentTargetLUFS = -14.0f; standard = "YouTube"; break;
                case 6: currentTargetLUFS = -14.0f; standard = "Douyin"; break;
                case 7: currentTargetLUFS = -16.0f; standard = "Bilibili"; break;
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
        const bool useVerticalLayout = shouldUseVerticalLayout(bounds);

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
        const float peakTextSmoothing = 0.03f;  // Extra slow for true peak
        textPeakL += (currentPeakL - textPeakL) * peakTextSmoothing;
        textPeakR += (currentPeakR - textPeakR) * peakTextSmoothing;
        textLUFS += (currentLUFS - textLUFS) * textSmoothing;
        textShortTerm += (currentShortTerm - textShortTerm) * textSmoothing;
        textIntegrated += (currentIntegrated - textIntegrated) * textSmoothing;
        textLURange += (currentLURange - textLURange) * 0.06f;

        // Quantized true peak display: only update when change > 0.4 dB
        if (std::abs(textPeakL - shownPeakL) > 0.4f) shownPeakL = textPeakL;
        if (std::abs(textPeakR - shownPeakR) > 0.4f) shownPeakR = textPeakR;

        // Smooth highlight transitions for true peak color (avoids flickering)
        float targetHL = (currentPeakL > -1.0f) ? 1.0f : 0.0f;
        float targetHR = (currentPeakR > -1.0f) ? 1.0f : 0.0f;
        peakHighlightL += (targetHL - peakHighlightL) * 0.06f;
        peakHighlightR += (targetHR - peakHighlightR) * 0.06f;

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

        // Pre-render text caches (moves drawText out of paint/CATransaction)
        {
            auto bounds = getLocalBounds();
            if (!bounds.isEmpty())
            {
                const bool useVerticalLayout = shouldUseVerticalLayout(bounds);
                int infoW, infoH;

                if (useVerticalLayout)
                {
                    // Vertical layout: bars on left, info on right
                    const int spacing = juce::jlimit(5, 12, static_cast<int>(bounds.getWidth() * 0.02f));
                    const int barsWidth = juce::jlimit(60, 160, static_cast<int>(bounds.getWidth() * 0.25f));
                    infoW = bounds.getWidth() - barsWidth - spacing;
                    infoH = bounds.getHeight();
                }
                else
                {
                    // Horizontal layout: bars on top, info below
                    const int totalHeight = bounds.getHeight();
                    const int barsHeight = static_cast<int>(totalHeight * 0.55f);
                    const int spacing = 10;
                    infoW = bounds.getWidth();
                    infoH = totalHeight - barsHeight - spacing;
                }

                prerenderLUFSText(infoW, infoH);

                // Tick labels: compute bar width same as drawPeakBars
                if (!useVerticalLayout)
                {
                    auto barArea = bounds.reduced(20, 0).withTrimmedTop(16);
                    prerenderTickText(barArea.getWidth());
                }
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

    void setStandardById(int standardId)
    {
        targetMenu.setSelectedId(standardId, juce::sendNotificationSync);
    }

    int getStandardId() const
    {
        return targetMenu.getSelectedId();
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;
    bool forceVerticalMiniLayout = false;
    bool marathonDarkStyle = false;

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

    // Quantized display values for true peak (only update when delta > threshold)
    float shownPeakL = -90.0f;
    float shownPeakR = -90.0f;
    // Smoothed highlight state for true peak color (avoids rapid color flickering)
    float peakHighlightL = 0.0f;
    float peakHighlightR = 0.0f;

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

    // === Offscreen text cache (zero drawText in paint!) ===
    juce::Image lufsTextCache;
    juce::Image tickTextCache;
    int lastTickWidth = 0;
    int lastTickHeight = 0;
    float lastTickScale = 0.0f;
    int lastLufsWidth = 0;
    int lastLufsHeight = 0;
    float lastLufsScale = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // Peak hold decay is handled in updateMetrics()
        // This timer just ensures smooth repaints
    }

    //==========================================================================
    bool shouldUseVerticalLayout(juce::Rectangle<int> bounds) const
    {
        if (forceVerticalMiniLayout)
            return true;

        const bool mobileCharts = GoodMeterLookAndFeel::isMobileCharts();
        const int heightThreshold = mobileCharts ? 148 : 140;
        const int widthThreshold = mobileCharts ? 210 : 0;
        return bounds.getHeight() < heightThreshold
            || (mobileCharts && bounds.getWidth() < widthThreshold);
    }

    juce::Colour dataPlateFill() const
    {
        return marathonDarkStyle ? juce::Colour(0xFF0A0D13)
                                 : juce::Colour(0xFFEAEAEA);
    }

    juce::Colour dataPlateBorder() const
    {
        return marathonDarkStyle ? juce::Colour(0xFFF3EFE7).withAlpha(0.78f)
                                 : GoodMeterLookAndFeel::border;
    }

    juce::Colour dataGridInk(float alpha) const
    {
        return marathonDarkStyle ? juce::Colour(0xFFF3EFE7).withAlpha(alpha)
                                 : GoodMeterLookAndFeel::chartInk(alpha);
    }

    juce::Colour dataMutedText(float alpha = 1.0f) const
    {
        if (!marathonDarkStyle)
            return alpha >= 0.999f ? GoodMeterLookAndFeel::chartMuted()
                                   : GoodMeterLookAndFeel::chartMuted(alpha);

        return juce::Colour(0xFF8D919C).withAlpha(alpha);
    }

    juce::Colour dataValueText(float highlightAmount) const
    {
        auto base = marathonDarkStyle ? juce::Colour(0xFFF3EFE7)
                                      : GoodMeterLookAndFeel::textMain;
        auto accent = marathonDarkStyle ? GoodMeterLookAndFeel::accentPink.brighter(0.15f)
                                        : GoodMeterLookAndFeel::accentPink;
        return base.interpolatedWith(accent, juce::jlimit(0.0f, 1.0f, highlightAmount));
    }

    //==========================================================================
    /**
     * Convert dB value to pixel X position (Levels.tsx lines 91-94)
     */
    float dbToX(float db, float width) const
    {
        const float clamped = juce::jlimit(minDb, maxDb, db);
        const float linearNorm = (clamped - minDb) / (maxDb - minDb);
        // Perceptual pseudo-log: sqrt expands high-level detail
        return std::pow(linearNorm, 0.5f) * width;
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
        g.setColour(dataPlateFill());
        g.fillRect(b);

        // Border (Levels.tsx line 83-86)
        g.setColour(dataPlateBorder());
        g.drawRect(b, GoodMeterLookAndFeel::chartStroke(2.0f, 1.18f, 2.4f));

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
        g.setColour(marathonDarkStyle ? juce::Colour(0xFFF4F0E8)
                                      : GoodMeterLookAndFeel::border);
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

            if (!GoodMeterLookAndFeel::isMobileCharts() && glowIntensity > 0.01f)
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
            juce::PathStrokeType strokeType(GoodMeterLookAndFeel::chartStroke(4.0f, 1.15f, 4.5f));
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
        const bool useTopTicksOnCompactMobile = GoodMeterLookAndFeel::isMobileCharts()
                                             && bounds.getHeight() <= 110;
        const int tickLabelHeight = useTopTicksOnCompactMobile ? 14 : 0;
        const int topPadding = useTopTicksOnCompactMobile ? 30 : 16;

        auto area = bounds.reduced(20, 0).withTrimmedTop(topPadding);

        // 确保两根柱子等高: 总高减去间隙后平分
        int bh = juce::jmin(barHeight, (area.getHeight() - barGap) / 2);

        auto barL = area.removeFromTop(bh);
        drawPeakBar(g, barL, displayPeakL, peakHoldL);

        area.removeFromTop(barGap);

        auto barR = area.removeFromTop(bh);
        drawPeakBar(g, barR, displayPeakR, peakHoldR);

        // Draw scale ticks (Levels.tsx lines 154-161)
        g.setColour(dataGridInk(marathonDarkStyle ? 0.14f : 0.16f));

        float lineTop = static_cast<float>(barL.getY());
        float lineBottom = static_cast<float>(barR.getBottom() + (useTopTicksOnCompactMobile ? 0 : 4));

        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            float x = static_cast<float>(barL.getX()) + dbToX(static_cast<float>(db), static_cast<float>(barL.getWidth()));
            g.drawVerticalLine(static_cast<int>(x), lineTop, lineBottom);
        }

        const int tickTextY = useTopTicksOnCompactMobile
                            ? barL.getY() - tickLabelHeight - 2
                            : static_cast<int>(lineBottom + 2.0f);

        if (GoodMeterLookAndFeel::preferDirectChartText() || useTopTicksOnCompactMobile)
        {
            g.setFont(GoodMeterLookAndFeel::chartFont(10.0f));
            for (int db : tickDbs)
            {
                float x = static_cast<float>(barL.getX()) + dbToX(static_cast<float>(db), static_cast<float>(barL.getWidth()));
                g.setColour(dataMutedText(0.94f));
                g.drawText(juce::String(db),
                           static_cast<int>(x - 15), tickTextY,
                           30, 12,
                           juce::Justification::centred, false);
            }
        }
        else if (!tickTextCache.isNull())
        {
            int tickY = tickTextY;
            g.drawImage(tickTextCache,
                        barL.getX(), tickY,
                        barL.getWidth(), 14,
                        0, 0,
                        tickTextCache.getWidth(), tickTextCache.getHeight());
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
            g.setFont(GoodMeterLookAndFeel::chartFont(juce::jlimit(7.0f, 10.0f, tickLabelH * 0.7f)));
        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            float norm = (static_cast<float>(db) - minDb) / (maxDb - minDb);
            float tickY = barL.getBottom() - norm * barHeight;

            // Tick line across both bars
            g.setColour(dataGridInk(marathonDarkStyle ? 0.16f : 0.2f));
            g.drawHorizontalLine(static_cast<int>(tickY),
                                barL.getX(), barR.getRight());

            // Label to the right of bars
            g.setColour(dataMutedText());
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
        g.setColour(dataPlateFill());
        g.fillRect(barBounds);

        // Border
        g.setColour(dataPlateBorder());
        g.drawRect(barBounds, GoodMeterLookAndFeel::chartStroke(1.5f, 1.2f, 1.9f));

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
            g.setColour(marathonDarkStyle ? juce::Colour(0xFFF4F0E8)
                                          : GoodMeterLookAndFeel::border);
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

            if (!GoodMeterLookAndFeel::isMobileCharts() && glowIntensity > 0.01f)
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
            juce::PathStrokeType strokeType(GoodMeterLookAndFeel::chartStroke(4.0f, 1.15f, 4.5f));
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
        g.setColour(dataPlateFill());
        g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

        // Border
        g.setColour(dataPlateBorder());
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 4.0f, 2.0f);

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            juce::Graphics::ScopedSaveState state(g);
            g.addTransform(juce::AffineTransform::translation(static_cast<float>(bounds.getX()),
                                                              static_cast<float>(bounds.getY())));
            renderLUFSText(g, bounds.getWidth(), bounds.getHeight());
        }
        else if (!lufsTextCache.isNull())
            g.drawImage(lufsTextCache,
                        bounds.getX(), bounds.getY(),
                        bounds.getWidth(), bounds.getHeight(),
                        0, 0,
                        lufsTextCache.getWidth(), lufsTextCache.getHeight());
    }

    //==========================================================================
    /**
     * Pre-render LUFS text to offscreen image (called from updateMetrics, NOT paint)
     * This moves ALL drawText cost out of CATransaction::commit
     */
    void prerenderLUFSText(int w, int h)
    {
        if (w < 30 || h < 20) return;

        const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

        if (lufsTextCache.isNull() || lastLufsWidth != w || lastLufsHeight != h
            || std::abs(lastLufsScale - scale) > 0.01f)
        {
            lufsTextCache = juce::Image(juce::Image::ARGB,
                                        juce::jmax(1, juce::roundToInt(static_cast<float>(w) * scale)),
                                        juce::jmax(1, juce::roundToInt(static_cast<float>(h) * scale)),
                                        true, juce::SoftwareImageType());
            lastLufsWidth = w;
            lastLufsHeight = h;
            lastLufsScale = scale;
        }

        lufsTextCache.clear(lufsTextCache.getBounds());
        juce::Graphics tg(lufsTextCache);
        tg.addTransform(juce::AffineTransform::scale(scale));
        renderLUFSText(tg, w, h);
    }

    /**
     * Pre-render tick labels to offscreen image (called from updateMetrics)
     * Tick labels are STATIC — only need rebuild on resize
     */
    void prerenderTickText(int barWidth)
    {
        if (barWidth < 10) return;
        const float scale = juce::Component::getApproximateScaleFactorForComponent(this);
        if (!tickTextCache.isNull() && lastTickWidth == barWidth
            && std::abs(lastTickScale - scale) <= 0.01f) return;

        lastTickWidth = barWidth;
        lastTickHeight = 14;
        lastTickScale = scale;
        tickTextCache = juce::Image(juce::Image::ARGB,
                                    juce::jmax(1, juce::roundToInt(static_cast<float>(barWidth) * scale)),
                                    juce::jmax(1, juce::roundToInt(static_cast<float>(lastTickHeight) * scale)),
                                    true, juce::SoftwareImageType());
        juce::Graphics tg(tickTextCache);
        tg.addTransform(juce::AffineTransform::scale(scale));
        tg.setFont(GoodMeterLookAndFeel::chartFont(10.0f));

        const int tickDbs[] = { -60, -40, -20, -10, -6, -3, 0 };
        for (int db : tickDbs)
        {
            float x = dbToX(static_cast<float>(db), static_cast<float>(barWidth));
            tg.setColour(dataMutedText());
            tg.drawText(juce::String(db),
                        static_cast<int>(x - 15), 0, 30, 12,
                        juce::Justification::centred, false);
        }
    }

    void renderLUFSText(juce::Graphics& g, int boundsW, int boundsH)
    {
        const int padX = juce::jlimit(4, 16, static_cast<int>(boundsW * 0.02f));
        const int padY = juce::jlimit(3, 12, static_cast<int>(boundsH * 0.05f));
        auto gridBounds = juce::Rectangle<int>(0, 0, boundsW, boundsH).reduced(padX, padY);

        // Adaptive grid: tall panel (left-right mode) → 2 cols × 3 rows
        //                wide panel (top-bottom mode) → 3 cols × 2 rows
        const bool useTallGrid = (boundsH > boundsW * 0.6f);
        const int numCols = useTallGrid ? 2 : 3;
        const int numRows = useTallGrid ? 3 : 2;

        const int colWidth = gridBounds.getWidth() / numCols;
        const int rowHeight = gridBounds.getHeight() / numRows;

        const float valueFontByH = static_cast<float>(rowHeight) * 0.38f;
        const float valueFontByW = static_cast<float>(colWidth) * 0.35f;
        const float valueFontSize = GoodMeterLookAndFeel::chartFont(
            juce::jlimit(13.0f, 22.0f, juce::jmin(valueFontByH, valueFontByW)));

        const float labelFontByH = static_cast<float>(rowHeight) * 0.22f;
        const float labelFontByW = static_cast<float>(colWidth) * 0.14f;
        const float labelFontSize = GoodMeterLookAndFeel::chartFont(
            juce::jlimit(8.0f, 13.0f, juce::jmin(labelFontByH, labelFontByW)));

        const float labelRatio = (colWidth < 120) ? 0.25f : 0.4f;
        const bool showUnit = colWidth > 180;

        juce::Font labelFont(labelFontSize, juce::Font::bold);
        juce::Font valueFont(valueFontSize, juce::Font::bold);

        auto drawMetric = [&](int col, int row, const juce::String& label, float value, const juce::String& unit, float highlightAmount)
        {
            auto cellBounds = juce::Rectangle<int>(
                gridBounds.getX() + col * colWidth,
                gridBounds.getY() + row * rowHeight,
                colWidth,
                rowHeight
            );

            const int cellPad = juce::jlimit(1, 6, static_cast<int>(rowHeight * 0.06f));
            cellBounds = cellBounds.reduced(0, cellPad);

            auto labelArea = cellBounds.removeFromLeft(static_cast<int>(cellBounds.getWidth() * labelRatio));
            auto valueArea = cellBounds;

            g.setColour(dataMutedText(0.92f));
            g.setFont(labelFont);
            g.drawText(label, labelArea,
                      juce::Justification::centredLeft, false);

            juce::String valueStr;
            if (value <= -60.0f)
                valueStr = juce::String(juce::CharPointer_UTF8(u8"\u2013\u221e"));
            else if (valueFontSize < 11.0f)
                valueStr = juce::String(static_cast<int>(std::round(value)));
            else
                valueStr = juce::String(value, 1);

            if (showUnit) valueStr += " " + unit;

            g.setColour(dataValueText(highlightAmount));
            g.setFont(valueFont);
            g.drawText(valueStr, valueArea,
                      juce::Justification::centredLeft, false);
        };

        if (useTallGrid)
        {
            // 2 cols × 3 rows layout:
            // Row 0: momentary | short-term
            // Row 1: integrated | lu range
            // Row 2: true peak l | true peak r
            drawMetric(0, 0, "Momentary", textLUFS, "LUFS", currentLUFS > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(1, 0, "Short-T", textShortTerm, "LUFS", currentShortTerm > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(0, 1, "Integrated", textIntegrated, "LUFS", currentIntegrated > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(1, 1, "LU Range", textLURange, "LU", 0.0f);
            drawMetric(0, 2, "True Pk L", shownPeakL, "dBTP", peakHighlightL);
            drawMetric(1, 2, "True Pk R", shownPeakR, "dBTP", peakHighlightR);
        }
        else
        {
            // 3 cols × 2 rows layout (original):
            drawMetric(0, 0, "Momentary", textLUFS, "LUFS", currentLUFS > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(0, 1, "True Peak L", shownPeakL, "dBTP", peakHighlightL);
            drawMetric(1, 0, "Short-Term", textShortTerm, "LUFS", currentShortTerm > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(1, 1, "True Peak R", shownPeakR, "dBTP", peakHighlightR);
            drawMetric(2, 0, "Integrated", textIntegrated, "LUFS", currentIntegrated > currentTargetLUFS ? 1.0f : 0.0f);
            drawMetric(2, 1, "LU Range", textLURange, "LU", 0.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelsMeterComponent)
};
