/*
  ==============================================================================
    SpectralTerrainComponent.h
    GOODMETER iOS - live 2.5D "Spatial Energy Terrain" (ridgeline mountains),
    ported from Audio Doctor's terrain view. Orientation per user spec:
    horizontal axis = time (newest at right), stacked ridges = log frequency
    (100 Hz front/bottom .. 20 kHz back/top), height = energy.

    Data source: the processor's short-window spectrogram FIFO (this view
    REPLACES the waterfall SpectrogramComponent on iOS, so it becomes the
    FIFO's sole consumer). Header-only.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"

class SpectralTerrainComponent : public juce::Component,
                                 private juce::Timer
{
public:
    explicit SpectralTerrainComponent(GOODMETERAudioProcessor& proc)
        : audioProcessor(proc)
    {
        for (auto& col : history) col.fill(0.0f);
        pendingColumn.fill(0.0f);
        pendingDb.fill(-120.0f);
        popScratch.fill(0.0f);
        setInterceptsMouseClicks(true, false);   // drag-to-rotate
        startTimerHz(30);
    }

    ~SpectralTerrainComponent() override { stopTimer(); }

    void setMarathonDarkStyle(bool dark) { marathonDarkStyle = dark; repaint(); }

    // Horizontal drag rotates the camera (yaw -1 .. +1)
    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStartX = e.position.x;
        dragStartYaw = cameraYaw;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const float w = juce::jmax(1.0f, (float) getWidth());
        cameraYaw = juce::jlimit(-1.0f, 1.0f,
                                 dragStartYaw + (e.position.x - dragStartX) / w * 2.2f);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const bool dark = marathonDarkStyle;

        // Edge-to-edge: flat fill, no rounded plate, no border — the card
        // already provides the chrome, an inner frame just wasted space.
        auto plot = getLocalBounds().toFloat();
        const auto bg = dark ? juce::Colour(0xFF0A0D13) : GoodMeterLookAndFeel::bgMain;
        g.setColour(bg);
        g.fillRect(plot);

        auto inner = plot.reduced(6.0f, 2.0f);

        // Projection: time along X, frequency ridges receding up-right.
        // Camera raised + tight headroom (8-up cards were wasting the top),
        // and cameraYaw (horizontal drag) swings the depth direction.
        // timeSpan is computed dynamically so the rightmost point always
        // reaches the card edge (the old static reserve left ~20% empty).
        const float w = inner.getWidth();
        const float dxn = 0.10f + 0.16f * cameraYaw;                 // depth x-share
        const float depthX = w * dxn;
        const float availW = w * 0.96f;
        const float timeSpan = dxn >= 0.0f
            ? juce::jmin(availW, (availW - depthX) / 0.86f)  // 0.86 = back-row perspective
            : availW - std::abs(depthX);
        const float depthY = inner.getHeight() * 0.72f;
        const float heightScale = inner.getHeight() * 0.22f;
        const float originX = inner.getX() + w * 0.01f + (dxn < 0.0f ? -depthX : 0.0f);
        const float originY = inner.getBottom() - inner.getHeight() * 0.02f;

        auto project = [&](float tN, float fN, float v) -> juce::Point<float>
        {
            const float perspective = 1.0f - fN * 0.14f;
            return { originX + tN * timeSpan * perspective + fN * depthX,
                     originY - fN * depthY - juce::jlimit(0.0f, 1.0f, v) * heightScale };
        };

        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(plot.toNearestInt());

        // Base grid
        g.setColour(dark ? juce::Colours::white.withAlpha(0.14f)
                         : GoodMeterLookAndFeel::textMain.withAlpha(0.10f));
        for (int i = 0; i <= 4; ++i)
        {
            const float tN = (float) i / 4.0f;
            auto a = project(tN, 0.0f, 0.0f); auto b = project(tN, 1.0f, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, 0.6f);
        }
        {
            auto a = project(0.0f, 0.0f, 0.0f); auto b = project(1.0f, 0.0f, 0.0f);
            g.drawLine(a.x, a.y, b.x, b.y, 1.1f);
        }

        // Ridges, back (high freq) to front (low freq), painter's algorithm:
        // each ridge fills down to the plot bottom in bg colour first
        // (occludes what's behind), then adds a band-coloured wash + stroke.
        const int newest = writeIndex; // ring: oldest at writeIndex, newest before it
        for (int f = freqRidges - 1; f >= 0; --f)
        {
            const float fN = (float) f / (float) (freqRidges - 1);

            juce::Path line;
            bool started = false;
            float ridgePeak = 0.0f;

            for (int t = 0; t < timeCols; ++t)
            {
                const int col = (newest + t) % timeCols; // oldest -> newest, left -> right
                const float v = history[(size_t) col][(size_t) f];
                ridgePeak = juce::jmax(ridgePeak, v);
                auto p = project((float) t / (float) (timeCols - 1), fN, v);
                if (!started) { line.startNewSubPath(p); started = true; }
                else            line.lineTo(p);
            }

            // Occlusion fill under the ridge
            juce::Path fill(line);
            auto pEnd = project(1.0f, fN, 0.0f);
            auto pStart = project(0.0f, fN, 0.0f);
            fill.lineTo(pEnd.x, plot.getBottom());
            fill.lineTo(pStart.x, plot.getBottom());
            fill.closeSubPath();
            g.setColour(bg);
            g.fillPath(fill);

            // Dark theme: Audio Doctor 3-band colours. Light theme: the grey
            // monochrome variant (slate wash + ink strokes), per user choice.
            const auto band = dark ? bandColour(f) : juce::Colour(0xFF64748B);
            if (ridgePeak > 0.02f)
            {
                g.setColour(band.withAlpha(dark ? 0.16f + ridgePeak * 0.22f
                                                : 0.12f + ridgePeak * 0.18f));
                g.fillPath(fill);
            }

            g.setColour(dark
                ? juce::Colours::white.withAlpha(0.20f + ridgePeak * 0.55f)
                      .interpolatedWith(band, 0.35f)
                : juce::Colour(0xFF334155).withAlpha(0.24f + ridgePeak * 0.58f));
            g.strokePath(line, juce::PathStrokeType(ridgePeak > 0.02f ? 1.1f : 0.6f));
        }

        // Axis hint labels
        g.setColour(dark ? juce::Colours::white.withAlpha(0.55f)
                         : GoodMeterLookAndFeel::textMuted);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        g.drawText("100", (int) inner.getX(), (int) originY - 10, 30, 10,
                   juce::Justification::left, false);
        auto top = project(0.0f, 1.0f, 0.0f);
        g.drawText("20k", (int) top.x, (int) top.y - 12, 30, 10,
                   juce::Justification::left, false);
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        bool advanced = false;
        while (audioProcessor.fftFifoSpectrogramL.pop(popScratch.data(), srcBins))
        {
            // Fold this frame into the pending column (max), advance the
            // column ring every few frames (~30 columns/sec).
            const float nyq = audioProcessor.getSampleRate() > 0.0
                ? (float) (audioProcessor.getSampleRate() * 0.5) : 24000.0f;
            const float minHz = 100.0f, maxHz = 20000.0f;
            const float logRange = std::log(maxHz / minHz);

            float framePeakDb = -120.0f;
            for (int f = 0; f < freqRidges; ++f)
            {
                const float fN0 = (float) f / (float) freqRidges;
                const float fN1 = (float) (f + 1) / (float) freqRidges;
                const float hz0 = minHz * std::exp(fN0 * logRange);
                const float hz1 = minHz * std::exp(fN1 * logRange);
                int b0 = juce::jlimit(1, srcBins - 1, (int) (hz0 / nyq * (float) srcBins));
                int b1 = juce::jlimit(b0, srcBins - 1, (int) (hz1 / nyq * (float) srcBins));

                float m = 0.0f;
                for (int b = b0; b <= b1; ++b)
                    m = juce::jmax(m, popScratch[(size_t) b]);

                const float db = juce::Decibels::gainToDecibels(
                                     m / (float) srcFftSize, -120.0f);
                framePeakDb = juce::jmax(framePeakDb, db);
                pendingDb[(size_t) f] = juce::jmax(pendingDb[(size_t) f], db);
            }

            adaptivePeakDb = framePeakDb >= adaptivePeakDb
                ? framePeakDb : juce::jmax(framePeakDb, adaptivePeakDb - 0.10f);

            if (++framesInColumn >= 4)  // ~23 columns/sec: smoother + cheaper
            {
                const float floorDb = adaptivePeakDb - 60.0f;
                for (int f = 0; f < freqRidges; ++f)
                {
                    pendingColumn[(size_t) f] = juce::jlimit(0.0f, 1.0f,
                        (pendingDb[(size_t) f] - floorDb) / 60.0f);
                    pendingDb[(size_t) f] = -120.0f;
                }
                history[(size_t) writeIndex] = pendingColumn;
                writeIndex = (writeIndex + 1) % timeCols;
                framesInColumn = 0;
                advanced = true;
            }
        }

        if (advanced)
            repaint();
    }

    juce::Colour bandColour(int ridgeIndex) const
    {
        // Dark theme: AUDIO LAB blue family (royal -> azure -> ice cyan),
        // one continuous hue ramp by frequency — replaced the blue/yellow/
        // pink 3-band scheme the user found cheap-looking.
        const float fN = (float) ridgeIndex / (float) (freqRidges - 1);
        const auto royal = juce::Colour(0xFF2A4FC0);
        const auto azure = juce::Colour(0xFF3D9BE9);
        const auto ice   = juce::Colour(0xFF56E1F2);
        return fN < 0.55f ? royal.interpolatedWith(azure, fN / 0.55f)
                          : azure.interpolatedWith(ice, (fN - 0.55f) / 0.45f);
    }

#if JUCE_IOS
    static constexpr int srcFftSize = GOODMETERAudioProcessor::spectrogramSourceFftSize;
#else
    static constexpr int srcFftSize = GOODMETERAudioProcessor::fftSize;
#endif
    static constexpr int srcBins = srcFftSize / 2;
    static constexpr int timeCols = 56;
    static constexpr int freqRidges = 28;

    GOODMETERAudioProcessor& audioProcessor;
    bool marathonDarkStyle = false;
    float adaptivePeakDb = -60.0f;
    float cameraYaw = 0.0f;      // -1 (depth left) .. +1 (depth hard right)
    float dragStartX = 0.0f;
    float dragStartYaw = 0.0f;
    int writeIndex = 0;
    int framesInColumn = 0;

    std::array<std::array<float, (size_t) freqRidges>, (size_t) timeCols> history;
    std::array<float, (size_t) freqRidges> pendingColumn;
    std::array<float, (size_t) freqRidges> pendingDb { };
    std::array<float, 2048> popScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectralTerrainComponent)
};
