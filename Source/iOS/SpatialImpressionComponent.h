/*
  ==============================================================================
    SpatialImpressionComponent.h
    GOODMETER iOS - live "Spatial Impression L-C-R" view, ported from
    Audio Doctor's spatial impression figure (same projection, palette and
    draw order), but driven in real time by the processor's spatialGridFifo
    (pan x log-frequency energy grid built per FFT hop).

    Replaces the STEREO card content on iOS. Header-only.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"

class SpatialImpressionComponent : public juce::Component,
                                   private juce::Timer
{
public:
    explicit SpatialImpressionComponent(GOODMETERAudioProcessor& proc)
        : audioProcessor(proc)
    {
        smoothed.fill(0.0f);
        popScratch.fill(0.0f);
        setInterceptsMouseClicks(false, false);
        startTimerHz(30);
    }

    ~SpatialImpressionComponent() override { stopTimer(); }

    void setMarathonDarkStyle(bool dark) { marathonDarkStyle = dark; repaint(); }

    void paint(juce::Graphics& g) override
    {
        const bool dark = marathonDarkStyle;
        auto plot = getLocalBounds().toFloat().reduced(4.0f, 4.0f);

        g.setColour(dark ? juce::Colour(0xFF0C1118) : juce::Colour(0xFFF5F7FA));
        g.fillRoundedRectangle(plot, 6.0f);
        g.setColour(dark ? juce::Colours::white.withAlpha(0.18f) : juce::Colour(0xFFD7DEE8));
        g.drawRoundedRectangle(plot, 6.0f, 1.0f);

        auto inner = plot.reduced(8.0f, 4.0f);

        // ── Audio Doctor projection, camera raised for the phone cards ──
        const float baseWidth = inner.getWidth() * 0.80f;
        const float depthX = inner.getWidth() * 0.14f;
        const float depthY = inner.getHeight() * 0.66f;
        const float heightScale = inner.getHeight() * 0.24f;
        const float originX = inner.getX() + inner.getWidth() * 0.03f;
        const float originY = inner.getBottom() - inner.getHeight() * 0.10f;

        auto project = [&](float xNorm, float fNorm, float value) -> juce::Point<float>
        {
            const float perspective = 1.0f - fNorm * 0.18f;
            return { originX + xNorm * baseWidth * perspective + fNorm * depthX
                         + (1.0f - perspective) * baseWidth * 0.5f,
                     originY - fNorm * depthY - juce::jlimit(0.0f, 1.0f, value) * heightScale };
        };

        juce::Graphics::ScopedSaveState clipState(g);
        g.reduceClipRegion(plot.toNearestInt());

        // Floor + grid
        const auto gridColour = dark ? juce::Colours::white.withAlpha(0.16f)
                                     : juce::Colour(0xFF64748B).withAlpha(0.20f);
        {
            juce::Path floor;
            auto p00 = project(0, 0, 0); auto p10 = project(1, 0, 0);
            auto p11 = project(1, 1, 0); auto p01 = project(0, 1, 0);
            floor.startNewSubPath(p00); floor.lineTo(p10); floor.lineTo(p11);
            floor.lineTo(p01); floor.closeSubPath();
            g.setColour(dark ? juce::Colour(0xFF111820).withAlpha(0.66f)
                             : juce::Colour(0xFFE8EDF3).withAlpha(0.88f));
            g.fillPath(floor);
            g.setColour(gridColour);
            g.strokePath(floor, juce::PathStrokeType(1.0f));

            for (int i = 0; i <= 6; ++i)
            {
                const float fN = (float) i / 6.0f;
                auto a = project(0.0f, fN, 0.0f); auto b = project(1.0f, fN, 0.0f);
                g.drawLine(a.x, a.y, b.x, b.y, i == 0 ? 1.2f : 0.6f);
            }
            for (float xN : { 0.0f, 0.5f, 1.0f })
            {
                auto a = project(xN, 0.0f, 0.0f); auto b = project(xN, 1.0f, 0.0f);
                g.drawLine(a.x, a.y, b.x, b.y, 0.8f);
            }
        }

        constexpr int P = GOODMETERAudioProcessor::spatialPanSteps;
        constexpr int F = GOODMETERAudioProcessor::spatialFreqSteps;
        auto at = [](int x, int f) -> size_t { return (size_t) (f * P + x); };

        // Surface quads, back (high freq) to front
        for (int f = F - 2; f >= 0; --f)
        {
            const float fN0 = (float) f / (float) (F - 1);
            const float fN1 = (float) (f + 1) / (float) (F - 1);
            for (int x = 0; x < P - 1; ++x)
            {
                const float v00 = smoothed[at(x, f)],     v10 = smoothed[at(x + 1, f)];
                const float v11 = smoothed[at(x + 1, f+1)], v01 = smoothed[at(x, f + 1)];
                const float avg = (v00 + v10 + v11 + v01) * 0.25f;
                if (avg <= 0.012f)
                    continue;

                const float xN0 = (float) x / (float) (P - 1);
                const float xN1 = (float) (x + 1) / (float) (P - 1);

                juce::Path quad;
                quad.startNewSubPath(project(xN0, fN0, v00));
                quad.lineTo(project(xN1, fN0, v10));
                quad.lineTo(project(xN1, fN1, v11));
                quad.lineTo(project(xN0, fN1, v01));
                quad.closeSubPath();

                auto colour = lcrColour((xN0 + xN1) * 0.5f,
                                        juce::jlimit(0.0f, 0.94f, 0.28f + avg * 0.62f));
                colour = colour.interpolatedWith(juce::Colours::white, dark ? avg * 0.18f : avg * 0.08f)
                               .interpolatedWith(juce::Colours::black, dark ? fN0 * 0.20f : fN0 * 0.05f);
                g.setColour(colour);
                g.fillPath(quad);
            }
        }

        // Front wall
        for (int x = 0; x < P - 1; ++x)
        {
            const float v0 = smoothed[at(x, 0)], v1 = smoothed[at(x + 1, 0)];
            if ((v0 + v1) <= 0.024f) continue;
            const float xN0 = (float) x / (float) (P - 1);
            const float xN1 = (float) (x + 1) / (float) (P - 1);
            juce::Path wall;
            wall.startNewSubPath(project(xN0, 0, 0));
            wall.lineTo(project(xN1, 0, 0));
            wall.lineTo(project(xN1, 0, v1));
            wall.lineTo(project(xN0, 0, v0));
            wall.closeSubPath();
            g.setColour(lcrColour((xN0 + xN1) * 0.5f, 0.42f)
                            .interpolatedWith(dark ? juce::Colours::black.withAlpha(0.18f)
                                                   : juce::Colour(0xFF94A3B8).withAlpha(0.16f), 0.32f));
            g.fillPath(wall);
        }

        // L / C / R labels
        g.setColour(dark ? juce::Colours::white.withAlpha(0.82f)
                         : juce::Colour(0xFF334155).withAlpha(0.90f));
        g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
        auto lp = project(0.0f, 0.0f, 0.0f);
        auto cp = project(0.5f, 0.0f, 0.0f);
        auto rp = project(1.0f, 0.0f, 0.0f);
        g.drawText("L", (int) lp.x - 8, (int) lp.y + 2, 16, 12, juce::Justification::centred, false);
        g.drawText("C", (int) cp.x - 8, (int) cp.y + 2, 16, 12, juce::Justification::centred, false);
        g.drawText("R", (int) rp.x - 8, (int) rp.y + 2, 16, 12, juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        if (! isShowing())
            return;

        constexpr int N = GOODMETERAudioProcessor::spatialPanSteps
                        * GOODMETERAudioProcessor::spatialFreqSteps;
        bool got = false;
        while (audioProcessor.spatialGridFifo.pop(popScratch.data(), N))
            got = true;

        if (got)
        {
            // Energy -> dB -> normalized against an adaptive peak
            float framePeak = 1.0e-12f;
            for (int i = 0; i < N; ++i)
                framePeak = juce::jmax(framePeak, popScratch[(size_t) i]);
            const float peakDb = 10.0f * std::log10(framePeak);
            adaptivePeakDb = peakDb >= adaptivePeakDb ? peakDb
                                                      : juce::jmax(peakDb, adaptivePeakDb - 0.15f);

            const float floorDb = adaptivePeakDb - 36.0f;
            for (int i = 0; i < N; ++i)
            {
                const float e = popScratch[(size_t) i];
                const float db = 10.0f * std::log10(e + 1.0e-12f);
                const float v = juce::jlimit(0.0f, 1.0f, (db - floorDb) / 36.0f);
                smoothed[(size_t) i] = smoothed[(size_t) i] * 0.55f + v * 0.45f;
            }
        }
        else
        {
            for (auto& v : smoothed)
                v *= 0.90f; // gentle decay when idle
        }

        repaint();
    }

    // Audio Doctor's L-C-R palette: cyan (left) -> yellow (centre) -> red (right)
    static juce::Colour lcrColour(float panIndex, float alpha)
    {
        const auto leftC = juce::Colour(0xFF22D3EE);
        const auto centreC = juce::Colour(0xFFFFD166);
        const auto rightC = juce::Colour(0xFFE6335F);
        auto c = panIndex < 0.5f ? leftC.interpolatedWith(centreC, panIndex * 2.0f)
                                 : centreC.interpolatedWith(rightC, (panIndex - 0.5f) * 2.0f);
        return c.withAlpha(juce::jlimit(0.0f, 1.0f, alpha));
    }

    GOODMETERAudioProcessor& audioProcessor;
    bool marathonDarkStyle = false;
    float adaptivePeakDb = -60.0f;

    std::array<float, (size_t) (GOODMETERAudioProcessor::spatialPanSteps
                                * GOODMETERAudioProcessor::spatialFreqSteps)> smoothed;
    std::array<float, 2048> popScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpatialImpressionComponent)
};
