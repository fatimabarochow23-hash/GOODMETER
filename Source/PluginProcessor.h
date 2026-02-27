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
#include <array>

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
        const auto currentWrite = writeIndex.load(std::memory_order_acquire);
        const auto nextWrite = (currentWrite + 1) % Size;

        if (nextWrite == readIndex.load(std::memory_order_acquire))
            return false; // Buffer full

        std::copy(data, data + numSamples, buffer[currentWrite].data());
        writeIndex.store(nextWrite, std::memory_order_release);
        return true;
    }

    bool pop(T* dest, size_t numSamples)
    {
        const auto currentRead = readIndex.load(std::memory_order_acquire);

        if (currentRead == writeIndex.load(std::memory_order_acquire))
            return false; // Buffer empty

        std::copy(buffer[currentRead].begin(), buffer[currentRead].begin() + numSamples, dest);
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

    // Phase Correlation (-1.0 to +1.0)
    std::atomic<float> phaseCorrelation { 0.0f };

    // Stereo Field RMS (M/L/R/S)
    std::atomic<float> rmsLevelMid { -90.0f };
    std::atomic<float> rmsLevelSide { -90.0f };

    // FFT Data (lock-free FIFO)
    LockFreeFIFO<float, 4> fftFifoL;
    LockFreeFIFO<float, 4> fftFifoR;

    // FFT Engine
    static constexpr int fftOrder = 12; // 2^12 = 4096
    static constexpr int fftSize = 1 << fftOrder;

private:
    //==============================================================================
    // Internal DSP state (WRITE-ONLY from audio thread)
    //==============================================================================

    // K-Weighting filters for LUFS
    KWeightingFilter kWeightingL;
    KWeightingFilter kWeightingR;

    // LUFS circular buffer (400ms = 19200 samples at 48kHz)
    static constexpr int lufsBufferSize = 32768;
    std::array<float, lufsBufferSize> lufsBufferL;
    std::array<float, lufsBufferSize> lufsBufferR;
    int lufsBufferIndex = 0;

    // FFT accumulation buffers
    std::array<float, fftSize> fftBufferL;
    std::array<float, fftSize> fftBufferR;
    int fftBufferIndex = 0;

    // FFT engine
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    // Sample rate
    double currentSampleRate = 48000.0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GOODMETERAudioProcessor)
};
