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
            g.setColour(marathonDarkStyle ? juce::Colour(0xFF0A0D13) : juce::Colours::black);
            g.fillPath(diamond);

            // Single image blit (clipped to diamond)
            if (!goniometerImage.isNull())
            {
                juce::Graphics::ScopedSaveState state(g);
                g.reduceClipRegion(diamond);
                g.drawImage(goniometerImage,
                            rightBounds.getX(), rightBounds.getY(),
                            rightBounds.getWidth(), rightBounds.getHeight(),
                            0, 0,
                            goniometerImage.getWidth(), goniometerImage.getHeight());
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

    void setMarathonDarkStyle(bool shouldUse)
    {
        if (marathonDarkStyle == shouldUse)
            return;

        marathonDarkStyle = shouldUse;
        lrmsTextCache = {};
        gonGridTextCache = {};
        dbScaleTextCache = {};
        repaint();
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;
    bool marathonDarkStyle = false;

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
    float lastGoniometerScale = 0.0f;

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
    float lastLrmsScale = 0.0f;
    float lastGonGridScale = 0.0f;
    float lastDbScaleScale = 0.0f;

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

    juce::Colour panelBackFill() const
    {
        return juce::Colour(0xFF0A0D13);
    }

    juce::Colour panelInk(float alpha) const
    {
        return juce::Colours::white.withAlpha(alpha);
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
            { "L", normLevel(displayL), marathonDarkStyle ? GoodMeterLookAndFeel::accentPink.brighter(0.10f) : GoodMeterLookAndFeel::accentPink },
            { "R", normLevel(displayR), marathonDarkStyle ? GoodMeterLookAndFeel::accentPink.brighter(0.10f) : GoodMeterLookAndFeel::accentPink },
            { "M", normLevel(displayM), marathonDarkStyle ? GoodMeterLookAndFeel::accentYellow.brighter(0.08f) : GoodMeterLookAndFeel::accentYellow },
            { "S", normLevel(displayS), marathonDarkStyle ? GoodMeterLookAndFeel::accentGreen.brighter(0.10f) : GoodMeterLookAndFeel::accentGreen }
        };

        if (!GoodMeterLookAndFeel::preferDirectChartText())
        {
            const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

            if (lrmsTextCache.isNull() || lastLrmsW != static_cast<int>(area.getWidth()) || lastLrmsH != static_cast<int>(area.getHeight())
                || std::abs(lastLrmsScale - scale) > 0.01f)
            {
                lastLrmsW = static_cast<int>(area.getWidth());
                lastLrmsH = static_cast<int>(area.getHeight());
                lastLrmsScale = scale;
                lrmsTextCache = juce::Image(juce::Image::ARGB,
                                            juce::jmax(1, juce::roundToInt(static_cast<float>(lastLrmsW) * scale)),
                                            juce::jmax(1, juce::roundToInt((labelSpace + 2.0f) * scale)),
                                            true, juce::SoftwareImageType());
                juce::Graphics tg(lrmsTextCache);
                tg.addTransform(juce::AffineTransform::scale(scale));
                const juce::String labels[] = { "L", "R", "M", "S" };
                for (int i = 0; i < 4; ++i)
                {
                    float colX = i * (colWidth + gap);
                    const float fontSize = GoodMeterLookAndFeel::chartFont(
                        juce::jlimit(8.0f, 12.0f, labelSpace * 0.55f));
                    tg.setColour(marathonDarkStyle ? juce::Colour(0xFFF3EFE7)
                                                   : GoodMeterLookAndFeel::textMain);
                    tg.setFont(juce::Font(fontSize, juce::Font::bold));
                    tg.drawText(labels[i], static_cast<int>(colX), 0, static_cast<int>(colWidth), static_cast<int>(labelSpace), juce::Justification::centred, false);
                }
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

                g.setColour(tube.color.withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.88f : 0.72f));
                g.fillRect(tx - 1.0f, fillY, tw + 2.0f, fillH + 1.0f);

                // Meniscus highlight
                if (fillH > 2.0f)
                {
                    g.setColour(tube.color.brighter(0.45f).withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.34f : 0.56f));
                    g.fillRect(tx - 1.0f, fillY, tw + 2.0f, 1.5f);
                }
            }

            // === Outer glow (desktop only — mobile keeps edges crisp) ===
            if (!GoodMeterLookAndFeel::isMobileCharts() && tube.level > 0.05f)
            {
                float glowAlpha = juce::jlimit(0.0f, 0.12f, tube.level * 0.12f);
                g.setColour(tube.color.withAlpha(glowAlpha));
                g.strokePath(tubePath, juce::PathStrokeType(5.0f));
            }

            // === Glass outline (Neo-Brutalism: solid visible border) ===
            g.setColour(marathonDarkStyle
                            ? juce::Colour(0xFFF3EFE7).withAlpha(0.78f)
                            : GoodMeterLookAndFeel::chartInk(GoodMeterLookAndFeel::isMobileCharts() ? 0.62f : 0.55f));
            g.strokePath(tubePath, juce::PathStrokeType(
                GoodMeterLookAndFeel::chartStroke(1.5f, 1.2f, 1.8f)));

            if (!GoodMeterLookAndFeel::isMobileCharts())
            {
                g.setColour(juce::Colours::white.withAlpha(0.35f));
                g.strokePath(tubePath, juce::PathStrokeType(0.8f));
            }

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
                g.setColour(marathonDarkStyle
                                ? panelInk(isMajor ? 0.58f : 0.42f)
                                : GoodMeterLookAndFeel::chartInk(isMajor ? 0.54f : 0.36f));
                g.drawLine(tx + tw - tickLen, tickY, tx + tw, tickY, tickStroke);
            }
        }

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            const juce::String labels[] = { "L", "R", "M", "S" };
            const float fontSize = GoodMeterLookAndFeel::chartFont(
                juce::jlimit(8.0f, 12.0f, labelSpace * 0.55f));
            g.setColour(marathonDarkStyle ? juce::Colour(0xFFF3EFE7)
                                          : GoodMeterLookAndFeel::textMain);
            g.setFont(juce::Font(fontSize, juce::Font::bold));
            for (int i = 0; i < 4; ++i)
            {
                float colX = area.getX() + i * (colWidth + gap);
                g.drawText(labels[i],
                           static_cast<int>(colX),
                           static_cast<int>(area.getY() + tubeHeight + 1.0f),
                           static_cast<int>(colWidth),
                           static_cast<int>(labelSpace),
                           juce::Justification::centred,
                           false);
            }
        }
        else if (!lrmsTextCache.isNull())
            g.drawImage(lrmsTextCache,
                        static_cast<int>(area.getX()), static_cast<int>(area.getY() + tubeHeight + 1.0f),
                        lastLrmsW, static_cast<int>(labelSpace + 2.0f),
                        0, 0,
                        lrmsTextCache.getWidth(), lrmsTextCache.getHeight());
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

        const float imageScale = juce::Component::getApproximateScaleFactorForComponent(this);
        const int pixelW = juce::jmax(1, juce::roundToInt(static_cast<float>(w) * imageScale));
        const int pixelH = juce::jmax(1, juce::roundToInt(static_cast<float>(h) * imageScale));

        // Lazy init with SoftwareImageType (immune to MML dark lock)
        if (goniometerImage.isNull() ||
            goniometerImage.getWidth() != pixelW ||
            goniometerImage.getHeight() != pixelH ||
            std::abs(lastGoniometerScale - imageScale) > 0.01f)
        {
            goniometerImage = juce::Image(juce::Image::ARGB, pixelW, pixelH, true, juce::SoftwareImageType());
            juce::Graphics initG(goniometerImage);
            initG.fillAll(juce::Colours::black);
            lastGoniometerWidth = static_cast<float>(pixelW);
            lastGoniometerHeight = static_cast<float>(pixelH);
            lastGoniometerScale = imageScale;
        }

        // Goniometer geometry (image-local coordinates)
        const float gonPad = juce::jmin(8.0f * imageScale, static_cast<float>(pixelH) * 0.03f);
        const float localW = static_cast<float>(pixelW) - gonPad * 2.0f;
        const float localH = static_cast<float>(pixelH) - gonPad * 2.0f;
        const float imgCx = static_cast<float>(pixelW) * 0.5f;
        const float imgCy = static_cast<float>(pixelH) * 0.5f;
        const float labelMargin = juce::jmin(20.0f * imageScale, localH * 0.1f);
        const float r = juce::jmax(5.0f * imageScale, juce::jmin(localW, localH) / 2.0f - labelMargin);

        // Phase 1: Ghost trail fade (single Graphics call, then release)
        {
            juce::Graphics imageG(goniometerImage);
            imageG.fillAll((marathonDarkStyle ? juce::Colour(0xFF0A0D13) : juce::Colours::black)
                               .withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.18f : 0.06f));
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
                              pixelW, pixelH, pr, pg, pb, 0.55f);
            }

            // Glow pass: 4-neighbor soft halo at each sample point
            for (int j = 0; j < numPts; ++j)
            {
                const float haloAlpha = GoodMeterLookAndFeel::isMobileCharts() ? 0.08f : 0.18f;
                blendPixel(bmp, ptX[j] - 1, ptY[j],         pixelW, pixelH, pr, pg, pb, haloAlpha);
                blendPixel(bmp, ptX[j] + 1, ptY[j],         pixelW, pixelH, pr, pg, pb, haloAlpha);
                blendPixel(bmp, ptX[j],     ptY[j] - 1,     pixelW, pixelH, pr, pg, pb, haloAlpha);
                blendPixel(bmp, ptX[j],     ptY[j] + 1,     pixelW, pixelH, pr, pg, pb, haloAlpha);
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
        g.setColour(panelInk(GoodMeterLookAndFeel::isMobileCharts() ? 0.72f : 0.55f));
        g.strokePath(outerDiamond, juce::PathStrokeType(
            GoodMeterLookAndFeel::chartStroke(1.5f, 1.18f, 1.8f)));

        // Inner diamond (reference grid — visible but secondary)
        const float innerR = r * 0.5f;
        juce::Path innerDiamond;
        innerDiamond.startNewSubPath(cx, cy - innerR);
        innerDiamond.lineTo(cx + innerR, cy);
        innerDiamond.lineTo(cx, cy + innerR);
        innerDiamond.lineTo(cx - innerR, cy);
        innerDiamond.closeSubPath();
        g.setColour(panelInk(GoodMeterLookAndFeel::isMobileCharts() ? 0.42f : 0.30f));
        g.strokePath(innerDiamond, juce::PathStrokeType(
            GoodMeterLookAndFeel::chartStroke(1.0f, 1.15f, 1.15f)));

        // Cross lines (axis indicators — bold)
        g.setColour(panelInk(GoodMeterLookAndFeel::isMobileCharts() ? 0.52f : 0.40f));
        g.drawLine(cx, cy - r, cx, cy + r, GoodMeterLookAndFeel::chartStroke(1.0f, 1.15f, 1.1f));
        g.drawLine(cx - r, cy, cx + r, cy, GoodMeterLookAndFeel::chartStroke(1.0f, 1.15f, 1.1f));

        int cacheW = static_cast<int>(r * 2.0f + 40.0f);
        int cacheH = static_cast<int>(r * 2.0f + 40.0f);
        if (cacheW < 4 || cacheH < 4) return;

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            const float gridFontSize = GoodMeterLookAndFeel::chartFont(
                juce::jlimit(8.0f, 11.0f, r * 0.1f));
            const float labelOff = juce::jmin(18.0f, r * 0.15f);
            g.setColour(panelInk(0.75f));
            g.setFont(juce::Font(gridFontSize, juce::Font::bold));
            g.drawFittedText("M", static_cast<int>(cx - 15.0f), static_cast<int>(cy - r - labelOff), 30, 20, juce::Justification::centred, 1);
            g.drawFittedText("-M", static_cast<int>(cx - 15.0f), static_cast<int>(cy + r + labelOff * 0.2f), 30, 20, juce::Justification::centred, 1);
            g.drawFittedText("L", static_cast<int>(cx - r - labelOff - 5.0f), static_cast<int>(cy - 10.0f), 30, 20, juce::Justification::centred, 1);
            g.drawFittedText("R", static_cast<int>(cx + r + labelOff * 0.2f - 5.0f), static_cast<int>(cy - 10.0f), 30, 20, juce::Justification::centred, 1);
        }
        else
        {
            const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

            if (gonGridTextCache.isNull() || lastGonGridW != cacheW || lastGonGridH != cacheH
                || std::abs(lastGonGridScale - scale) > 0.01f)
            {
                lastGonGridW = cacheW;
                lastGonGridH = cacheH;
                lastGonGridScale = scale;
                gonGridTextCache = juce::Image(juce::Image::ARGB,
                                               juce::jmax(1, juce::roundToInt(static_cast<float>(cacheW) * scale)),
                                               juce::jmax(1, juce::roundToInt(static_cast<float>(cacheH) * scale)),
                                               true, juce::SoftwareImageType());
                juce::Graphics tg(gonGridTextCache);
                tg.addTransform(juce::AffineTransform::scale(scale));

                float lcx = static_cast<float>(cacheW) / 2.0f;
                float lcy = static_cast<float>(cacheH) / 2.0f;
                const float gridFontSize = GoodMeterLookAndFeel::chartFont(
                    juce::jlimit(8.0f, 11.0f, r * 0.1f));
                const float labelOff = juce::jmin(18.0f, r * 0.15f);
                tg.setColour(panelInk(0.75f));
                tg.setFont(juce::Font(gridFontSize, juce::Font::bold));

                tg.drawFittedText("M", static_cast<int>(lcx - 15), static_cast<int>(lcy - r - labelOff), 30, 20, juce::Justification::centred, 1);
                tg.drawFittedText("-M", static_cast<int>(lcx - 15), static_cast<int>(lcy + r + labelOff * 0.2f), 30, 20, juce::Justification::centred, 1);
                tg.drawFittedText("L", static_cast<int>(lcx - r - labelOff - 5), static_cast<int>(lcy - 10), 30, 20, juce::Justification::centred, 1);
                tg.drawFittedText("R", static_cast<int>(lcx + r + labelOff * 0.2f - 5), static_cast<int>(lcy - 10), 30, 20, juce::Justification::centred, 1);
            }

            float offsetX = cx - static_cast<float>(cacheW) / 2.0f;
            float offsetY = cy - static_cast<float>(cacheH) / 2.0f;
            g.drawImage(gonGridTextCache,
                        static_cast<int>(offsetX), static_cast<int>(offsetY),
                        cacheW, cacheH,
                        0, 0,
                        gonGridTextCache.getWidth(), gonGridTextCache.getHeight());
        }
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

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
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

            g.setColour(marathonDarkStyle ? panelInk(0.78f) : juce::Colours::black);
            g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(9.0f), juce::Font::bold));

            for (float db : tickDbs)
            {
                float norm = juce::jlimit(0.0f, 1.0f, (db - dbMin) / (dbMax - dbMin));
                // Sync with perceptual pow(0.5) mapping used in normLevel
                norm = std::pow(norm, 0.5f);
                float y = tubeTop + tubeH - norm * tubeH;
                g.drawLine(scaleBounds.getX() + rightX - 6.0f, scaleBounds.getY() + y,
                           scaleBounds.getX() + rightX, scaleBounds.getY() + y,
                           GoodMeterLookAndFeel::chartStroke(1.5f, 1.15f, 1.8f));
                juce::String text = juce::String(static_cast<int>(db));
                g.drawText(text, scaleBounds.getX(), scaleBounds.getY() + static_cast<int>(y - 6.0f),
                           static_cast<int>(rightX) - 6, 12, juce::Justification::right, false);
            }
        }
        else
        {
            const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

            if (dbScaleTextCache.isNull() || lastDbScaleW != sw || lastDbScaleH != sh
                || std::abs(lastDbScaleScale - scale) > 0.01f)
            {
                lastDbScaleW = sw;
                lastDbScaleH = sh;
                lastDbScaleScale = scale;
                dbScaleTextCache = juce::Image(juce::Image::ARGB,
                                               juce::jmax(1, juce::roundToInt(static_cast<float>(sw) * scale)),
                                               juce::jmax(1, juce::roundToInt(static_cast<float>(sh) * scale)),
                                               true, juce::SoftwareImageType());
                juce::Graphics tg(dbScaleTextCache);
                tg.addTransform(juce::AffineTransform::scale(scale));

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

                tg.setColour(marathonDarkStyle ? panelInk(0.78f) : juce::Colours::black);
                tg.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(9.0f), juce::Font::bold));

                for (float db : tickDbs)
                {
                    float norm = juce::jlimit(0.0f, 1.0f, (db - dbMin) / (dbMax - dbMin));
                    norm = std::pow(norm, 0.5f);
                    float y = tubeTop + tubeH - norm * tubeH;
                    tg.drawLine(rightX - 6.0f, y, rightX, y,
                                GoodMeterLookAndFeel::chartStroke(1.5f, 1.15f, 1.8f));
                    juce::String text = juce::String(static_cast<int>(db));
                    tg.drawText(text, 0, static_cast<int>(y - 6.0f), static_cast<int>(rightX) - 6, 12, juce::Justification::right, false);
                }
            }

            g.drawImage(dbScaleTextCache,
                        scaleBounds.getX(), scaleBounds.getY(),
                        sw, sh,
                        0, 0,
                        dbScaleTextCache.getWidth(), dbScaleTextCache.getHeight());
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoImageComponent)
};
