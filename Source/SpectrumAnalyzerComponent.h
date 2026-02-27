/*
  ==============================================================================
    SpectrumAnalyzerComponent.h
    GOODMETER - FFT Spectrum Analyzer

    Translated from SpectrumAnalyzer.tsx
    Features: Logarithmic frequency mapping, smooth polygon fill, dB scale
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
/**
 * FFT Spectrum Analyzer Component
 * Displays frequency spectrum from 20Hz to 20kHz with logarithmic X-axis
 * Y-axis shows magnitude in dB (0 dB to -80 dB)
 */
class SpectrumAnalyzerComponent : public juce::Component,
                                   public juce::Timer
{
public:
    //==========================================================================
    SpectrumAnalyzerComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Set fixed height (from SpectrumAnalyzer.tsx)
        setSize(500, 200);

        // Start timer for FFT data updates (30Hz is sufficient for spectrum)
        startTimerHz(30);
    }

    ~SpectrumAnalyzerComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        // 🔒 JUCE 全局渲染纪律 1: 动态边界，绝不写死坐标
        auto bounds = getLocalBounds().toFloat();

        // 🔒 JUCE 全局渲染纪律 2: 安全边界判定
        if (bounds.isEmpty() || bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
            return;

        // Background
        g.fillAll(juce::Colours::white);

        // Border
        g.setColour(GoodMeterLookAndFeel::border);
        g.drawRect(bounds, 2.0f);

        // Draw spectrum polygon if we have valid FFT data
        if (hasValidData)
        {
            drawSpectrum(g, bounds);
        }

        // Draw frequency grid lines and labels
        drawFrequencyGrid(g, bounds);
    }

    void resized() override
    {
        // No child components
    }

private:
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    // FFT data storage (half of fftSize due to Nyquist)
    static constexpr int numBins = GOODMETERAudioProcessor::fftSize / 2;
    std::array<float, numBins> fftData;
    bool hasValidData = false;

    // Frequency range
    static constexpr float minFreq = 20.0f;    // 20 Hz
    static constexpr float maxFreq = 20000.0f; // 20 kHz

    // dB range
    static constexpr float minDb = -80.0f;
    static constexpr float maxDb = 0.0f;

    //==========================================================================
    void timerCallback() override
    {
        // 🎯 接通 FFT 数据总线：从 processor 的 FIFO 中 pop 最新数据
        // Try to get latest FFT data from left channel
        if (audioProcessor.fftFifoL.pop(fftData.data(), numBins))
        {
            hasValidData = true;
            repaint();
        }
    }

    //==========================================================================
    /**
     * Convert frequency (Hz) to X pixel coordinate (logarithmic scale)
     */
    float frequencyToX(float freq, float width) const
    {
        // Logarithmic interpolation: x = (log(freq) - log(minFreq)) / (log(maxFreq) - log(minFreq))
        const float logMin = std::log10(minFreq);
        const float logMax = std::log10(maxFreq);
        const float logFreq = std::log10(freq);

        const float normalized = (logFreq - logMin) / (logMax - logMin);
        return normalized * width;
    }

    /**
     * Convert FFT bin index to frequency (Hz)
     */
    float binToFrequency(int bin) const
    {
        const float sampleRate = static_cast<float>(audioProcessor.getSampleRate());
        return (bin * sampleRate) / static_cast<float>(GOODMETERAudioProcessor::fftSize);
    }

    /**
     * Convert magnitude to dB
     */
    float magnitudeToDb(float magnitude) const
    {
        return 20.0f * std::log10(magnitude + 1e-8f);
    }

    /**
     * Convert dB to Y pixel coordinate (0 dB at top, -80 dB at bottom)
     */
    float dbToY(float db, float height) const
    {
        const float clamped = juce::jlimit(minDb, maxDb, db);
        const float normalized = (maxDb - clamped) / (maxDb - minDb);
        return normalized * height;
    }

    //==========================================================================
    /**
     * Draw smooth spectrum polygon with gradient fill
     */
    void drawSpectrum(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float width = bounds.getWidth();
        const float height = bounds.getHeight();

        // 🎨 创建平滑的多边形路径
        juce::Path spectrumPath;

        // Start at bottom-left corner
        spectrumPath.startNewSubPath(bounds.getX(), bounds.getBottom());

        // Iterate through FFT bins and map to screen coordinates
        for (int bin = 1; bin < numBins; ++bin)
        {
            const float freq = binToFrequency(bin);

            // Only draw frequencies in visible range (20Hz - 20kHz)
            if (freq < minFreq || freq > maxFreq)
                continue;

            const float magnitude = fftData[bin];
            const float db = magnitudeToDb(magnitude);

            const float x = bounds.getX() + frequencyToX(freq, width);
            const float y = bounds.getY() + dbToY(db, height);

            spectrumPath.lineTo(x, y);
        }

        // Close path at bottom-right corner
        spectrumPath.lineTo(bounds.getRight(), bounds.getBottom());
        spectrumPath.closeSubPath();

        // 🎨 Fill with semi-transparent cyan (SpectrumAnalyzer.tsx style)
        g.setColour(GoodMeterLookAndFeel::accentCyan.withAlpha(0.3f));
        g.fillPath(spectrumPath);

        // 🎨 Stroke with solid cyan line for definition
        g.setColour(GoodMeterLookAndFeel::accentCyan);
        g.strokePath(spectrumPath, juce::PathStrokeType(2.0f));
    }

    //==========================================================================
    /**
     * Draw frequency grid lines and labels
     */
    void drawFrequencyGrid(juce::Graphics& g, const juce::Rectangle<float>& bounds)
    {
        const float width = bounds.getWidth();

        // Major frequency markers (logarithmically spaced)
        const float frequencies[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                     1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

        g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.2f));
        g.setFont(juce::Font(10.0f));

        for (float freq : frequencies)
        {
            const float x = bounds.getX() + frequencyToX(freq, width);

            // Vertical grid line
            g.drawVerticalLine(static_cast<int>(x), bounds.getY(), bounds.getBottom());

            // Frequency label
            juce::String label;
            if (freq >= 1000.0f)
                label = juce::String(freq / 1000.0f, 1) + "k";
            else
                label = juce::String(static_cast<int>(freq));

            g.setColour(GoodMeterLookAndFeel::textMuted);
            g.drawText(label,
                      static_cast<int>(x - 15), static_cast<int>(bounds.getBottom() - 20),
                      30, 16,
                      juce::Justification::centred, false);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpectrumAnalyzerComponent)
};
