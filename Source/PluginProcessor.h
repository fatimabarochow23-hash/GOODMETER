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

    // 3-Band Frequency RMS (LOW/MID/HIGH)
    std::atomic<float> rmsLevelLow { -90.0f };   // 20-250Hz
    std::atomic<float> rmsLevelMid3Band { -90.0f };  // 250-2kHz (renamed to avoid conflict)
    std::atomic<float> rmsLevelHigh { -90.0f };  // 2k-20kHz

    // FFT Data (lock-free FIFO)
    LockFreeFIFO<float, 4> fftFifoL;
    LockFreeFIFO<float, 4> fftFifoR;

    // Stereo Image Sample Buffer (for Goniometer/Lissajous)
    // Stores recent raw (L, R) sample pairs for XY plotting
    static constexpr int stereoSampleBufferSize = 1024;
    LockFreeFIFO<float, 4> stereoSampleFifoL;  // Left channel samples
    LockFreeFIFO<float, 4> stereoSampleFifoR;  // Right channel samples

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

    // 3-Band frequency filters (LOW/MID/HIGH)
    // LOW: 20-250Hz, MID: 250-2kHz, HIGH: 2k-20kHz
    juce::dsp::IIR::Filter<float> lowPassL_250Hz;
    juce::dsp::IIR::Filter<float> lowPassR_250Hz;
    juce::dsp::IIR::Filter<float> bandPassL_250_2k;
    juce::dsp::IIR::Filter<float> bandPassR_250_2k;
    juce::dsp::IIR::Filter<float> highPassL_2kHz;
    juce::dsp::IIR::Filter<float> highPassR_2kHz;

    // LUFS circular buffer (400ms = 19200 samples at 48kHz)
    static constexpr int lufsBufferSize = 32768;
    std::array<float, lufsBufferSize> lufsBufferL;
    std::array<float, lufsBufferSize> lufsBufferR;
    int lufsBufferIndex = 0;

    // FFT accumulation buffers
    // IMPORTANT: performFrequencyOnlyForwardTransform requires fftSize * 2 elements!
    // First fftSize elements are input data, second fftSize elements are used as working memory
    std::array<float, fftSize * 2> fftBufferL;
    std::array<float, fftSize * 2> fftBufferR;
    int fftBufferIndex = 0;

    // FFT engine
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { fftSize, juce::dsp::WindowingFunction<float>::hann };

    // 🎯 Stereo sample accumulation buffers (batch push to FIFO)
    std::array<float, 512> tempStereoBufL;
    std::array<float, 512> tempStereoBufR;
    int tempStereoIndex = 0;

    // Sample rate
    double currentSampleRate = 48000.0;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GOODMETERAudioProcessor)
};
