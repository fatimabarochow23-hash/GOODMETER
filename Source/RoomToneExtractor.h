/*
  ==============================================================================
    RoomToneExtractor.h
    GOODMETER - Room Tone Extraction Engine (Pure DSP)

    Pipeline:
      1. VAD: detect silent segments (energy + spectral flatness)
      2. Spectral envelope: FFT on silent segments → average magnitude
      3. Synthesis: filter white noise with spectral envelope (overlap-add)

    All functions are static — no persistent state needed.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

//==============================================================================
class RoomToneExtractor
{
public:
    //==========================================================================
    // Types
    //==========================================================================
    struct SilentSegment
    {
        int startSample;
        int endSample;
    };

    static constexpr int fftOrder = 12;          // 2^12 = 4096
    static constexpr int fftSize = 4096;
    static constexpr int halfFFT = fftSize / 2;  // 2048 bins
    static constexpr int hopSize = 1024;          // 75% overlap

    //==========================================================================
    // 1. VAD — Voice Activity Detection (energy + spectral flatness)
    //==========================================================================
    /** Mix multi-channel audio down to mono (average all channels).
     *  If already mono, returns a lightweight copy of channel 0. */
    static juce::AudioBuffer<float> mixToMono(const juce::AudioBuffer<float>& audio)
    {
        int numCh = audio.getNumChannels();
        int numSamples = audio.getNumSamples();

        juce::AudioBuffer<float> mono(1, numSamples);

        if (numCh == 1)
        {
            mono.copyFrom(0, 0, audio, 0, 0, numSamples);
        }
        else
        {
            mono.clear();
            float invCh = 1.0f / static_cast<float>(numCh);
            for (int ch = 0; ch < numCh; ++ch)
                mono.addFrom(0, 0, audio, ch, 0, numSamples, invCh);
        }

        return mono;
    }

    static std::vector<SilentSegment> detectSilentSegments(
        const juce::AudioBuffer<float>& audio, double sampleRate)
    {
        juce::ignoreUnused(sampleRate);

        const int totalSamples = audio.getNumSamples();
        if (totalSamples < fftSize) return {};

        // Mix to mono for analysis (handles 5ch, stereo, etc.)
        auto mono = mixToMono(audio);
        const float* data = mono.getReadPointer(0);

        const float energyThresholdDb = -35.0f;   // relaxed from -40 for production audio
        const float spectralFlatnessMin = 0.2f;    // relaxed from 0.3
        const int minSilentFrames = 4;             // relaxed from 10 (~80ms minimum)

        juce::dsp::FFT fft(fftOrder);
        juce::dsp::WindowingFunction<float> window(
            fftSize, juce::dsp::WindowingFunction<float>::hann);

        // Scan frames — also record RMS for fallback ranking
        struct FrameInfo { bool isSilent; int startSample; float rmsDb; };
        std::vector<FrameInfo> frames;

        for (int pos = 0; pos + fftSize <= totalSamples; pos += hopSize)
        {
            // Compute RMS
            float sumSq = 0.0f;
            for (int i = 0; i < fftSize; ++i)
            {
                float s = data[pos + i];
                sumSq += s * s;
            }
            float rms = std::sqrt(sumSq / static_cast<float>(fftSize));
            float rmsDb = (rms > 1e-10f)
                ? 20.0f * std::log10(rms) : -120.0f;

            // Compute spectral flatness
            std::array<float, fftSize * 2> fftBuf = {};
            for (int i = 0; i < fftSize; ++i)
                fftBuf[static_cast<size_t>(i)] = data[pos + i];
            window.multiplyWithWindowingTable(fftBuf.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftBuf.data());

            // Spectral flatness = exp(mean(log(mag))) / mean(mag)
            float logSum = 0.0f;
            float magSum = 0.0f;
            for (int i = 1; i < halfFFT; ++i)  // skip DC
            {
                float mag = fftBuf[static_cast<size_t>(i)];
                if (mag < 1e-10f) mag = 1e-10f;
                logSum += std::log(mag);
                magSum += mag;
            }
            float n = static_cast<float>(halfFFT - 1);
            float geoMean = std::exp(logSum / n);
            float arithMean = magSum / n;
            float flatness = (arithMean > 1e-10f) ? geoMean / arithMean : 0.0f;

            bool silent = (rmsDb < energyThresholdDb && flatness > spectralFlatnessMin);
            frames.push_back({ silent, pos, rmsDb });
        }

        // Merge consecutive silent frames into segments
        std::vector<SilentSegment> segments;
        int runStart = -1;
        int runCount = 0;

        for (size_t i = 0; i < frames.size(); ++i)
        {
            if (frames[i].isSilent)
            {
                if (runStart < 0) runStart = static_cast<int>(i);
                runCount++;
            }
            else
            {
                if (runCount >= minSilentFrames)
                {
                    segments.push_back({
                        frames[static_cast<size_t>(runStart)].startSample,
                        frames[i - 1].startSample + fftSize
                    });
                }
                runStart = -1;
                runCount = 0;
            }
        }
        // Handle trailing silent segment
        if (runCount >= minSilentFrames)
        {
            segments.push_back({
                frames[static_cast<size_t>(runStart)].startSample,
                frames.back().startSample + fftSize
            });
        }

        // ── Fallback: if no silent segments found, pick the quietest N frames ──
        if (segments.empty() && !frames.empty())
        {
            // Sort frame indices by RMS (quietest first)
            std::vector<size_t> indices(frames.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(), indices.end(),
                [&frames](size_t a, size_t b) {
                    return frames[a].rmsDb < frames[b].rmsDb;
                });

            // Take quietest 10% of frames (minimum 8 frames)
            int fallbackCount = juce::jmax(8, static_cast<int>(frames.size()) / 10);
            fallbackCount = juce::jmin(fallbackCount, static_cast<int>(frames.size()));

            for (int i = 0; i < fallbackCount; ++i)
            {
                auto idx = indices[static_cast<size_t>(i)];
                int startSample = frames[idx].startSample;
                segments.push_back({ startSample, startSample + fftSize });
            }
        }

        return segments;
    }

    //==========================================================================
    // 2. Spectral envelope extraction (average magnitude over silent segments)
    //==========================================================================
    static std::array<float, halfFFT> extractSpectralEnvelope(
        const juce::AudioBuffer<float>& audio,
        const std::vector<SilentSegment>& silentSegments,
        double sampleRate)
    {
        juce::ignoreUnused(sampleRate);

        std::array<float, halfFFT> avgMagnitude = {};
        int frameCount = 0;

        if (silentSegments.empty()) return avgMagnitude;

        // Mix to mono for spectral analysis
        auto mono = mixToMono(audio);

        juce::dsp::FFT fft(fftOrder);
        juce::dsp::WindowingFunction<float> window(
            fftSize, juce::dsp::WindowingFunction<float>::hann);

        // Running average for transient detection
        float runningAvgEnergy = 0.0f;
        int energyCount = 0;

        for (const auto& seg : silentSegments)
        {
            for (int pos = seg.startSample;
                 pos + fftSize <= seg.endSample;
                 pos += hopSize)
            {
                std::array<float, fftSize * 2> fftBuf = {};
                for (int i = 0; i < fftSize; ++i)
                    fftBuf[static_cast<size_t>(i)] = mono.getSample(0, pos + i);

                window.multiplyWithWindowingTable(fftBuf.data(), fftSize);
                fft.performFrequencyOnlyForwardTransform(fftBuf.data());

                // Transient detection: skip frames with energy >> running average
                float frameEnergy = 0.0f;
                for (int i = 0; i < halfFFT; ++i)
                    frameEnergy += fftBuf[static_cast<size_t>(i)]
                                 * fftBuf[static_cast<size_t>(i)];

                if (energyCount > 3 && runningAvgEnergy > 1e-10f
                    && frameEnergy / runningAvgEnergy > 3.0f)
                {
                    continue;  // skip transient frame
                }

                // Update running average
                runningAvgEnergy = (runningAvgEnergy * static_cast<float>(energyCount)
                                   + frameEnergy)
                                   / static_cast<float>(energyCount + 1);
                energyCount++;

                // Accumulate magnitude
                for (int i = 0; i < halfFFT; ++i)
                    avgMagnitude[static_cast<size_t>(i)]
                        += fftBuf[static_cast<size_t>(i)];
                frameCount++;
            }
        }

        // Average
        if (frameCount > 0)
        {
            float invCount = 1.0f / static_cast<float>(frameCount);
            for (auto& v : avgMagnitude) v *= invCount;
        }

        return avgMagnitude;
    }

    //==========================================================================
    // 2b. Measure actual time-domain RMS of silent segments (for calibration)
    //==========================================================================
    static float measureNoiseFloorRms(
        const juce::AudioBuffer<float>& audio,
        const std::vector<SilentSegment>& silentSegments)
    {
        auto mono = mixToMono(audio);
        const float* data = mono.getReadPointer(0);
        int totalSamples = mono.getNumSamples();

        float sumSq = 0.0f;
        int count = 0;

        for (const auto& seg : silentSegments)
        {
            for (int s = seg.startSample; s < seg.endSample && s < totalSamples; ++s)
            {
                sumSq += data[s] * data[s];
                count++;
            }
        }

        if (count == 0) return 0.0f;
        return std::sqrt(sumSq / static_cast<float>(count));
    }

    //==========================================================================
    // 3. Room Tone synthesis (overlap-add: random phase + spectral envelope)
    //     noiseFloorRms: if > 0, calibrate each channel's RMS to this value
    //                    (scaled down by 0.75 to ensure room tone <= original noise)
    //==========================================================================
    static juce::AudioBuffer<float> synthesizeRoomTone(
        const std::array<float, halfFFT>& spectralEnvelope,
        double sampleRate,
        float durationSeconds,
        int numChannels = 2,
        float noiseFloorRms = -1.0f)
    {
        int totalSamples = static_cast<int>(sampleRate * durationSeconds);
        if (totalSamples <= 0) totalSamples = static_cast<int>(sampleRate * 30.0);
        if (numChannels < 1) numChannels = 1;

        juce::AudioBuffer<float> output(numChannels, totalSamples);
        output.clear();

        juce::dsp::FFT fft(fftOrder);

        // Check if envelope has any energy
        float envSum = 0.0f;
        for (auto v : spectralEnvelope) envSum += v;
        if (envSum < 1e-10f) return output;  // no noise detected, return silence

        juce::dsp::WindowingFunction<float> window(
            fftSize, juce::dsp::WindowingFunction<float>::hann);

        // ── High-pass filter the spectral envelope ──
        // Roll off below ~80 Hz to prevent boomy low-frequency buildup.
        // At 48kHz/4096 = 11.72 Hz per bin → 80 Hz ≈ bin 7
        float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        int hpfBin = juce::jmax(1, static_cast<int>(80.0f / binHz));
        std::array<float, halfFFT> filteredEnvelope = spectralEnvelope;
        for (int i = 0; i < hpfBin && i < halfFFT; ++i)
        {
            float ratio = static_cast<float>(i) / static_cast<float>(hpfBin);
            filteredEnvelope[static_cast<size_t>(i)] *= ratio * ratio;  // quadratic roll-off
        }
        // Also kill DC completely
        filteredEnvelope[0] = 0.0f;

        // ── Target RMS ──
        // If measured noise floor provided, use it (scaled to 75% for safety margin).
        // Otherwise fall back to envelope-derived estimate.
        float targetRms;
        if (noiseFloorRms > 0.0f)
            targetRms = noiseFloorRms * 0.75f;
        else
        {
            targetRms = 0.0f;
            for (auto v : filteredEnvelope) targetRms += v * v;
            targetRms = std::sqrt(targetRms / static_cast<float>(halfFFT)) * 0.03f;
        }

        // Synthesize EACH channel independently with its own random phases
        // This avoids comb filtering caused by time-shifted copies
        for (int ch = 0; ch < numChannels; ++ch)
        {
            juce::Random rng(static_cast<int64_t>(ch + 1) * 31337);  // unique seed per channel
            int workLen = totalSamples + fftSize;
            std::vector<float> workspace(static_cast<size_t>(workLen), 0.0f);

            for (int pos = 0; pos + fftSize <= workLen; pos += hopSize)
            {
                // Build frequency-domain frame: envelope magnitude + random phase
                std::array<float, fftSize * 2> frame = {};

                for (int bin = 0; bin < halfFFT; ++bin)
                {
                    float magnitude = filteredEnvelope[static_cast<size_t>(bin)];
                    float phase = rng.nextFloat() * juce::MathConstants<float>::twoPi;

                    frame[static_cast<size_t>(bin * 2)]     = magnitude * std::cos(phase);
                    frame[static_cast<size_t>(bin * 2 + 1)] = magnitude * std::sin(phase);
                }

                // IFFT
                fft.performRealOnlyInverseTransform(frame.data());

                // Windowing + overlap-add
                window.multiplyWithWindowingTable(frame.data(), fftSize);

                for (int i = 0; i < fftSize && (pos + i) < workLen; ++i)
                    workspace[static_cast<size_t>(pos + i)] += frame[static_cast<size_t>(i)];
            }

            // Normalize: match RMS to envelope RMS
            float rmsOut = 0.0f;
            for (int i = 0; i < totalSamples; ++i)
                rmsOut += workspace[static_cast<size_t>(i)]
                        * workspace[static_cast<size_t>(i)];
            rmsOut = std::sqrt(rmsOut / static_cast<float>(totalSamples));

            if (rmsOut > 1e-10f)
            {
                float gain = targetRms / rmsOut;
                for (int i = 0; i < totalSamples; ++i)
                    workspace[static_cast<size_t>(i)] *= gain;
            }

            // Copy to this channel
            for (int i = 0; i < totalSamples; ++i)
                output.setSample(ch, i, workspace[static_cast<size_t>(i)]);
        }

        return output;
    }
};
