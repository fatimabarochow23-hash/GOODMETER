/*
  ==============================================================================
    StereoImageComponent.h
    GOODMETER - Stereo Field Visualization (LRMS Cylinders + Goniometer)

    Responsive Layout:
    - Normal Mode: Left ~40% LRMS cylinders, Right ~60% Lissajous goniometer
    - Compact Mode: Lissajous hidden, LRMS tubes fill full width
    - Mode switch: when Lissajous max diameter < tube render height, or width < 250px
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
/**
 * Stereo Field Visualization Component
 * Normal mode: Left ~40% LRMS cylinders, Right ~60% Lissajous goniometer
 * Compact mode: Lissajous hidden, LRMS tubes fill full width (saves CPU)
 */
class StereoImageComponent : public juce::Component,
                              public juce::Timer
{
public:
    //==========================================================================
    StereoImageComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Initialize sample buffers
        sampleBufferL.fill(0.0f);
        sampleBufferR.fill(0.0f);

        // Set fixed height
        setSize(100, 350);

        // Start 60Hz timer for smooth updates
        startTimerHz(60);
    }

    ~StereoImageComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        if (getLocalBounds().isEmpty())
            return;

        // Draw LRMS Cylinders (uses cached layoutTubeBounds)
        drawLRMSCylinders(g, layoutTubeBounds);

        // Goniometer: only in normal mode
        if (!compactMode && !layoutGonBounds.isEmpty())
        {
            auto rightBounds = layoutGonBounds;
            const float gonPad = juce::jmin(8.0f, rightBounds.getHeight() * 0.03f);
            auto gonLocal = rightBounds.toFloat().reduced(gonPad, gonPad);
            const float cx = gonLocal.getCentreX();
            const float cy = gonLocal.getCentreY();
            const float labelMargin = juce::jmin(20.0f, gonLocal.getHeight() * 0.1f);
            const float r = juce::jmax(5.0f, juce::jmin(gonLocal.getWidth(), gonLocal.getHeight()) / 2.0f - labelMargin);

            // Diamond background
            juce::Path diamond;
            diamond.startNewSubPath(cx, cy - r);
            diamond.lineTo(cx + r, cy);
            diamond.lineTo(cx, cy + r);
            diamond.lineTo(cx - r, cy);
            diamond.closeSubPath();
            g.setColour(juce::Colours::black);
            g.fillPath(diamond);

            // Single image blit (clipped to diamond)
            if (!goniometerImage.isNull())
            {
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(diamond);
                g.drawImageAt(goniometerImage, rightBounds.getX(), rightBounds.getY());
            }

            // Grid overlay (lightweight lines only)
            drawGoniometerGrid(g, cx, cy, r);
        }

        // 最顶层：悬浮 dB 刻度尺（与 LRMS 试管高度对齐）
        drawDbScale(g, layoutScaleBounds, layoutTubeBounds);
    }

    void resized() override
    {
        // =================================================================
        // Dimension Sniffing: compute layout rects + compact mode flag
        // =================================================================
        auto bounds = getLocalBounds();
        if (bounds.isEmpty()) return;

        layoutScaleBounds = bounds.removeFromLeft(scaleWidth);

        // Compute tube render height (approximate: area reduced by padding - label space)
        const float padY = juce::jmin(15.0f, bounds.getHeight() * 0.05f);
        const float labelSpace = juce::jlimit(12.0f, 22.0f, (bounds.getHeight() - padY * 2.0f) * 0.09f);
        const float tubeRenderH = bounds.getHeight() - padY * 2.0f - labelSpace;

        // Max diameter available for Lissajous if we gave tubes 40% width
        const int tubeWidth40 = static_cast<int>(bounds.getWidth() * 0.40f);
        const int spacing = juce::jlimit(5, 15, static_cast<int>(bounds.getWidth() * 0.03f));
        const int gonAvailW = bounds.getWidth() - tubeWidth40 - spacing;
        const int gonAvailH = bounds.getHeight();
        const int gonMaxDiameter = juce::jmin(gonAvailW, gonAvailH);

        // Compact mode: Lissajous too small to be useful, or total width too narrow
        bool wasCompact = compactMode;
        compactMode = (gonMaxDiameter < static_cast<int>(tubeRenderH))
                   || (bounds.getWidth() + scaleWidth < 250);

        if (compactMode)
        {
            // Tubes take full width — no goniometer
            layoutTubeBounds = bounds;
            layoutGonBounds = {};

            // Release offscreen buffer to save memory
            if (!goniometerImage.isNull())
                goniometerImage = juce::Image();
        }
        else
        {
            // Normal mode: tubes 40%, gap, goniometer gets the rest
            const int leftWidth = juce::jmax(
                juce::jmin(120, bounds.getWidth() / 2),
                static_cast<int>(bounds.getWidth() * 0.35f));

            auto work = bounds;
            layoutTubeBounds = work.removeFromLeft(leftWidth);
            work.removeFromLeft(spacing);
            layoutGonBounds = work;

            // Recreate offscreen buffer if size changed
            if (wasCompact || goniometerImage.isNull())
                goniometerImage = juce::Image();
        }

        // Invalidate text caches on any resize
        lrmsTextCache = juce::Image();
        gonGridTextCache = juce::Image();
        dbScaleTextCache = juce::Image();
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // Sample buffers for Goniometer (stores recent L/R pairs)
    static constexpr int bufferSize = GOODMETERAudioProcessor::stereoSampleBufferSize;
    std::array<float, bufferSize> sampleBufferL;
    std::array<float, bufferSize> sampleBufferR;
    int sampleCount = 0;

    // Current LRMS levels (raw from processor atomics)
    float currentL = -90.0f;
    float currentR = -90.0f;
    float currentM = -90.0f;
    float currentS = -90.0f;

    // 🎯 平滑插值显示值 (Lerp smoothing for silky animation)
    float displayL = -90.0f;
    float displayR = -90.0f;
    float displayM = -90.0f;
    float displayS = -90.0f;

    // 🎯 Offscreen ghosting buffer for Goniometer
    juce::Image goniometerImage;
    float lastGoniometerWidth = 0.0f;
    float lastGoniometerHeight = 0.0f;

    // Responsive layout: cached rects computed in resized()
    bool compactMode = false;                   // true = Lissajous hidden, tubes fill width
    juce::Rectangle<int> layoutScaleBounds;     // dB scale (left 35px)
    juce::Rectangle<int> layoutTubeBounds;      // LRMS cylinder area
    juce::Rectangle<int> layoutGonBounds;       // Goniometer area (empty in compact mode)

    // 左侧刻度尺宽度
    static constexpr int scaleWidth = 35;

    // Offscreen text caches (STATIC — rebuild on resize only)
    juce::Image lrmsTextCache;
    juce::Image gonGridTextCache;
    juce::Image dbScaleTextCache;
    int lastLrmsW = 0, lastLrmsH = 0;
    int lastGonGridW = 0, lastGonGridH = 0;
    int lastDbScaleW = 0, lastDbScaleH = 0;

    //==========================================================================
    void timerCallback() override
    {
        // 60Hz → 30Hz smart throttle during mouse drag
        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        {
            static int dragThrottleCounter = 0;
            if (++dragThrottleCounter % 2 != 0) return;
        }

        // Update LRMS levels (RMS dB values from processor)
        currentL = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
        currentR = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
        currentM = audioProcessor.rmsLevelMid.load(std::memory_order_relaxed);
        currentS = audioProcessor.rmsLevelSide.load(std::memory_order_relaxed);

        const float smoothing = 0.35f;
        displayL += (currentL - displayL) * smoothing;
        displayR += (currentR - displayR) * smoothing;
        displayM += (currentM - displayM) * smoothing;
        displayS += (currentS - displayS) * smoothing;

        // Flush stereo FIFO: drain all, keep only the latest batch
        float tempL[512];
        float tempR[512];
        sampleCount = 0;
        while (audioProcessor.stereoSampleFifoL.pop(tempL, 512) &&
               audioProcessor.stereoSampleFifoR.pop(tempR, 512))
        {
            for (int i = 0; i < 512; ++i)
            {
                sampleBufferL[i] = tempL[i];
                sampleBufferR[i] = tempR[i];
            }
            sampleCount = 512;
        }

        // Render Goniometer trails to offscreen SoftwareImage (zero MML)
        renderGoniometerOffscreen();

        repaint();
    }

    //==========================================================================
    /**
     * Draw LRMS as four uniform cylinders with glow aesthetic
     */
    void drawLRMSCylinders(juce::Graphics& g, const juce::Rectangle<int>& bounds)
    {
        const float padX = juce::jmin(10.0f, bounds.getWidth() * 0.05f);
        const float padY = juce::jmin(15.0f, bounds.getHeight() * 0.05f);
        auto area = bounds.toFloat().reduced(padX, padY);

        if (area.getHeight() < 30.0f || area.getWidth() < 40.0f)
            return;

        const float labelSpace = juce::jlimit(12.0f, 22.0f, area.getHeight() * 0.09f);
        const float tubeHeight = area.getHeight() - labelSpace;

        // Four columns with gaps
        const float gap = juce::jlimit(4.0f, 12.0f, area.getWidth() * 0.03f);
        const float colWidth = (area.getWidth() - gap * 3.0f) / 4.0f;
        const float tubeWidth = juce::jmin(colWidth * 0.65f, 24.0f);

        const float dbMin = -60.0f;
        const float dbMax = 0.0f;

        auto normLevel = [&](float db) -> float {
            float n = juce::jlimit(0.0f, 1.0f, juce::jmap(db, dbMin, dbMax, 0.0f, 1.0f));
            // Perceptual pseudo-log: sqrt expands high-level detail
            n = std::pow(n, 0.5f);
            return n < 0.005f ? 0.0f : n;
        };

        struct TubeConfig {
            juce::String label;
            float level;
            juce::Colour color;
        };

        TubeConfig tubes[4] = {
            { "L", normLevel(displayL), GoodMeterLookAndFeel::accentPink },
            { "R", normLevel(displayR), GoodMeterLookAndFeel::accentPink },
            { "M", normLevel(displayM), GoodMeterLookAndFeel::accentYellow },
            { "S", normLevel(displayS), GoodMeterLookAndFeel::accentGreen }
        };

        // Pre-render labels if size changed
        int lrmsKey = static_cast<int>(area.getWidth()) * 1000 + static_cast<int>(area.getHeight());
        if (lrmsTextCache.isNull() || lastLrmsW != static_cast<int>(area.getWidth()) || lastLrmsH != static_cast<int>(area.getHeight()))
        {
            lastLrmsW = static_cast<int>(area.getWidth());
            lastLrmsH = static_cast<int>(area.getHeight());
            lrmsTextCache = juce::Image(juce::Image::ARGB, lastLrmsW, static_cast<int>(labelSpace + 2), true, juce::SoftwareImageType());
            juce::Graphics tg(lrmsTextCache);
            const juce::String labels[] = { "L", "R", "M", "S" };
            for (int i = 0; i < 4; ++i)
            {
                float colX = i * (colWidth + gap);
                const float fontSize = juce::jlimit(8.0f, 12.0f, labelSpace * 0.55f);
                tg.setColour(GoodMeterLookAndFeel::textMain);
                tg.setFont(juce::Font(fontSize, juce::Font::bold));
                tg.drawText(labels[i], static_cast<int>(colX), 0, static_cast<int>(colWidth), static_cast<int>(labelSpace), juce::Justification::centred, false);
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            auto& tube = tubes[i];

            // Center the tube within its column
            float colX = area.getX() + i * (colWidth + gap);
            float tx = colX + (colWidth - tubeWidth) / 2.0f;
            float ty = area.getY();
            float tw = tubeWidth;
            float th = tubeHeight;
            float cornerR = tw / 2.0f;

            // Rounded-bottom cylinder path
            juce::Path tubePath;
            tubePath.addRoundedRectangle(tx, ty, tw, th, cornerR, cornerR,
                                          false, false, true, true);

            // === Clip-based liquid fill ===
            if (tube.level > 0.0f)
            {
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(tubePath);

                float fillH = th * tube.level;
                float fillY = ty + th - fillH;

                g.setColour(tube.color.withAlpha(0.65f));
                g.fillRect(tx - 1.0f, fillY, tw + 2.0f, fillH + 1.0f);

                // Meniscus highlight
                if (fillH > 2.0f)
                {
                    g.setColour(tube.color.brighter(0.5f).withAlpha(0.5f));
                    g.fillRect(tx - 1.0f, fillY, tw + 2.0f, 1.5f);
                }
            }

            // === Outer glow (neon halo) ===
            if (tube.level > 0.05f)
            {
                float glowAlpha = juce::jlimit(0.0f, 0.12f, tube.level * 0.12f);
                g.setColour(tube.color.withAlpha(glowAlpha));
                g.strokePath(tubePath, juce::PathStrokeType(5.0f));
            }

            // === Glass outline (Neo-Brutalism: solid visible border) ===
            g.setColour(juce::Colour(0xFF2A2A35).withAlpha(0.55f));
            g.strokePath(tubePath, juce::PathStrokeType(1.5f));

            // === Glass highlight (subtle but present) ===
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.strokePath(tubePath, juce::PathStrokeType(0.8f));

            // === Tick marks (synced with pow(0.5) mapping) ===
            const float tickDbs[] = { -45.0f, -30.0f, -15.0f };
            for (float tdb : tickDbs)
            {
                float tn = juce::jlimit(0.0f, 1.0f, (tdb - dbMin) / (dbMax - dbMin));
                tn = std::pow(tn, 0.5f);
                float tickY = ty + th - tn * th;
                bool isMajor = (static_cast<int>(tdb) == -30);
                float tickLen = isMajor ? tw * 0.5f : tw * 0.3f;
                float tickStroke = isMajor ? 1.5f : 1.0f;
                g.setColour(juce::Colour(0xFF2A2A35).withAlpha(isMajor ? 0.50f : 0.35f));
                g.drawLine(tx + tw - tickLen, tickY, tx + tw, tickY, tickStroke);
            }
        }

        // Blit cached labels
        if (!lrmsTextCache.isNull())
            g.drawImageAt(lrmsTextCache, static_cast<int>(area.getX()), static_cast<int>(area.getY() + tubeHeight + 1.0f));
    }

    //==========================================================================
    /**
     * Render Goniometer trails to offscreen SoftwareImage (called from timerCallback)
     * Zero CoreGraphics — pure BitmapData pixel writing + Bresenham lines
     */
    void renderGoniometerOffscreen()
    {
        // Skip entirely in compact mode (no Lissajous to render)
        if (compactMode || layoutGonBounds.isEmpty())
            return;

        auto rightBounds = layoutGonBounds;
        int w = rightBounds.getWidth();
        int h = rightBounds.getHeight();
        if (w < 2 || h < 2) return;

        // Lazy init with SoftwareImageType (immune to MML dark lock)
        if (goniometerImage.isNull() ||
            goniometerImage.getWidth() != w ||
            goniometerImage.getHeight() != h)
        {
            goniometerImage = juce::Image(juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
            juce::Graphics initG(goniometerImage);
            initG.fillAll(juce::Colours::black);
            lastGoniometerWidth = static_cast<float>(w);
            lastGoniometerHeight = static_cast<float>(h);
        }

        // Goniometer geometry (image-local coordinates)
        const float gonPad = juce::jmin(8.0f, static_cast<float>(h) * 0.03f);
        const float localW = static_cast<float>(w) - gonPad * 2.0f;
        const float localH = static_cast<float>(h) - gonPad * 2.0f;
        const float imgCx = static_cast<float>(w) * 0.5f;
        const float imgCy = static_cast<float>(h) * 0.5f;
        const float labelMargin = juce::jmin(20.0f, localH * 0.1f);
        const float r = juce::jmax(5.0f, juce::jmin(localW, localH) / 2.0f - labelMargin);

        // Phase 1: Ghost trail fade (single Graphics call, then release)
        {
            juce::Graphics imageG(goniometerImage);
            imageG.fillAll(juce::Colours::black.withAlpha(0.06f));
        }

        // Phase 2: BitmapData pixel-level rendering (zero CoreGraphics)
        if (sampleCount > 2)
        {
            const float scale = r * 0.8f;
            const juce::Colour pink = GoodMeterLookAndFeel::accentPink;
            const uint8_t pr = pink.getRed(), pg = pink.getGreen(), pb = pink.getBlue();

            // Precompute decimated points (step-4 for 30Hz: same visual density as step-2@60Hz)
            static constexpr int maxPts = bufferSize / 4 + 1;
            int ptX[maxPts], ptY[maxPts];
            int numPts = 0;
            for (int i = 0; i < sampleCount && numPts < maxPts; i += 4)
            {
                float fx, fy;
                computeGonPoint(i, imgCx, imgCy, r, scale, fx, fy);
                ptX[numPts] = juce::roundToInt(fx);
                ptY[numPts] = juce::roundToInt(fy);
                numPts++;
            }

            juce::Image::BitmapData bmp(goniometerImage, juce::Image::BitmapData::readWrite);

            // Bresenham core lines + soft glow at sample points
            for (int j = 0; j < numPts - 1; ++j)
            {
                bresenhamLine(bmp, ptX[j], ptY[j], ptX[j + 1], ptY[j + 1],
                              w, h, pr, pg, pb, 0.55f);
            }

            // Glow pass: 4-neighbor soft halo at each sample point
            for (int j = 0; j < numPts; ++j)
            {
                blendPixel(bmp, ptX[j] - 1, ptY[j],     w, h, pr, pg, pb, 0.18f);
                blendPixel(bmp, ptX[j] + 1, ptY[j],     w, h, pr, pg, pb, 0.18f);
                blendPixel(bmp, ptX[j],     ptY[j] - 1, w, h, pr, pg, pb, 0.18f);
                blendPixel(bmp, ptX[j],     ptY[j] + 1, w, h, pr, pg, pb, 0.18f);
            }
        }
    }

    /** Alpha-blend a single pixel into BitmapData (no CoreGraphics) */
    static inline void blendPixel(juce::Image::BitmapData& bmp, int x, int y, int w, int h,
                                  uint8_t r, uint8_t g, uint8_t b, float alpha)
    {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        auto existing = bmp.getPixelColour(x, y);
        bmp.setPixelColour(x, y, existing.interpolatedWith(juce::Colour(r, g, b), alpha));
    }

    /** Bresenham line rasterizer — pure integer math, zero CoreGraphics */
    static inline void bresenhamLine(juce::Image::BitmapData& bmp,
                                     int x0, int y0, int x1, int y1,
                                     int w, int h,
                                     uint8_t r, uint8_t g, uint8_t b, float alpha)
    {
        int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        for (int step = 0; step < 2000; ++step)  // safety cap
        {
            blendPixel(bmp, x0, y0, w, h, r, g, b, alpha);
            if (x0 == x1 && y0 == y1) break;
            int e2 = err * 2;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 <  dx) { err += dx; y0 += sy; }
        }
    }

    inline void computeGonPoint(int i, float cx, float cy, float r, float scale,
                                float& outX, float& outY) const
    {
        const float mid  = sampleBufferL[i] + sampleBufferR[i];
        const float side = sampleBufferL[i] - sampleBufferR[i];
        float x = cx + side * scale;
        float y = cy - mid * scale;
        const float dist = std::abs(x - cx) + std::abs(y - cy);
        if (dist > r)
        {
            const float sf = r / dist;
            x = cx + (x - cx) * sf;
            y = cy + (y - cy) * sf;
        }
        outX = x;
        outY = y;
    }

    //==========================================================================
    /**
     * 🎨 Draw Diamond Grid (菱形网格)
     * 1. 外菱形边框
     * 2. 内菱形辅助线（半径 0.5r）
     * 3. 十字交叉线（M 轴垂直，S 轴水平）
     * 4. 标签文本：M, -M, L, R
     */
    void drawGoniometerGrid(juce::Graphics& g, float cx, float cy, float r)
    {
        // Outer diamond (bold Neo-Brutalism border)
        juce::Path outerDiamond;
        outerDiamond.startNewSubPath(cx, cy - r);
        outerDiamond.lineTo(cx + r, cy);
        outerDiamond.lineTo(cx, cy + r);
        outerDiamond.lineTo(cx - r, cy);
        outerDiamond.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.55f));
        g.strokePath(outerDiamond, juce::PathStrokeType(1.5f));

        // Inner diamond (reference grid — visible but secondary)
        const float innerR = r * 0.5f;
        juce::Path innerDiamond;
        innerDiamond.startNewSubPath(cx, cy - innerR);
        innerDiamond.lineTo(cx + innerR, cy);
        innerDiamond.lineTo(cx, cy + innerR);
        innerDiamond.lineTo(cx - innerR, cy);
        innerDiamond.closeSubPath();
        g.setColour(juce::Colours::white.withAlpha(0.30f));
        g.strokePath(innerDiamond, juce::PathStrokeType(1.0f));

        // Cross lines (axis indicators — bold)
        g.setColour(juce::Colours::white.withAlpha(0.40f));
        g.drawLine(cx, cy - r, cx, cy + r, 1.0f);
        g.drawLine(cx - r, cy, cx + r, cy, 1.0f);

        // Labels: blit from cache (static, only rebuild on size change)
        int cacheW = static_cast<int>(r * 2.0f + 40.0f);
        int cacheH = static_cast<int>(r * 2.0f + 40.0f);
        if (cacheW < 4 || cacheH < 4) return;

        if (gonGridTextCache.isNull() || lastGonGridW != cacheW || lastGonGridH != cacheH)
        {
            lastGonGridW = cacheW;
            lastGonGridH = cacheH;
            gonGridTextCache = juce::Image(juce::Image::ARGB, cacheW, cacheH, true, juce::SoftwareImageType());
            juce::Graphics tg(gonGridTextCache);

            float lcx = static_cast<float>(cacheW) / 2.0f;
            float lcy = static_cast<float>(cacheH) / 2.0f;
            const float gridFontSize = juce::jlimit(8.0f, 11.0f, r * 0.1f);
            const float labelOff = juce::jmin(18.0f, r * 0.15f);
            tg.setColour(juce::Colours::white.withAlpha(0.75f));
            tg.setFont(juce::Font(gridFontSize, juce::Font::bold));

            tg.drawFittedText("M", static_cast<int>(lcx - 15), static_cast<int>(lcy - r - labelOff), 30, 20, juce::Justification::centred, 1);
            tg.drawFittedText("-M", static_cast<int>(lcx - 15), static_cast<int>(lcy + r + labelOff * 0.2f), 30, 20, juce::Justification::centred, 1);
            tg.drawFittedText("L", static_cast<int>(lcx - r - labelOff - 5), static_cast<int>(lcy - 10), 30, 20, juce::Justification::centred, 1);
            tg.drawFittedText("R", static_cast<int>(lcx + r + labelOff * 0.2f - 5), static_cast<int>(lcy - 10), 30, 20, juce::Justification::centred, 1);
        }

        float offsetX = cx - static_cast<float>(cacheW) / 2.0f;
        float offsetY = cy - static_cast<float>(cacheH) / 2.0f;
        g.drawImageAt(gonGridTextCache, static_cast<int>(offsetX), static_cast<int>(offsetY));
    }

    //==========================================================================
    /**
     * 左侧标准 dB 刻度尺（与 LRMS 试管高度精确对齐）
     */
    void drawDbScale(juce::Graphics& g,
                     const juce::Rectangle<int>& scaleBounds,
                     const juce::Rectangle<int>& tubeBounds)
    {
        int sw = scaleBounds.getWidth();
        int sh = scaleBounds.getHeight();
        if (sw < 4 || sh < 20) return;

        if (dbScaleTextCache.isNull() || lastDbScaleW != sw || lastDbScaleH != sh)
        {
            lastDbScaleW = sw;
            lastDbScaleH = sh;
            dbScaleTextCache = juce::Image(juce::Image::ARGB, sw, sh, true, juce::SoftwareImageType());
            juce::Graphics tg(dbScaleTextCache);

            const float padX = juce::jmin(10.0f, tubeBounds.getWidth() * 0.05f);
            const float padY = juce::jmin(15.0f, tubeBounds.getHeight() * 0.05f);
            auto area = tubeBounds.toFloat().reduced(padX, padY);
            if (area.getHeight() < 30.0f) return;

            const float labelSpaceV = juce::jlimit(12.0f, 22.0f, area.getHeight() * 0.09f);
            const float tubeTop = area.getY() - static_cast<float>(scaleBounds.getY());
            const float tubeH = area.getHeight() - labelSpaceV;

            const float tickDbs[] = { -6.0f, -12.0f, -18.0f, -24.0f, -30.0f, -40.0f, -50.0f, -60.0f };
            const float dbMin = -60.0f;
            const float dbMax = 0.0f;
            const float rightX = static_cast<float>(sw);

            tg.setColour(juce::Colours::black);
            tg.setFont(juce::Font(9.0f, juce::Font::bold));

            for (float db : tickDbs)
            {
                float norm = juce::jlimit(0.0f, 1.0f, (db - dbMin) / (dbMax - dbMin));
                // Sync with perceptual pow(0.5) mapping used in normLevel
                norm = std::pow(norm, 0.5f);
                float y = tubeTop + tubeH - norm * tubeH;
                tg.drawLine(rightX - 6.0f, y, rightX, y, 1.5f);
                juce::String text = juce::String(static_cast<int>(db));
                tg.drawText(text, 0, static_cast<int>(y - 6.0f), static_cast<int>(rightX) - 6, 12, juce::Justification::right, false);
            }
        }

        g.drawImageAt(dbScaleTextCache, scaleBounds.getX(), scaleBounds.getY());
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoImageComponent)
};
