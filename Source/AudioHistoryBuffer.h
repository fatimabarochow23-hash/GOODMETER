/*
  ==============================================================================
    AudioHistoryBuffer.h
    GOODMETER - Retroactive Recording Engine

    A lock-free circular buffer that continuously captures the last N seconds
    of audio from processBlock. When the user triggers "Save Last 60s",
    the buffer is snapshot-copied and written to WAV on a background thread.

    Thread safety model:
      - Audio thread: pushSamples() — single producer, writes at writePos
      - Any thread:   exportLastSeconds() — copies snapshot, launches async WAV writer
      - No mutex, no lock, no allocation on the audio thread

    Memory: 65s × 48kHz × 2ch × 4 bytes = ~24.4 MB (constant, no growth)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <thread>

class AudioHistoryBuffer
{
public:
    static constexpr int kCapacitySeconds = 65;  // 60s max export + 5s safety margin

    AudioHistoryBuffer() = default;

    //==========================================================================
    // Called from prepareToPlay — allocates buffer for current sample rate
    //==========================================================================
    void prepare(double sampleRate)
    {
        const int totalSamples = static_cast<int>(sampleRate * kCapacitySeconds);
        buffer.setSize(2, totalSamples);
        buffer.clear();
        capacity = totalSamples;
        writePos.store(0, std::memory_order_relaxed);
        totalSamplesWritten.store(0, std::memory_order_relaxed);
        cachedSampleRate = sampleRate;
    }

    //==========================================================================
    // Audio thread: push stereo samples into circular buffer
    // Uses block memcpy via FloatVectorOperations — cache-friendly, no per-sample modulo
    //==========================================================================
    void pushSamples(const float* srcL, const float* srcR, int numSamples)
    {
        if (capacity <= 0 || srcL == nullptr)
            return;

        const int pos = writePos.load(std::memory_order_relaxed);
        float* dstL = buffer.getWritePointer(0);
        float* dstR = buffer.getWritePointer(1);
        const float* actualR = (srcR != nullptr) ? srcR : srcL;

        // Block 1: pos → end of buffer (or pos + numSamples if no wrap)
        const int block1 = juce::jmin(numSamples, capacity - pos);
        juce::FloatVectorOperations::copy(dstL + pos, srcL, block1);
        juce::FloatVectorOperations::copy(dstR + pos, actualR, block1);

        // Block 2: wrap around to start of buffer
        const int block2 = numSamples - block1;
        if (block2 > 0)
        {
            juce::FloatVectorOperations::copy(dstL, srcL + block1, block2);
            juce::FloatVectorOperations::copy(dstR, actualR + block1, block2);
        }

        writePos.store((pos + numSamples) % capacity, std::memory_order_release);
        totalSamplesWritten.fetch_add(numSamples, std::memory_order_relaxed);
    }

    //==========================================================================
    // How many seconds of valid (non-zero) audio are available
    //==========================================================================
    double getAvailableSeconds() const
    {
        if (cachedSampleRate <= 0.0 || capacity <= 0)
            return 0.0;

        const int64_t written = totalSamplesWritten.load(std::memory_order_relaxed);
        const int64_t available = juce::jmin(written, static_cast<int64_t>(capacity));
        return static_cast<double>(available) / cachedSampleRate;
    }

    //==========================================================================
    // Export last N seconds to WAV file (called from message thread)
    //
    // 1. Snapshots writePos (atomic acquire)
    // 2. Copies ring buffer segment into linear AudioBuffer (~24MB, takes <50ms)
    // 3. Launches detached thread to write WAV file (slow I/O off main thread)
    //
    // Safe because: copy completes in milliseconds, audio thread's write head
    // is 5 seconds away from the oldest data we read (65s buffer, 60s max export).
    //==========================================================================
    void exportLastSeconds(int secondsToSave, const juce::File& outputFile)
    {
        if (capacity <= 0 || cachedSampleRate <= 0.0)
            return;

        const int pos = writePos.load(std::memory_order_acquire);
        const double sr = cachedSampleRate;
        const int cap = capacity;

        // Calculate how many samples to save
        int samplesToSave = static_cast<int>(sr * secondsToSave);

        // Clamp to available data (don't export zeros from unfilled buffer)
        const int64_t written = totalSamplesWritten.load(std::memory_order_relaxed);
        const int available = static_cast<int>(juce::jmin(written, static_cast<int64_t>(cap)));
        samplesToSave = juce::jmin(samplesToSave, available);

        // Leave 5s safety margin from write head
        const int maxSafe = cap - static_cast<int>(sr * 5.0);
        samplesToSave = juce::jmin(samplesToSave, maxSafe);

        if (samplesToSave <= 0)
            return;

        // Snapshot-copy from ring buffer into linear buffer
        auto exportBuffer = std::make_shared<juce::AudioBuffer<float>>(2, samplesToSave);

        const float* srcL = buffer.getReadPointer(0);
        const float* srcR = buffer.getReadPointer(1);
        float* dstL = exportBuffer->getWritePointer(0);
        float* dstR = exportBuffer->getWritePointer(1);

        int readPos = (pos - samplesToSave + cap) % cap;

        // Block copy with wrap-around
        const int block1 = juce::jmin(samplesToSave, cap - readPos);
        juce::FloatVectorOperations::copy(dstL, srcL + readPos, block1);
        juce::FloatVectorOperations::copy(dstR, srcR + readPos, block1);

        const int block2 = samplesToSave - block1;
        if (block2 > 0)
        {
            juce::FloatVectorOperations::copy(dstL + block1, srcL, block2);
            juce::FloatVectorOperations::copy(dstR + block1, srcR, block2);
        }

        juce::Logger::outputDebugString("AudioHistoryBuffer: copying "
            + juce::String(samplesToSave) + " samples ("
            + juce::String(samplesToSave / sr, 1) + "s) to "
            + outputFile.getFullPathName());

        // Write WAV on background thread (I/O-bound, must not block GUI)
        std::thread([exportBuffer, outputFile, sr]()
        {
            outputFile.getParentDirectory().createDirectory();

            if (outputFile.existsAsFile())
                outputFile.deleteFile();

            auto stream = outputFile.createOutputStream();
            if (stream == nullptr)
            {
                juce::Logger::outputDebugString("AudioHistoryBuffer ERROR: cannot create " + outputFile.getFullPathName());
                return;
            }

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wav.createWriterFor(stream.release(), sr, 2, 24, {}, 0));

            if (writer == nullptr)
            {
                juce::Logger::outputDebugString("AudioHistoryBuffer ERROR: WAV writer creation failed");
                return;
            }

            writer->writeFromAudioSampleBuffer(*exportBuffer, 0, exportBuffer->getNumSamples());

            juce::Logger::outputDebugString("AudioHistoryBuffer: export complete — "
                + juce::String(exportBuffer->getNumSamples() / sr, 1) + "s written to "
                + outputFile.getFullPathName());

        }).detach();
    }

private:
    juce::AudioBuffer<float> buffer;
    std::atomic<int> writePos { 0 };
    int capacity = 0;
    double cachedSampleRate = 0.0;
    std::atomic<int64_t> totalSamplesWritten { 0 };
};
