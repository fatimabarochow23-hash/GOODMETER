/*
  ==============================================================================
    AudioRecorder.h
    GOODMETER - Lock-free audio recorder

    Architecture:
      - processBlock() on the audio thread pushes samples into a lock-free FIFO
      - A background Thread drains the FIFO and writes 24-bit WAV via JUCE
      - start()/stop() are safe to call from the GUI thread
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class AudioRecorder : public juce::Thread
{
public:
    AudioRecorder()
        : Thread("GOODMETER-Recorder"),
          fifo(fifoSize)
    {
    }

    ~AudioRecorder() override
    {
        stop();
    }

    //==========================================================================
    /** Start recording to a file. Call from GUI thread.
     *  @param file       Target WAV file
     *  @param sampleRate Current audio sample rate
     *  @param numCh      Number of channels (1 or 2)
     */
    bool start(const juce::File& file, double sampleRate, int numCh)
    {
        if (isRecording.load()) return false;

        numChannels = juce::jlimit(1, 2, numCh);
        currentSampleRate = sampleRate;

        // Delete existing file
        if (file.existsAsFile())
            file.deleteFile();

        // Create output stream
        auto fos = file.createOutputStream();
        if (fos == nullptr) return false;

        // Create WAV writer (24-bit)
        juce::WavAudioFormat wavFormat;
        writer.reset(wavFormat.createWriterFor(
            fos.release(),   // writer takes ownership
            sampleRate,
            static_cast<unsigned int>(numChannels),
            24,              // bits per sample
            {},              // metadata
            0));

        if (writer == nullptr) return false;

        // Reset FIFO
        fifo.reset();
        fifoOverrun = false;

        // Start writer thread
        isRecording.store(true);
        startThread(juce::Thread::Priority::normal);

        recordingFile = file;
        return true;
    }

    /** Stop recording. Call from GUI thread. */
    void stop()
    {
        isRecording.store(false);

        // Wait for writer thread to finish
        if (isThreadRunning())
            stopThread(3000);

        // Flush remaining samples
        if (writer != nullptr)
        {
            drainFifo();
            writer.reset();
        }
    }

    /** Check if currently recording */
    bool getIsRecording() const { return isRecording.load(); }

    /** Get the current recording file */
    const juce::File& getRecordingFile() const { return recordingFile; }

    /** Check if FIFO overran (audio came faster than disk could write) */
    bool didOverrun() const { return fifoOverrun; }

    //==========================================================================
    /** Called from audio thread's processBlock.
     *  Copies interleaved L/R samples into the FIFO.
     *  Lock-free — safe for real-time use.
     */
    void pushSamples(const float* const* channelData, int numSamples)
    {
        if (!isRecording.load(std::memory_order_relaxed)) return;
        if (numSamples <= 0) return;

        // Interleave into temp buffer then push to FIFO
        int totalFloats = numSamples * numChannels;
        if (totalFloats > tempBufferSize) totalFloats = tempBufferSize;
        int clampedSamples = totalFloats / numChannels;

        // Interleave
        if (numChannels == 2)
        {
            for (int i = 0; i < clampedSamples; ++i)
            {
                tempBuffer[i * 2]     = channelData[0][i];
                tempBuffer[i * 2 + 1] = channelData[1][i];
            }
        }
        else
        {
            for (int i = 0; i < clampedSamples; ++i)
                tempBuffer[i] = channelData[0][i];
        }

        // Push to FIFO (lock-free)
        int start1, size1, start2, size2;
        fifo.prepareToWrite(totalFloats, start1, size1, start2, size2);

        if (size1 + size2 < totalFloats)
        {
            fifoOverrun = true;
            // Write what we can
        }

        if (size1 > 0)
            std::memcpy(fifoBuffer.data() + start1, tempBuffer.data(),
                        static_cast<size_t>(size1) * sizeof(float));
        if (size2 > 0)
            std::memcpy(fifoBuffer.data() + start2, tempBuffer.data() + size1,
                        static_cast<size_t>(size2) * sizeof(float));

        fifo.finishedWrite(size1 + size2);
    }

private:
    //==========================================================================
    // Thread: drain FIFO and write to disk
    //==========================================================================
    void run() override
    {
        while (!threadShouldExit() && isRecording.load())
        {
            drainFifo();
            Thread::sleep(5);  // ~200Hz polling, plenty fast for disk I/O
        }

        // Final drain after stop
        drainFifo();
    }

    /** Drain available samples from FIFO and write to WAV */
    void drainFifo()
    {
        if (writer == nullptr) return;

        int start1, size1, start2, size2;
        fifo.prepareToRead(fifo.getNumReady(), start1, size1, start2, size2);

        if (size1 > 0)
            writeInterleavedToWav(fifoBuffer.data() + start1, size1);
        if (size2 > 0)
            writeInterleavedToWav(fifoBuffer.data() + start2, size2);

        fifo.finishedRead(size1 + size2);
    }

    /** Write interleaved float samples to WAV via AudioFormatWriter */
    void writeInterleavedToWav(const float* interleaved, int numFloats)
    {
        int numSamples = numFloats / numChannels;
        if (numSamples <= 0) return;

        // De-interleave into write buffer
        writeBuffer.setSize(numChannels, numSamples, false, false, true);

        if (numChannels == 2)
        {
            auto* outL = writeBuffer.getWritePointer(0);
            auto* outR = writeBuffer.getWritePointer(1);
            for (int i = 0; i < numSamples; ++i)
            {
                outL[i] = interleaved[i * 2];
                outR[i] = interleaved[i * 2 + 1];
            }
        }
        else
        {
            auto* out = writeBuffer.getWritePointer(0);
            std::memcpy(out, interleaved, static_cast<size_t>(numSamples) * sizeof(float));
        }

        writer->writeFromAudioSampleBuffer(writeBuffer, 0, numSamples);
    }

    //==========================================================================
    std::atomic<bool> isRecording { false };
    int numChannels = 2;
    double currentSampleRate = 48000.0;
    juce::File recordingFile;
    bool fifoOverrun = false;

    // FIFO: ~2 seconds at 48kHz stereo
    static constexpr int fifoSize = 262144;
    juce::AbstractFifo fifo;
    std::array<float, fifoSize> fifoBuffer = {};

    // Temp interleave buffer (audio thread side, max 4096 stereo samples)
    static constexpr int tempBufferSize = 8192;
    std::array<float, tempBufferSize> tempBuffer = {};

    // Write buffer (writer thread side)
    juce::AudioBuffer<float> writeBuffer;

    // WAV writer
    std::unique_ptr<juce::AudioFormatWriter> writer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioRecorder)
};
