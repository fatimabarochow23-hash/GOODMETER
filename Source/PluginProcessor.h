/*
  ==============================================================================
    PluginProcessor.h
    GOODMETER - Professional Audio Metering Plugin

    Company: Solaris
    Version: 1.0.0

    DSP Architecture: Lock-free, real-time safe metering engine
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "AudioRecorder.h"
#include "AudioHistoryBuffer.h"
#if JUCE_MAC && JucePlugin_Build_Standalone
#include "SystemAudioCapture.h"
#endif
#include <array>
#include <vector>
#include <algorithm>
#include <mutex>

//==============================================================================
/**
 * Lock-free FIFO for passing FFT data from audio thread to GUI thread
 */
template <typename T, size_t Size>
class LockFreeFIFO
{
public:
    LockFreeFIFO() : writeIndex(0), readIndex(0) {}

    bool push(const T* data, size_t numSamples)
    {
        // Writer owns writeIndex → relaxed load is sufficient
        const auto currentWrite = writeIndex.load(std::memory_order_relaxed);
        const auto nextWrite = (currentWrite + 1) % Size;

        // Must acquire readIndex to see consumer's latest progress
        if (nextWrite == readIndex.load(std::memory_order_acquire))
            return false; // Buffer full

        std::copy(data, data + numSamples, buffer[currentWrite].data());
        // Release: ensure buffer[] writes are visible before index advances
        writeIndex.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool pop(T* dest, size_t numSamples)
    {
        // Reader owns readIndex → relaxed load is sufficient
        const auto currentRead = readIndex.load(std::memory_order_relaxed);

        // Must acquire writeIndex to see producer's latest data
        if (currentRead == writeIndex.load(std::memory_order_acquire))
            return false; // Buffer empty

        std::copy(buffer[currentRead].begin(), buffer[currentRead].begin() + numSamples, dest);
        // Release: ensure buffer[] reads complete before index advances
        readIndex.store((currentRead + 1) % Size, std::memory_order_release);
        return true;
    }

private:
    std::array<std::array<T, 2048>, Size> buffer;
    std::atomic<size_t> writeIndex;
    std::atomic<size_t> readIndex;
};

//==============================================================================
/**
 * K-Weighting Filter (ITU-R BS.1770-4)
 * Used for LUFS measurement
 */
class KWeightingFilter
{
public:
    KWeightingFilter() = default;

    void prepare(double sampleRate)
    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = 512;
        spec.numChannels = 1;

        // ITU-R BS.1770-4 K-weighting: high-shelf @ 1500Hz +4dB, highpass @ 38Hz
        // Using IIR filters for proper high-shelf
        auto highShelfCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf(
            sampleRate, 1500.0f, 0.707f, juce::Decibels::decibelsToGain(4.0f));
        highShelf.coefficients = highShelfCoeffs;

        auto highPassCoeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass(
            sampleRate, 38.0f, 0.5f);
        highPass.coefficients = highPassCoeffs;

        highShelf.prepare(spec);
        highPass.prepare(spec);
    }

    float processSample(float sample)
    {
        return highPass.processSample(highShelf.processSample(sample));
    }

    void reset()
    {
        highShelf.reset();
        highPass.reset();
    }

private:
    juce::dsp::IIR::Filter<float> highShelf;
    juce::dsp::IIR::Filter<float> highPass;
};

//==============================================================================
/**
 * Main Audio Processor
 */
class GOODMETERAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    GOODMETERAudioProcessor();
    ~GOODMETERAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // Public atomic data for GUI thread (READ-ONLY from GUI)
    //==============================================================================

    // Peak levels (dBFS)
    std::atomic<float> peakLevelL { -90.0f };
    std::atomic<float> peakLevelR { -90.0f };

    // RMS levels (dBFS)
    std::atomic<float> rmsLevelL { -90.0f };
    std::atomic<float> rmsLevelR { -90.0f };

    // LUFS (Momentary, 400ms window)
    std::atomic<float> lufsLevel { -70.0f };

    // LUFS Short-Term (3s window)
    std::atomic<float> lufsShortTerm { -70.0f };

    // LUFS Integrated (from start of playback)
    std::atomic<float> lufsIntegrated { -70.0f };

    // LU Range (EBU Tech 3342) — calculated on timer thread
    std::atomic<float> luRange { 0.0f };

    // Phase Correlation (-1.0 to +1.0)
    std::atomic<float> phaseCorrelation { 0.0f };

    // Stereo Field RMS (M/L/R/S)
    std::atomic<float> rmsLevelMid { -90.0f };
    std::atomic<float> rmsLevelSide { -90.0f };

    // 3-Band Frequency RMS (LOW/MID/HIGH)
    std::atomic<float> rmsLevelLow { -90.0f };   // 20-250Hz
    std::atomic<float> rmsLevelMid3Band { -90.0f };  // 250-2kHz (renamed to avoid conflict)
    std::atomic<float> rmsLevelHigh { -90.0f };  // 2k-20kHz

    // Audio recorder (public — GUI thread starts/stops, audio thread pushes samples)
    AudioRecorder audioRecorder;

#if JUCE_MAC && JucePlugin_Build_Standalone
    // System audio capture via CoreAudio Process Tap (macOS 14.2+)
    std::unique_ptr<SystemAudioCapture> systemAudioCapture;
    std::atomic<bool> useSystemAudio { false };
#endif

    // Retroactive recording — always-on 60s audio history buffer
    AudioHistoryBuffer audioHistoryBuffer;

    // Export last N seconds of audio to WAV file (called from GUI)
    void exportRetrospectiveRecording(int secondsToSave = 60,
                                      const juce::File& exportDir = {});

    // FFT Data (lock-free FIFOs — separate channels for Spectrum and Spectrogram)
    LockFreeFIFO<float, 256> fftFifoL;            // Spectrum analyzer
    LockFreeFIFO<float, 256> fftFifoR;            // Spectrum analyzer
    LockFreeFIFO<float, 256> fftFifoSpectrogramL; // Spectrogram (independent)

    // Stereo Image Sample Buffer (for Goniometer/Lissajous)
    // Stores recent raw (L, R) sample pairs for XY plotting
    static constexpr int stereoSampleBufferSize = 1024;
    LockFreeFIFO<float, 256> stereoSampleFifoL;  // Left channel samples
    LockFreeFIFO<float, 256> stereoSampleFifoR;  // Right channel samples

    // FFT Engine
    static constexpr int fftOrder = 12; // 2^12 = 4096
    static constexpr int fftSize = 1 << fftOrder;

    /**
     * Calculate LRA in real-time (EBU Tech 3342)
     * Call from GUI timer thread, NOT audio thread!
     */
    void calculateLRARealtime();

    /**
     * Push a Short-Term LUFS sample into the LRA history pool.
     * Called from GUI timer every ~100ms.
     */
    void pushShortTermLUFSForLRA(float stLufs);

private:
    //==============================================================================
    // Internal DSP state (WRITE-ONLY from audio thread)
    //==============================================================================

    // K-Weighting filters for LUFS
    KWeightingFilter kWeightingL;
    KWeightingFilter kWeightingR;

    // 3-Band frequency filters (LOW/MID/HIGH)
    // LOW: 20-250Hz, MID: 250-2kHz, HIGH: 2k-20kHz
    juce::dsp::IIR::Filter<float> lowPassL_250Hz;
    juce::dsp::IIR::Filter<float> lowPassR_250Hz;
    juce::dsp::IIR::Filter<float> midHpL_250Hz;   // MID band: HP @ 250Hz
    juce::dsp::IIR::Filter<float> midHpR_250Hz;
    juce::dsp::IIR::Filter<float> midLpL_2kHz;   // MID band: LP @ 2kHz
    juce::dsp::IIR::Filter<float> midLpR_2kHz;
    juce::dsp::IIR::Filter<float> highPassL_2kHz;
    juce::dsp::IIR::Filter<float> highPassR_2kHz;

    // LUFS circular buffer (400ms = 19200 samples at 48kHz)
    static constexpr int lufsBufferSize = 32768;
    std::array<float, lufsBufferSize> lufsBufferL;
    std::array<float, lufsBufferSize> lufsBufferR;
    int lufsBufferIndex = 0;

    // FFT accumulation ring buffer (75% overlap for ~43Hz FFT frame rate)
    // performFrequencyOnlyForwardTransform requires fftSize * 2 working space
    std::array<float, fftSize> fftRingL;
    std::array<float, fftSize> fftRingR;
    int fftRingIndex = 0;
    int fftSamplesSinceLastPush = 0;
    static constexpr int fftHopSize = fftSize / 4;  // 75% overlap → hop = 1024

    // Temporary FFT working buffer (in-place transform needs fftSize * 2)
    std::array<float, fftSize * 2> fftWorkBuffer;

    // FFT engine
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    // 🎯 Stereo sample accumulation buffers (batch push to FIFO)
    std::array<float, 512> tempStereoBufL;
    std::array<float, 512> tempStereoBufR;
    int tempStereoIndex = 0;

    // Sample rate
    double currentSampleRate = 48000.0;

    // Short-Term LUFS circular buffer (3s window = ~144000 samples at 48kHz)
    static constexpr int stLufsBufferSize = 196608;  // ~4s at 48kHz for safety
    std::array<float, stLufsBufferSize> stLufsBufferL;
    std::array<float, stLufsBufferSize> stLufsBufferR;
    int stLufsBufferIndex = 0;

    // Integrated LUFS gating (BS.1770-4 with absolute + relative gating)
    // Lock-free: all data stays on audio thread. Only lufsIntegrated atomic is read by GUI.
    static constexpr int integratedBlockMaxCount = 8192;   // ~55 min at 400ms blocks
    float integratedBlockStorage[integratedBlockMaxCount];  // Fixed-size ring (no allocation)
    int integratedBlockCount = 0;                          // Number of blocks stored
    int integratedBlockSampleCount = 0;                    // Counter for 400ms blocks
    static constexpr int integratedBlockSamples400ms = 19200; // at 48kHz

    // LRA history pool (Short-Term LUFS samples, ~100ms intervals, up to 5 min)
    std::vector<float> lraHistory;
    std::mutex lraMutex;
    static constexpr int lraMaxSamples = 3000; // 5 min at 100ms intervals

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GOODMETERAudioProcessor)
};
