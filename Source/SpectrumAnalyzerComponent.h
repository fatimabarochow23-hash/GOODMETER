/*
  ==============================================================================
    SpectrumAnalyzerComponent.h
    GOODMETER - FFT Spectrum Analyzer

    Commercial-Grade 75% Overlap + Independent GUI Lerp Architecture:
    - Backend: 75% overlap (hop=1024) → ~43Hz FFT frame rate
    - FIFO: 16-slot lock-free ring buffer, zero contention with Spectrogram
    - Frontend: targetData / smoothedData separation, 60Hz independent lerp
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
class SpectrumAnalyzerComponent : public juce::Component,
                                   public juce::Timer
{
public:
    //==========================================================================
    SpectrumAnalyzerComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        targetData.fill(0.0f);
        smoothedData.fill(0.0f);
        tempBuffer.fill(0.0f);

        setSize(100, 200);
        startTimerHz(60);
    }

    ~SpectrumAnalyzerComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        if (bounds.isEmpty() || bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
            return;

        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(bounds, GoodMeterLookAndFeel::chartStroke(2.0f, 1.18f, 2.3f));

        if (hasValidData)
            drawSpectrum(g, bounds);

        drawFrequencyGrid(g, bounds);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();

        if (bounds.isEmpty() || bounds.getWidth() <= 0)
            return;

        const float width = bounds.getWidth();

        cachedXCoords.resize(numBins);
        for (int bin = 0; bin < numBins; ++bin)
        {
            const float freq = binToFrequency(bin);
            cachedXCoords[bin] = bounds.getX() + frequencyToX(freq, width);
        }
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    static constexpr int numBins = GOODMETERAudioProcessor::fftSize / 2;

    // Three-tier data architecture:
    // tempBuffer  → raw FIFO drain buffer (transient, overwritten each pop)
    // targetData  → latest FFT snapshot (the "truth" the display chases)
    // smoothedData → what actually gets rendered (lerps toward targetData every frame)
    std::array<float, numBins> tempBuffer;
    std::array<float, numBins> targetData;
    std::array<float, numBins> smoothedData;
    bool hasValidData = false;

    // X coordinate lookup table (recomputed only on resize)
    std::vector<float> cachedXCoords;

    // Frequency range
    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;

    // Y axis dynamic range
    static constexpr float minDb = -100.0f;
    static constexpr float maxDb = 6.0f;

    // Offscreen text cache (STATIC — rebuild on resize only)
    juce::Image freqGridTextCache;
    int lastFreqGridW = 0, lastFreqGridH = 0;
    float lastFreqGridScale = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // 60Hz → 30Hz smart throttle during mouse drag
        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
        {
            static int dragThrottleCounter = 0;
            if (++dragThrottleCounter % 2 != 0) return;
        }

        // === 1. Flush FIFO: drain everything, keep only the latest frame ===
        bool gotNewData = false;
        while (audioProcessor.fftFifoL.pop(tempBuffer.data(), numBins))
        {
            std::copy(tempBuffer.begin(), tempBuffer.end(), targetData.begin());
            gotNewData = true;
        }

        if (gotNewData)
            hasValidData = true;

        // === 2. Independent GUI lerp: ALWAYS runs, even without new FFT data ===
        // smoothedData chases targetData at 35% per frame → silky 60Hz animation
        const float smoothing = 0.35f;
        for (int i = 0; i < numBins; ++i)
        {
            smoothedData[i] += (targetData[i] - smoothedData[i]) * smoothing;
        }

        // === 3. Unconditional repaint: full 60Hz visual refresh ===
        repaint();
    }

    //==========================================================================
    float frequencyToX(float freq, float width) const
    {
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(maxFreq);
        const float logFreq = std::log10(freq);

        return ((logFreq - logMin) / (logMax - logMin)) * width;
    }

    float binToFrequency(int bin) const
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        return (bin * sampleRate) / static_cast<float>(GOODMETERAudioProcessor::fftSize);
    }

    float dbToY(float db, float height, float topY) const
    {
        const float topPadding = height * 0.2f;
        return juce::jmap(db, minDb, maxDb, topY + height, topY + topPadding);
    }

    //==========================================================================
    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float height = bounds.getHeight();
        const float topY = bounds.getY();

        if (cachedXCoords.empty())
            return;

        juce::Path spectrumPath;
        spectrumPath.startNewSubPath(bounds.getX(), bounds.getBottom());

        const int maxPoints = 250;
        const int step = juce::jmax(1, numBins / maxPoints);

        for (int bin = 1; bin < numBins; bin += step)
        {
            if (bin >= static_cast<int>(cachedXCoords.size()))
                break;

            const float freq = binToFrequency(bin);
            if (freq < minFreq || freq > maxFreq)
                continue;

            const float x = cachedXCoords[bin];
            const float rawMagnitude = smoothedData[bin];
            const float scaledAmplitude = rawMagnitude / static_cast<float>(GOODMETERAudioProcessor::fftSize);
            const float db = juce::Decibels::gainToDecibels(scaledAmplitude, -100.0f);
            const float y = dbToY(db, height, topY);

            spectrumPath.lineTo(x, y);
        }

        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.closeSubPath();

        // Gradient fill
        juce::ColourGradient gradient(
            GoodMeterLookAndFeel::accentPink.withAlpha(GoodMeterLookAndFeel::isMobileCharts() ? 0.24f : 0.4f),
            bounds.getCentreX(), bounds.getY(),
            GoodMeterLookAndFeel::accentPink.withAlpha(0.0f),
            bounds.getCentreX(), bounds.getBottom(),
            false
        );
        g.setGradientFill(gradient);
        g.fillPath(spectrumPath);

        // Stroke
        g.setColour(GoodMeterLookAndFeel::accentPink);
        g.strokePath(spectrumPath, juce::PathStrokeType(
            GoodMeterLookAndFeel::chartStroke(2.0f, 1.18f, 2.3f)));
    }

    //==========================================================================
    void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        int bw = static_cast<int>(bounds.getWidth());
        int bh = static_cast<int>(bounds.getHeight());
        if (bw < 4 || bh < 4) return;

        auto drawGridInto = [this, bw, bh](juce::Graphics& tg)
        {
            const float width = static_cast<float>(bw);
            const float height = static_cast<float>(bh);

            const float dbLines[] = { -90.0f, -80.0f, -70.0f, -60.0f, -50.0f,
                                      -40.0f, -30.0f, -20.0f, -10.0f, 0.0f };

            tg.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(9.0f)));
            for (float db : dbLines)
            {
                float y = dbToY(db, height, 0.0f);
                if (y < 0.0f || y > height) continue;

                // Subtle horizontal line
                tg.setColour(GoodMeterLookAndFeel::chartInk(0.16f));
                tg.drawHorizontalLine(static_cast<int>(y), 0.0f, width);

                // dB label on the left
                tg.setColour(GoodMeterLookAndFeel::chartMuted());
                tg.drawText(juce::String(static_cast<int>(db)),
                           2, static_cast<int>(y - 6), 28, 12,
                           juce::Justification::centredLeft, false);
            }

            const float frequencies[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                         1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

            auto getFreqColour = [](float freq) -> juce::Colour
            {
                if (std::abs(freq - 100.0f)   < 1.0f) return juce::Colour(0xFF00D084);  // green
                if (std::abs(freq - 1000.0f)  < 1.0f) return juce::Colour(0xFFF5A623);  // yellow
                if (std::abs(freq - 10000.0f) < 1.0f) return juce::Colour(0xFFFF6600);  // orange
                return juce::Colour(0x00000000);  // no accent
            };

            tg.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(10.0f)));

            for (float freq : frequencies)
            {
                const float x = frequencyToX(freq, width);
                auto accent = getFreqColour(freq);
                bool hasAccent = accent.getAlpha() > 0;

                if (hasAccent)
                {
                    if (GoodMeterLookAndFeel::isMobileCharts())
                    {
                        tg.setColour(accent.withAlpha(0.72f));
                        tg.drawVerticalLine(static_cast<int>(x), 0.0f, height);
                    }
                    else
                    {
                        // Glow layers: wide soft → narrow bright
                        tg.setColour(accent.withAlpha(0.06f));
                        tg.fillRect(x - 4.0f, 0.0f, 8.0f, height);
                        tg.setColour(accent.withAlpha(0.15f));
                        tg.fillRect(x - 1.5f, 0.0f, 3.0f, height);
                        tg.setColour(accent.withAlpha(0.5f));
                        tg.drawVerticalLine(static_cast<int>(x), 0.0f, height);
                    }
                }
                else
                {
                    tg.setColour(GoodMeterLookAndFeel::chartInk(0.18f));
                    tg.drawVerticalLine(static_cast<int>(x), 0.0f, height);
                }

                // Frequency label (no decimals: "1k" not "1.0k")
                juce::String label;
                if (freq >= 1000.0f)
                    label = juce::String(static_cast<int>(freq / 1000.0f)) + "k";
                else
                    label = juce::String(static_cast<int>(freq));

                int labelX = juce::jlimit(0, bw - 30, static_cast<int>(x - 15));
                tg.setColour(hasAccent ? accent.withAlpha(0.9f) : GoodMeterLookAndFeel::chartMuted());
                tg.drawText(label,
                          labelX, bh - 20,
                          30, 16,
                          juce::Justification::centred, false);
            }
        };

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            juce::Graphics::ScopedSaveState state(g);
            g.addTransform(juce::AffineTransform::translation(bounds.getX(), bounds.getY()));
            drawGridInto(g);
        }
        else
        {
            const float scale = juce::Component::getApproximateScaleFactorForComponent(this);

            if (freqGridTextCache.isNull() || lastFreqGridW != bw || lastFreqGridH != bh
                || std::abs(lastFreqGridScale - scale) > 0.01f)
            {
                lastFreqGridW = bw;
                lastFreqGridH = bh;
                lastFreqGridScale = scale;
                freqGridTextCache = juce::Image(juce::Image::ARGB,
                                                juce::jmax(1, juce::roundToInt(static_cast<float>(bw) * scale)),
                                                juce::jmax(1, juce::roundToInt(static_cast<float>(bh) * scale)),
                                                true, juce::SoftwareImageType());
                juce::Graphics tg(freqGridTextCache);
                tg.addTransform(juce::AffineTransform::scale(scale));
                drawGridInto(tg);
            }

            g.drawImage(freqGridTextCache,
                        static_cast<int>(bounds.getX()), static_cast<int>(bounds.getY()),
                        bw, bh,
                        0, 0,
                        freqGridTextCache.getWidth(), freqGridTextCache.getHeight());
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzerComponent)
};
