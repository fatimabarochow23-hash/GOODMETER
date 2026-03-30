/*
  ==============================================================================
    VUMeterComponent.h
    GOODMETER - Classic VU Meter with ballistics

    Translated from ClassicVUMeter.tsx
    Features: Flat arc, needle animation with damping, dual color zones
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Classic VU Meter Component with ballistic damping
 * Range: -30 VU to +3 VU
 * Dual zones: normal (-30 to 0) and danger (0 to +3)
 */
class VUMeterComponent : public juce::Component,
                         public juce::Timer
{
public:
    //==========================================================================
    VUMeterComponent()
    {
        // ✅ 只设置高度，宽度由父容器（MeterCard）控制
        setSize(100, 220);  // 初始宽度会被父容器覆盖

        // Start 60Hz timer for smooth needle animation
        startTimerHz(60);
    }

    ~VUMeterComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (bounds.isEmpty() || bounds.getHeight() < 20.0f)
            return;

        int bw = getWidth(), bh = getHeight();

        if (!GoodMeterLookAndFeel::preferDirectChartText())
        {
            // Rebuild text cache only on resize
            const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

            if (vuTextCache.isNull() || lastVuCacheW != bw || lastVuCacheH != bh
                || std::abs(lastVuCacheScale - scale) > 0.01f)
            {
                lastVuCacheW = bw;
                lastVuCacheH = bh;
                lastVuCacheScale = scale;
                vuTextCache = juce::Image(juce::Image::ARGB,
                                          juce::jmax(1, juce::roundToInt(static_cast<float>(bw) * scale)),
                                          juce::jmax(1, juce::roundToInt(static_cast<float>(bh) * scale)),
                                          true, juce::SoftwareImageType());
                juce::Graphics tg(vuTextCache);
                tg.addTransform(juce::AffineTransform::scale(scale));

                // Recompute geometry (same as below)
                const float vuTextH = juce::jlimit(14.0f, 35.0f, bounds.getHeight() * 0.1f);
                const float bottomPad = juce::jlimit(3.0f, 12.0f, bounds.getHeight() * 0.03f);
                const float topPad = juce::jlimit(5.0f, 15.0f, bounds.getHeight() * 0.05f);
                const float sidePad = 15.0f;
                float cx = bounds.getCentreX();
                float cy = bounds.getBottom() - bottomPad - vuTextH;
                float topSpace = juce::jmax(10.0f, cy - bounds.getY() - topPad);
                float sideSpace = juce::jmax(10.0f, bounds.getWidth() / 2.0f - sidePad);
                float radiusV = topSpace * 0.90f;
                float sinVal = juce::jlimit(0.0f, 0.99f, sideSpace / juce::jmax(1.0f, radiusV));
                float naturalHalfAngle = std::asin(sinVal);
                float halfAngle = juce::jlimit(
                    juce::MathConstants<float>::pi / 3.5f,
                    juce::MathConstants<float>::pi * 0.44f,
                    naturalHalfAngle);
                float radiusH = sideSpace / std::sin(halfAngle);
                float radius = juce::jmax(15.0f, juce::jmin(radiusV, radiusH));
                float minAngle = -halfAngle;
                float maxAngle =  halfAngle;

                drawTicksAndLabels(tg, cx, cy, radius, minAngle, maxAngle);

                const float vuFontSize = GoodMeterLookAndFeel::chartFont(
                    juce::jlimit(12.0f, 32.0f, vuTextH * 0.8f));
                tg.setColour(GoodMeterLookAndFeel::border);
                tg.setFont(juce::Font(vuFontSize, juce::Font::bold));
                tg.drawText("VU",
                          static_cast<int>(bounds.getX()),
                          static_cast<int>(cy + 2.0f),
                          static_cast<int>(bounds.getWidth()),
                          static_cast<int>(vuTextH),
                          juce::Justification::centred, false);
            }
        }

        // VU text space
        const float vuTextH = juce::jlimit(14.0f, 35.0f, bounds.getHeight() * 0.1f);
        const float bottomPad = juce::jlimit(3.0f, 12.0f, bounds.getHeight() * 0.03f);
        const float topPad = juce::jlimit(5.0f, 15.0f, bounds.getHeight() * 0.05f);
        const float sidePad = 15.0f;

        float cx = bounds.getCentreX();
        float cy = bounds.getBottom() - bottomPad - vuTextH;
        float topSpace = juce::jmax(10.0f, cy - bounds.getY() - topPad);
        float sideSpace = juce::jmax(10.0f, bounds.getWidth() / 2.0f - sidePad);
        float radiusV = topSpace * 0.90f;
        float sinVal = juce::jlimit(0.0f, 0.99f, sideSpace / juce::jmax(1.0f, radiusV));
        float naturalHalfAngle = std::asin(sinVal);
        float halfAngle = juce::jlimit(
            juce::MathConstants<float>::pi / 3.5f,
            juce::MathConstants<float>::pi * 0.44f,
            naturalHalfAngle);
        float radiusH = sideSpace / std::sin(halfAngle);
        float radius = juce::jmax(15.0f, juce::jmin(radiusV, radiusH));
        float minAngle = -halfAngle;
        float maxAngle =  halfAngle;
        float zeroVuAngle = juce::jmap(0.0f, minVu, maxVu, minAngle, maxAngle);
        const float arcThickness = juce::jlimit(3.0f, 6.0f, radius * 0.04f);

        // Draw arcs (no text)
        drawArc(g, cx, cy, radius, minAngle, zeroVuAngle, GoodMeterLookAndFeel::border, arcThickness);
        drawArc(g, cx, cy, radius, zeroVuAngle, maxAngle, GoodMeterLookAndFeel::accentPink, arcThickness);

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            drawTicksAndLabels(g, cx, cy, radius, minAngle, maxAngle);
            const float vuFontSize = GoodMeterLookAndFeel::chartFont(
                juce::jlimit(12.0f, 32.0f, vuTextH * 0.8f));
            g.setColour(GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(vuFontSize, juce::Font::bold));
            g.drawText("VU",
                       static_cast<int>(bounds.getX()),
                       static_cast<int>(cy + 2.0f),
                       static_cast<int>(bounds.getWidth()),
                       static_cast<int>(vuTextH),
                       juce::Justification::centred, false);
        }
        else
        {
            g.drawImage(vuTextCache,
                        0, 0, bw, bh,
                        0, 0,
                        vuTextCache.getWidth(),
                        vuTextCache.getHeight());
        }

        // Draw needle (dynamic, no text)
        drawNeedle(g, cx, cy, radius, minAngle, maxAngle);
    }

    void resized() override
    {
        // No child components
    }

    //==========================================================================
    /**
     * Update VU value from processor (called from PluginEditor::timerCallback)
     *
     * CRITICAL: Processor 传入的已经是 dB 值（rmsL_dB, rmsR_dB）
     * 不要再做 log10 转换！
     */
    void updateVU(float rmsL_dB, float rmsR_dB)
    {
        // 1. Calculate max RMS in dB (ClassicVUMeter.tsx line 32)
        const float vu_dB = std::max(rmsL_dB, rmsR_dB);

        // 2. Apply +18 dB offset: 0 VU = -18 dBFS (broadcast standard)
        const float vuCalibrated = vu_dB + 18.0f;

        // 3. Normalize to 0.0-1.0 range (ClassicVUMeter.tsx lines 42-43)
        // VU range: -30 VU to +3 VU
        float targetLevel = (vuCalibrated - minVu) / (maxVu - minVu);
        targetLevel = juce::jlimit(0.0f, 1.0f, targetLevel);

        // 3. Apply ballistics (smoothing) (ClassicVUMeter.tsx line 46)
        currentVuDisplay += (targetLevel - currentVuDisplay) * vuSmoothing;

        repaint();
    }

private:
    //==========================================================================
    // VU range constants (ClassicVUMeter.tsx lines 39-40)
    static constexpr float minVu = -30.0f;
    static constexpr float maxVu = 3.0f;

    // Ballistics: 300ms integration time constant (IIR first-order)
    // alpha = 1 - exp(-dt/tau), dt=1/60s, tau=0.3s → ~0.054
    static constexpr float vuSmoothing = 0.054f;

    // Current display value (0.0 to 1.0)
    float currentVuDisplay = 0.0f;

    // Needle trail ring buffer (stores recent angles for afterimage)
    static constexpr int trailSize = 12;
    float trailAngles[trailSize] = {};
    int trailHead = 0;
    bool trailFilled = false;  // true once buffer has been filled at least once

    // Offscreen text cache (STATIC — only rebuild on resize)
    juce::Image vuTextCache;
    int lastVuCacheW = 0;
    int lastVuCacheH = 0;
    float lastVuCacheScale = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // Smooth animation handled in updateVU()
    }

    //==========================================================================
    /**
     * Draw circular arc using juce::Path
     */
    void drawArc(juce::Graphics& g,
                 float centerX, float centerY, float radius,
                 float startAngle, float endAngle,
                 const juce::Colour& colour, float lineWidth)
    {
        juce::Path arcPath;
        arcPath.addCentredArc(centerX, centerY, radius, radius,
                             0.0f,  // rotation
                             startAngle, endAngle,
                             true);  // startAsNewSubPath

        g.setColour(colour);
        g.strokePath(arcPath, juce::PathStrokeType(GoodMeterLookAndFeel::chartStroke(lineWidth, 1.18f, lineWidth)));
    }

    //==========================================================================
    /**
     * Draw ticks and labels
     */
    void drawTicksAndLabels(juce::Graphics& g,
                           float cx, float cy, float radius,
                           float minAngle, float maxAngle)
    {
        // Proportional tick dimensions
        const float majorTickLen = juce::jlimit(15.0f, 30.0f, radius * 0.2f);
        const float mediumTickLen = juce::jlimit(8.0f, 15.0f, radius * 0.1f);
        const float smallTickLen = juce::jlimit(5.0f, 10.0f, radius * 0.065f);
        const float tinyTickLen = juce::jlimit(3.0f, 7.0f, radius * 0.04f);
        const float majorTickWidth = juce::jlimit(2.5f, 4.0f, radius * 0.03f);
        const float mediumTickWidth = juce::jlimit(2.0f, 4.0f, radius * 0.025f);
        const float smallTickWidth = juce::jlimit(1.2f, 2.5f, radius * 0.015f);
        const float tinyTickWidth = juce::jlimit(0.8f, 1.5f, radius * 0.01f);
        const float labelFontSize = juce::jlimit(9.0f, 14.0f, radius * 0.1f);

        // === 1. Tiny ticks: every 1 dB from -30 to +3 (finest graduation) ===
        for (int db = -30; db <= 3; ++db)
        {
            const float angle = juce::jmap(static_cast<float>(db), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (db > 0);
            const float innerR = radius - tinyTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour((isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border)
                            .withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.42f : 0.25f));
            g.drawLine(x1, y1, x2, y2, tinyTickWidth);
        }

        // === 2. Small ticks: every 5 dB from -25 to -15, plus -7 ===
        const int smallTicks[] = { -25, -15, -7 };
        for (int tickVu : smallTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (tickVu > 0);
            const float innerR = radius - smallTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour((isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border)
                            .withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.62f : 0.45f));
            g.drawLine(x1, y1, x2, y2, smallTickWidth);
        }

        // === 3. Medium ticks (labeled): -5, -3, -1, +1, +2, +3 ===
        const int mediumTicks[] = { -5, -3, -1, 1, 2, 3 };
        for (int tickVu : mediumTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isDanger = (tickVu > 0);
            const float innerR = radius - mediumTickLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.drawLine(x1, y1, x2, y2, mediumTickWidth);

            // Label
            const float labelR = radius - mediumTickLen - juce::jlimit(5.0f, 10.0f, radius * 0.07f);
            const float lx = cx + std::sin(angle) * labelR;
            const float ly = cy - std::cos(angle) * labelR;
            juce::String labelText = (tickVu > 0) ? ("+" + juce::String(tickVu)) : juce::String(tickVu);

            g.setColour(isDanger ? GoodMeterLookAndFeel::accentPink : GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(labelFontSize), juce::Font::bold));
            g.drawText(labelText,
                      static_cast<int>(lx - 15), static_cast<int>(ly - 8),
                      30, 16,
                      juce::Justification::centred, false);
        }

        // === 4. Major ticks (labeled): -30, -20, -10, 0 ===
        const int majorTicks[] = { -30, -20, -10, 0 };
        for (int tickVu : majorTicks)
        {
            const float angle = juce::jmap(static_cast<float>(tickVu), minVu, maxVu, minAngle, maxAngle);
            const bool isZero = (tickVu == 0);
            const float tLen = isZero ? majorTickLen * 1.1f : majorTickLen;
            const float innerR = radius - tLen;

            const float x1 = cx + std::sin(angle) * radius;
            const float y1 = cy - std::cos(angle) * radius;
            const float x2 = cx + std::sin(angle) * innerR;
            const float y2 = cy - std::cos(angle) * innerR;

            g.setColour(GoodMeterLookAndFeel::border);
            g.drawLine(x1, y1, x2, y2, majorTickWidth);

            // Label
            const float labelR = radius - tLen - juce::jlimit(5.0f, 10.0f, radius * 0.07f);
            const float lx = cx + std::sin(angle) * labelR;
            const float ly = cy - std::cos(angle) * labelR;
            juce::String labelText = juce::String(tickVu);

            g.setColour(GoodMeterLookAndFeel::border);
            g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(labelFontSize), juce::Font::bold));
            g.drawText(labelText,
                      static_cast<int>(lx - 15), static_cast<int>(ly - 8),
                      30, 16,
                      juce::Justification::centred, false);
        }
    }

    //==========================================================================
    /**
     * Draw needle with green fluorescent sweep trail on the tip quarter
     * Trail is perpendicular to needle — a soft glowing fan/flag that sweeps with motion
     */
    void drawNeedle(juce::Graphics& g,
                   float centerX, float centerY, float radius,
                   float minAngle, float maxAngle)
    {
        float safeVuDisplay = currentVuDisplay;
        if (std::isnan(safeVuDisplay) || std::isinf(safeVuDisplay))
            safeVuDisplay = 0.0f;
        safeVuDisplay = juce::jlimit(0.0f, 1.0f, safeVuDisplay);

        const float mappedAngle = juce::jmap(safeVuDisplay, 0.0f, 1.0f, minAngle, maxAngle);
        const float needleLength = radius * 0.9f;

        // Push current angle into trail buffer
        trailAngles[trailHead] = mappedAngle;
        trailHead = (trailHead + 1) % trailSize;
        if (trailHead == 0) trailFilled = true;

        // ===== Fluorescent sweep: sliced fan with head→tail gradient =====
        const int trailCount = trailFilled ? trailSize : trailHead;
        if (trailCount > 2)
        {
            // Find angle range in trail buffer
            float trailMin = mappedAngle, trailMax = mappedAngle;
            for (int i = 0; i < trailCount; ++i)
            {
                int idx = trailFilled ? (trailHead + i) % trailSize : i;
                trailMin = juce::jmin(trailMin, trailAngles[idx]);
                trailMax = juce::jmax(trailMax, trailAngles[idx]);
            }

            float spread = trailMax - trailMin;
            if (spread > 0.003f)
            {
                const float innerR = needleLength * 0.68f;
                const float outerR = needleLength * 1.02f;

                // Zone colors: green → yellow → purple by dB region
                const juce::Colour colGreen (0xFF00D084);  // -30 ~ -16
                const juce::Colour colYellow(0xFFF5A623);  // -16 ~ -8
                const juce::Colour colPurple(0xFFB44DFF);  //  -8 ~ +3

                // Precompute angle thresholds for zone boundaries
                const float angleAt16 = juce::jmap(-16.0f, minVu, maxVu, minAngle, maxAngle);
                const float angleAt8  = juce::jmap(-8.0f,  minVu, maxVu, minAngle, maxAngle);

                // Map angle → trail colour with smooth crossfade at boundaries
                auto getTrailColour = [&](float angle) -> juce::Colour
                {
                    float db = juce::jmap(angle, minAngle, maxAngle, minVu, maxVu);
                    if (db < -16.0f)
                        return colGreen;
                    else if (db < -8.0f)
                        return colYellow;
                    else
                        return colPurple;
                };

                // Determine which side is the "tail" (away from needle)
                // and which is the "head" (near current needle angle)
                float distToMin = std::abs(mappedAngle - trailMin);
                float distToMax = std::abs(mappedAngle - trailMax);
                float headAngle, tailAngle;
                if (distToMin < distToMax)
                {
                    headAngle = trailMin;  // needle is near min side
                    tailAngle = trailMax;
                }
                else
                {
                    headAngle = trailMax;  // needle is near max side
                    tailAngle = trailMin;
                }

                // Gap: push headAngle away from needle so trail doesn't overlap
                // Direction: head → tail, so we move head 15% of spread toward tail
                const float gapRatio = 0.15f;
                headAngle += (tailAngle - headAngle) * gapRatio;
                float gappedSpread = std::abs(tailAngle - headAngle);

                if (gappedSpread > 0.002f)
                {
                // Slice the fan into N angular strips with gradient alpha
                const int numSlices = 14;
                for (int s = 0; s < numSlices; ++s)
                {
                    float t0 = static_cast<float>(s) / static_cast<float>(numSlices);
                    float t1 = static_cast<float>(s + 1) / static_cast<float>(numSlices);
                    // t=0 is head (bright), t=1 is tail (dim)
                    float a0 = headAngle + (tailAngle - headAngle) * t0;
                    float a1 = headAngle + (tailAngle - headAngle) * t1;
                    float tMid = (t0 + t1) * 0.5f;

                    // Intensity: cubic falloff from head to tail
                    float intensity = (1.0f - tMid) * (1.0f - tMid);

                    // Color based on angular position (dB zone)
                    float aMid = (a0 + a1) * 0.5f;
                    juce::Colour sliceColour = getTrailColour(aMid);

                    // Build slice path
                    juce::Path slice;
                    slice.addCentredArc(centerX, centerY, outerR, outerR,
                                        0.0f, a0, a1, true);
                    slice.lineTo(centerX + std::sin(a1) * innerR,
                                 centerY - std::cos(a1) * innerR);
                    slice.addCentredArc(centerX, centerY, innerR, innerR,
                                        0.0f, a1, a0, false);
                    slice.closeSubPath();

                    if (!GoodMeterLookAndFeel::isMobileCharts())
                    {
                        // Layer 1: wide soft bloom (outer haze)
                        g.setColour(sliceColour.withAlpha(0.12f * intensity));
                        g.strokePath(slice, juce::PathStrokeType(GoodMeterLookAndFeel::chartStroke(6.0f, 1.15f, 6.5f)));
                    }

                    // Layer 2: core fill
                    g.setColour(sliceColour.withAlpha((GoodMeterLookAndFeel::isMobileCharts() ? 0.42f : 0.55f) * intensity));
                    g.fillPath(slice);
                }
                } // gappedSpread guard
            }
        }

        // ===== Main needle =====
        juce::Path needle;
        needle.startNewSubPath(centerX, centerY);
        needle.lineTo(centerX, centerY - needleLength);

        juce::Graphics::ScopedSaveState state(g);
        g.addTransform(juce::AffineTransform::rotation(mappedAngle, centerX, centerY));

        g.setColour(juce::Colours::red);
        g.strokePath(needle, juce::PathStrokeType(GoodMeterLookAndFeel::chartStroke(3.0f, 1.22f, 3.6f)));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VUMeterComponent)
};
