/*
  ==============================================================================
    iOSAudioEngine.h
    GOODMETER iOS - Audio playback engine

    Wraps AudioDeviceManager + AudioTransportSource + AudioProcessorPlayer
    to enable real-time file playback through GOODMETERAudioProcessor.

    Pipeline:
      AudioFormatReader -> AudioFormatReaderSource -> AudioTransportSource
        -> AudioProcessorPlayer(GOODMETERAudioProcessor)
          -> AudioDeviceManager -> speaker
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "FOABinauralDecoder.h"
#include "AdmParser.h"

// Defined in AmbisonicRecorder.mm (ObjC++): true when the current output
// route is personal listening (wired/BT headphones), false for speakers.
bool goodmeter_outputIsHeadphones();
// Headphone head-tracking (AirPods): yaw in radians, 0 when unavailable.
void goodmeter_headTracker_start();
void goodmeter_headTracker_stop();
float goodmeter_headTracker_yaw();
// Apple system spatializer bridge (multichannel PCM via AVAudioEngine).
bool goodmeter_appleSpatial_play(const char* path);
void goodmeter_appleSpatial_stop();
bool goodmeter_appleSpatial_isPlaying();

class iOSAudioEngine : private juce::ChangeListener,
                        private juce::AudioIODeviceCallback
{
    enum class FadeCompletionAction
    {
        none,
        pause,
        stopAtStart
    };

public:
    iOSAudioEngine(GOODMETERAudioProcessor& proc)
        : processor(proc)
    {
        formatManager.registerBasicFormats();
        transportSource.addChangeListener(this);
        foaScratch.setSize(4, FOABinauralDecoder::maxBlockSize); // preallocated: audio thread never allocates
        mcScratch.setSize(16, FOABinauralDecoder::maxBlockSize); // multichannel (5.1..9.1.4) pull buffer
    }

    ~iOSAudioEngine()
    {
        // Stop outside the lock so the render callback can acknowledge it.
        transportSource.stop();

        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
            transportSource.setSource(nullptr);
            readerSource.reset();
        }

        transportSource.removeChangeListener(this);

        if (deviceInitialised)
            deviceManager.removeAudioCallback(this);
    }

    //==========================================================================
    // File loading
    //==========================================================================
    bool loadFile(const juce::File& file)
    {
        if (!file.existsAsFile())
            return false;

        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        // Store duration directly from reader metadata — don't rely on
        // transportSource.getLengthInSeconds() which needs prepareToPlay's
        // sampleRate to have been set (may be 0 on iOS simulator).
        fileSampleRate = reader->sampleRate;
        fileLengthSamples = reader->lengthInSamples;
        fileDurationSeconds = (reader->sampleRate > 0)
            ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
            : 0.0;

        currentFileName = juce::URL::removeEscapeChars(file.getFileName());
        currentFilePath = file.getFullPathName();

        auto newReaderSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

        // Exactly 4ch = first-order-ambisonics take from the spatial recorder.
        // (>= 4 would misroute 5.1/7.1 surround imports through the FOA
        // decoder, reading L/R/C/LFE as W/Y/Z/X.)
        const bool isFOAFile = ((int) reader->numChannels == 4);
        // Anything else above stereo (5.1, 7.1, 7.1.4, 9.1.4, ADM beds...)
        // gets a proper standards-based stereo downmix instead of silently
        // playing only the first two channels.
        const int srcChannels = juce::jmin(16, (int) reader->numChannels);
        const bool isMultichannel = !isFOAFile && srcChannels > 2;

        // Stop OUTSIDE the callback lock: stop() needs the render callback
        // alive to acknowledge, and it may legitimately be playing here
        // (e.g. loading a new video while the previous one's audio runs).
        transportSource.stop();

        {
            const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

            fileLoaded = false;
            transportSource.setPosition(0.0);
            transportSource.setSource(nullptr);
            readerSource.reset();
            resetOutputEnvelope();

            readerSource = std::move(newReaderSource);
            transportSource.setSource(readerSource.get(), 0, nullptr,
                                      reader->sampleRate,
                                      isFOAFile ? 4 : (isMultichannel ? srcChannels : 2));

            foaPlaybackActive = isFOAFile;
            mcPlaybackActive = isMultichannel;
            mcChannels = srcChannels;
            fumaPlaybackActive = isFOAFile
                && file.getFileExtension().equalsIgnoreCase(".amb");
            if (isFOAFile)
                goodmeter_headTracker_start();   // no-op if unsupported
            else
                goodmeter_headTracker_stop();
            foaDecoder.prepare(currentDeviceSampleRate);
            foaDecoder.reset();

            // If the audio device is already running, re-prepare the transport
            // so it picks up the new source's length with a valid sampleRate.
            auto* device = deviceManager.getCurrentAudioDevice();
            if (device != nullptr)
            {
                double sr = device->getCurrentSampleRate();
                int bs = device->getCurrentBufferSizeSamples();
                if (sr > 0)
                    transportSource.prepareToPlay(bs, sr);
            }
            else
            {
                // No audio device — prepare transport with the file's own sample rate
                // so at least getLengthInSeconds() works for UI display
                transportSource.prepareToPlay(512, reader->sampleRate);
            }

            fileLoaded = true;
        }

        return true;
    }

    void clearFile()
    {
        // Stop outside the lock — see loadFile() for why.
        transportSource.stop();

        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        fileLoaded = false;
        currentFileName.clear();
        currentFilePath.clear();
        fileDurationSeconds = 0.0;
        fileSampleRate = 0.0;
        fileLengthSamples = 0;

        transportSource.setPosition(0.0);
        transportSource.setSource(nullptr);
        readerSource.reset();
        resetOutputEnvelope();
        foaPlaybackActive = false;
        fumaPlaybackActive = false;
        mcPlaybackActive = false;
        goodmeter_headTracker_stop();
    }

    //==========================================================================
    // Transport controls
    //==========================================================================
    void play()
    {
        ensureDeviceInitialised();

        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (fileLoaded)
        {
            ++transportCommandSerial;
            transportStopPending.store(false, std::memory_order_release);
            const double totalLength = getTotalLength();
            if (totalLength > 0.1 && transportSource.getCurrentPosition() >= totalLength - 0.01)
                transportSource.setPosition(0.0);

            forceOutputMute.store(false, std::memory_order_release);
            beginOutputFade(1.0f, FadeCompletionAction::none, -1.0);
            transportSource.start();
        }
    }

    void pause()
    {
        pauseAndSeek(-1.0);
    }

    void pauseAndSeek(double positionSeconds)
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (!fileLoaded)
            return;

        if (transportStopPending.load(std::memory_order_acquire))
            return;

        if (!transportSource.isPlaying())
        {
            transportSource.stop();
            if (positionSeconds >= 0.0)
                transportSource.setPosition(positionSeconds);
            transportStopPending.store(false, std::memory_order_release);
            silenceOutputEnvelope();
            return;
        }

        const auto serial = ++transportCommandSerial;
        beginDeferredTransportStop(serial, FadeCompletionAction::pause, positionSeconds);
    }

    void stop()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

        if (transportStopPending.load(std::memory_order_acquire))
            return;

        if (!fileLoaded || !transportSource.isPlaying())
        {
            transportSource.stop();
            transportSource.setPosition(0.0);
            transportStopPending.store(false, std::memory_order_release);
            silenceOutputEnvelope();
            return;
        }

        const auto serial = ++transportCommandSerial;
        beginDeferredTransportStop(serial, FadeCompletionAction::stopAtStart, 0.0);
    }

    void seek(double positionSeconds)
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
        ++transportCommandSerial;
        transportStopPending.store(false, std::memory_order_release);
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        outputEnvelopeSamplesRemaining = 0;
        transportSource.setPosition(positionSeconds);
        foaDecoder.reset(); // position jump: clear binaural delay/filter tails
    }

    void setVolume(float newVolume)
    {
        playbackGain.store(juce::jlimit(0.0f, 1.0f, newVolume), std::memory_order_relaxed);
    }

    // While the built-in-mic recorder owns the processor's metering (it feeds
    // processBlock from the capture queue), suspend our own processBlock calls
    // so the two threads never touch the meter DSP concurrently.
    void setMeteringSuspended(bool shouldSuspend)
    {
        meteringSuspended.store(shouldSuspend, std::memory_order_release);
    }

    // Reset the meter DSP for a recording take UNDER the audio callback lock.
    // Calling prepareToPlay from the message thread while the render callback
    // is mid-processBlock is a data race on the filter states (intermittent
    // heap corruption -> the "swipe to ambeo sometimes crashes" family).
    void prepareProcessorForRecording()
    {
        const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());
        processor.setPlayConfigDetails(0, 2, 48000.0, 512);
        processor.prepareToPlay(48000.0, 512);
    }

    // The recorder's meter tap must hold this lock while it feeds
    // processor.processBlock: AVCapture session activation can restart the
    // playback device mid-take, and the device-restart path re-runs
    // prepareToPlay (reallocating the buffers processBlock iterates) under
    // this same lock. Crash-log verified: all recording SIGSEGVs were
    // processBlock on the capture queue racing exactly that.
    juce::CriticalSection& getAudioCallbackLock()
    {
        return deviceManager.getAudioCallbackLock();
    }

    float getVolume() const
    {
        return playbackGain.load(std::memory_order_relaxed);
    }

    //==========================================================================
    // State queries
    //==========================================================================
    // Report "not playing" as soon as a stop/pause has been requested, even
    // though the transport keeps running silently during the declick fade.
    // This keeps the 30Hz page-sync logic from issuing extra transport
    // commands during the fade window.
    bool isPlaying() const
    {
        return transportSource.isPlaying()
            && !transportStopPending.load(std::memory_order_acquire);
    }
    bool isFileLoaded() const { return fileLoaded; }

    double getCurrentPosition() const { return transportSource.getCurrentPosition(); }

    double getTotalLength() const
    {
        double transportLen = transportSource.getLengthInSeconds();
        // transportLen can be +infinity if transport's sampleRate is 0
        if (std::isfinite(transportLen) && transportLen > 0.01)
            return transportLen;
        return fileDurationSeconds;
    }

    juce::String getCurrentFileName() const { return currentFileName; }
    juce::String getCurrentFilePath() const { return currentFilePath; }

    /** A/B audition: hand the loaded multichannel file to Apple's own
        spatializer (AVAudioEngine + system head tracking). Pauses our
        transport while active; toggling off returns to the in-app renderer.
        Returns the resulting on/off state. */
    bool setAppleSpatialPlayback(bool shouldBeOn)
    {
        if (! shouldBeOn)
        {
            goodmeter_appleSpatial_stop();
            return false;
        }
        const juce::File src(currentFilePath);
        if (! src.existsAsFile())
            return false;
        stop();   // our transport yields the stage
        return goodmeter_appleSpatial_play(currentFilePath.toRawUTF8());
    }

    bool isAppleSpatialPlaying() const { return goodmeter_appleSpatial_isPlaying(); }

    //==========================================================================
    // Virtual mic (FOA playback monitor + offline export)
    //==========================================================================
    void setVirtualMic(bool enabled, float azimuthRad, float elevationRad, float pattern)
    {
        vmicAz.store(azimuthRad, std::memory_order_relaxed);
        vmicEl.store(elevationRad, std::memory_order_relaxed);
        vmicPattern.store(juce::jlimit(0.0f, 1.0f, pattern), std::memory_order_relaxed);
        vmicEnabled.store(enabled, std::memory_order_relaxed);
    }

    /** Offline-renders the loaded FOA file through the SAME binaural decoder
        you monitor with, into a shareable stereo 24-bit "_BIN.wav". */
    void exportBinauralWav(std::function<void(juce::File)> onDone)
    {
        const juce::File src(currentFilePath);
        if (! src.existsAsFile())
            return;
        const bool fuma = fumaPlaybackActive;

        juce::Thread::launch([src, fuma, onDone]()
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(src));
            if (reader == nullptr)
                return;

            // Multichannel / Atmos ADM master: object+bed renderer instead
            // of the FOA decoder (which is strictly the 4ch ambisonic path).
            if ((int) reader->numChannels != 4)
            {
                if ((int) reader->numChannels >= 3)
                {
                    reader.reset();
                    auto dest = src.getSiblingFile(src.getFileNameWithoutExtension() + "_ADM-BIN.wav");
                    if (AdmParser::renderToBinaural(src, dest) && onDone)
                        juce::MessageManager::callAsync([onDone, dest]() { onDone(dest); });
                }
                return;
            }

            auto decoder = std::make_unique<FOABinauralDecoder>();
            decoder->prepare(reader->sampleRate);

            auto dest = src.getSiblingFile(src.getFileNameWithoutExtension() + "_BIN.wav");
            dest.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os(dest.createOutputStream());
            if (os == nullptr)
                return;
            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(os.get(), reader->sampleRate, 2, 24, {}, 0));
            if (writer == nullptr)
                return;
            os.release();

            const int bs = juce::jmin(8192, (int) FOABinauralDecoder::maxBlockSize);
            juce::AudioBuffer<float> in(4, bs), out(2, bs);
            juce::int64 pos = 0;
            const auto total = (juce::int64) reader->lengthInSamples;
            while (pos < total)
            {
                const int n = (int) juce::jmin<juce::int64>(bs, total - pos);
                reader->read(&in, 0, n, pos, true, true);
                if (fuma)
                {
                    auto* cw = in.getWritePointer(0); auto* c1 = in.getWritePointer(1);
                    auto* c2 = in.getWritePointer(2); auto* c3 = in.getWritePointer(3);
                    for (int i = 0; i < n; ++i)
                    {
                        const float fx = c1[i], fy = c2[i], fz = c3[i];
                        cw[i] *= 1.41421356f;
                        c1[i] = fy; c2[i] = fz; c3[i] = fx;
                    }
                }
                out.clear();
                decoder->process(in, out, n);
                writer->writeFromAudioSampleBuffer(out, 0, n);
                pos += n;
            }
            writer->flush();

            if (onDone)
                juce::MessageManager::callAsync([onDone, dest]() { onDone(dest); });
        });
    }

    /** Offline-renders the currently loaded FOA file through the virtual mic
        into a mono 24-bit WAV next to it; onDone fires on the message thread. */
    void exportVirtualMicWav(float az, float el, float p,
                             std::function<void(juce::File)> onDone)
    {
        const juce::File src(currentFilePath);
        if (! src.existsAsFile())
            return;

        juce::Thread::launch([src, az, el, p, onDone]()
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(src));
            if (reader == nullptr || reader->numChannels < 4)
                return;

            const float dx = std::cos(el) * std::cos(az);
            const float dy = std::cos(el) * std::sin(az);
            const float dz = std::sin(el);
            const float k  = (1.0f - p) * 1.7320508f;

            auto dest = src.getSiblingFile(src.getFileNameWithoutExtension()
                          + "_VMIC" + juce::String(juce::roundToInt(juce::radiansToDegrees(az)))
                          + ".wav");
            dest.deleteFile();
            std::unique_ptr<juce::FileOutputStream> os(dest.createOutputStream());
            if (os == nullptr)
                return;

            juce::WavAudioFormat wavFormat;
            std::unique_ptr<juce::AudioFormatWriter> writer(
                wavFormat.createWriterFor(os.get(), reader->sampleRate, 1, 24, {}, 0));
            if (writer == nullptr)
                return;
            os.release();   // writer owns the stream now

            juce::AudioBuffer<float> in(4, 8192), out(1, 8192);
            juce::int64 pos = 0;
            const auto total = (juce::int64) reader->lengthInSamples;
            while (pos < total)
            {
                const int n = (int) juce::jmin<juce::int64>(8192, total - pos);
                reader->read(&in, 0, n, pos, true, true);
                const float* w = in.getReadPointer(0);
                const float* y = in.getReadPointer(1);
                const float* z = in.getReadPointer(2);
                const float* x = in.getReadPointer(3);
                float* o = out.getWritePointer(0);
                for (int i = 0; i < n; ++i)
                    o[i] = p * w[i] + k * (dx * x[i] + dy * y[i] + dz * z[i]);
                writer->writeFromAudioSampleBuffer(out, 0, n);
                pos += n;
            }
            writer->flush();

            if (onDone)
                juce::MessageManager::callAsync([onDone, dest]() { onDone(dest); });
        });
    }

    //==========================================================================
    // Audio source for processorPlayer
    // The processorPlayer needs audio input — we feed it from transportSource
    //==========================================================================
    juce::AudioTransportSource& getTransportSource() { return transportSource; }

private:
    void ensureDeviceInitialised()
    {
        if (deviceInitialised)
            return;

        // Delay RemoteIO startup until the app actually needs playback.
        // Starting the device during app launch can deadlock on iOS simulator.
        auto err = deviceManager.initialise(0, 2, nullptr, true);
        juce::ignoreUnused(err);

        deviceManager.addAudioCallback(this);
        deviceInitialised = true;
    }

    //==========================================================================
    // AudioIODeviceCallback: read from transport -> process -> output
    //==========================================================================
    void audioDeviceIOCallbackWithContext(const float* const* /*inputData*/,
                                          int /*numInputChannels*/,
                                          float* const* outputData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext&) override
    {
        // Create a buffer pointing to the output channels
        juce::AudioBuffer<float> buffer(outputData, numOutputChannels, numSamples);
        buffer.clear();

        // ALWAYS pull the transport, even while force-muted.
        //
        // Why: AudioTransportSource::stop() spin-waits (up to ~1 second) for
        // getNextAudioBlock() to acknowledge the stop by setting its internal
        // 'stopped' flag. The old early-return-on-mute meant that ack never
        // happened, so every pause parked the message thread inside stop()
        // for the full timeout — and because stop() used to be called while
        // holding the device's audio callback lock, the CoreAudio render
        // thread was starved for that entire second. Dozens of consecutive
        // missed render deadlines are exactly the raspy "electric crackle"
        // burst heard on pause. Pulling unconditionally keeps the ack path
        // alive; a stopped transport just clears the region (position does
        // not advance), so this costs nothing.
        if (foaPlaybackActive && numOutputChannels >= 2
            && numSamples <= FOABinauralDecoder::maxBlockSize
            && foaDecoder.isPrepared())
        {
            // FOA take: pull all 4 ambisonic channels, then render binaural
            // stereo into the device buffer (meters then see what you hear).
            juce::AudioSourceChannelInfo foaInfo(&foaScratch, 0, numSamples);
            foaScratch.clear(0, 0, numSamples);
            foaScratch.clear(1, 0, numSamples);
            foaScratch.clear(2, 0, numSamples);
            foaScratch.clear(3, 0, numSamples);
            transportSource.getNextAudioBlock(foaInfo);

            // Feed meters/DOA/tracks only while actually rolling — a parked
            // transport still gets pulled (stop-ack) but delivers zeros, and
            // pushing those kept the DOA/TRACKS cards "alive" on silence.
            const bool foaRolling = transportSource.isPlaying();

            // FuMa (.amb) -> ambiX: reorder W,X,Y,Z -> W,Y,Z,X and undo the
            // -3 dB FuMa W convention, so downstream code sees ambiX only.
            if (fumaPlaybackActive)
            {
                auto* cw = foaScratch.getWritePointer(0);
                auto* c1 = foaScratch.getWritePointer(1);
                auto* c2 = foaScratch.getWritePointer(2);
                auto* c3 = foaScratch.getWritePointer(3);
                for (int i = 0; i < numSamples; ++i)
                {
                    const float fx = c1[i], fy = c2[i], fz = c3[i];
                    cw[i] *= 1.41421356f;
                    c1[i] = fy; c2[i] = fz; c3[i] = fx;
                }
            }

            // Head tracking: counter-rotate the sound field about Z by the
            // headphone yaw so the scene stays world-fixed as you turn.
            const float yaw = goodmeter_headTracker_yaw();
            if (std::abs(yaw) > 0.005f)
            {
                const float c = std::cos(yaw), s = std::sin(yaw);
                auto* cy = foaScratch.getWritePointer(1);   // Y
                auto* cx = foaScratch.getWritePointer(3);   // X
                for (int i = 0; i < numSamples; ++i)
                {
                    const float xv = cx[i], yv = cy[i];
                    cx[i] = c * xv + s * yv;
                    cy[i] = -s * xv + c * yv;
                }
            }

            // DOA MAP feed (ACN order: W, Y, Z, X)
            if (foaRolling)
                processor.foaDoa.process(foaScratch.getReadPointer(0), foaScratch.getReadPointer(1),
                                         foaScratch.getReadPointer(2), foaScratch.getReadPointer(3),
                                         numSamples);

            // TRACKS card: 4 lanes = the raw ambisonic channels
            if (foaRolling)
            {
                const float* chs[4] = { foaScratch.getReadPointer(0), foaScratch.getReadPointer(1),
                                        foaScratch.getReadPointer(2), foaScratch.getReadPointer(3) };
                processor.trackScope.push(chs, 4, numSamples);
            }

            if (vmicEnabled.load(std::memory_order_relaxed))            {
                // Virtual first-order mic monitor: s = p*W + (1-p)*sqrt3*(d.XYZ)
                // p: 1=omni, 0.5=cardioid, ~0.34=hypercardioid, 0=figure-8.
                const float az = vmicAz.load(std::memory_order_relaxed);
                const float el = vmicEl.load(std::memory_order_relaxed);
                const float p  = vmicPattern.load(std::memory_order_relaxed);
                const float dx = std::cos(el) * std::cos(az);
                const float dy = std::cos(el) * std::sin(az);
                const float dz = std::sin(el);
                const float k  = (1.0f - p) * 1.7320508f;   // SN3D dipole gain
                const float* w = foaScratch.getReadPointer(0);
                const float* y = foaScratch.getReadPointer(1);
                const float* z = foaScratch.getReadPointer(2);
                const float* x = foaScratch.getReadPointer(3);
                auto* outL = buffer.getWritePointer(0);
                auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float s = p * w[i] + k * (dx * x[i] + dy * y[i] + dz * z[i]);
                    outL[i] = s;
                    if (outR != nullptr)
                        outR[i] = s;
                }
            }
            else if (! headphonesRoute.load(std::memory_order_relaxed))
            {
                // Loudspeaker route: binaural cues die in crosstalk — decode
                // a clean mid/side stereo pair instead (cardioids at +/-90).
                const float* w = foaScratch.getReadPointer(0);
                const float* y = foaScratch.getReadPointer(1);
                auto* outL = buffer.getWritePointer(0);
                auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
                for (int i = 0; i < numSamples; ++i)
                {
                    const float m = 0.5f * w[i];
                    const float s = 0.5f * 1.7320508f * y[i];
                    outL[i] = m + s;
                    if (outR != nullptr)
                        outR[i] = m - s;
                }
            }
            else
            {
                foaDecoder.process(foaScratch, buffer, numSamples);
            }
        }
        else if (mcPlaybackActive && mcChannels > 2
                 && numSamples <= FOABinauralDecoder::maxBlockSize)
        {
            // Multichannel (5.1 / 7.1 / 7.1.4 / 9.1.4 / ADM beds...): pull all
            // channels, standards-based stereo downmix. SMPTE/Atmos order
            // convention: L R C LFE then L/R pairs (sides, rears, wides, tops).
            juce::AudioSourceChannelInfo mcInfo(&mcScratch, 0, numSamples);
            for (int c = 0; c < mcScratch.getNumChannels(); ++c)
                mcScratch.clear(c, 0, numSamples);
            transportSource.getNextAudioBlock(mcInfo);

            auto* outL = buffer.getWritePointer(0);
            auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
            const int n = mcChannels;
            const int pairStart = n >= 6 ? 4 : 3;   // LFE only in 6ch+ layouts

            for (int i = 0; i < numSamples; ++i)
            {
                float l = mcScratch.getReadPointer(0)[i];
                float r = n > 1 ? mcScratch.getReadPointer(1)[i] : l;
                if (n > 2)  { const float c0 = mcScratch.getReadPointer(2)[i] * 0.7071f; l += c0; r += c0; }        // C
                if (n >= 6) { const float lf = mcScratch.getReadPointer(3)[i] * 0.5f;    l += lf; r += lf; }        // LFE
                for (int c = pairStart; c < n; ++c)
                {
                    const float v = mcScratch.getReadPointer(c)[i] * 0.7071f;   // surrounds/wides/tops
                    if (((c - pairStart) & 1) == 0) l += v; else r += v;
                }
                outL[i] = l * 0.71f;                 // headroom against downmix build-up
                if (outR != nullptr)
                    outR[i] = r * 0.71f;
            }

            // TRACKS card: first 4 source lanes (scope is 4-lane capped)
            if (transportSource.isPlaying())
            {
                const float* chs[4] = { mcScratch.getReadPointer(0), mcScratch.getReadPointer(1),
                                        mcScratch.getReadPointer(2), mcScratch.getReadPointer(3) };
                processor.trackScope.push(chs, juce::jmin(4, n), numSamples);
            }
        }
        else
        {
            juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);
            transportSource.getNextAudioBlock(info);

            // TRACKS card: stereo/mono lanes (only while actually playing)
            if (transportSource.isPlaying() && buffer.getNumChannels() > 0)
            {
                const float* chs[2] = { buffer.getReadPointer(0),
                                        buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : nullptr };
                processor.trackScope.push(chs, chs[1] != nullptr ? 2 : 1, numSamples);
            }
        }

        // Meter DSP gate: while the transport is engaged (or briefly after it
        // stops, so meters decay to silence naturally) run the full metering
        // chain. Once parked, skip processBlock entirely — running K-weighting
        // + FFT pushes on pure zeros 24/7 was wasted CPU that fed iPhone
        // thermal throttling ("app gets slower the longer it runs").
        const bool transportEngaged = transportSource.isPlaying()
                                   || transportStopPending.load(std::memory_order_acquire);
        if (transportEngaged)
        {
            const double sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
            const int bs = juce::jmax(1, currentDeviceBufferSizeSamples);
            idleMeterDecayBlocksRemaining = juce::jmax(1, (int) std::ceil(1.5 * sr / bs));
        }

        if (transportEngaged || idleMeterDecayBlocksRemaining > 0)
        {
            if (!transportEngaged)
                --idleMeterDecayBlocksRemaining;

            // Skip while the recorder owns the meter DSP (see setMeteringSuspended).
            if (!meteringSuspended.load(std::memory_order_acquire))
            {
                // Feed through the processor so it can compute meters
                juce::MidiBuffer midi;
                processor.processBlock(buffer, midi);
            }
        }

        maybeBeginEndFade();
        applyOutputEnvelope(buffer, numSamples);

        // Force-mute is now a final output gate instead of an early return,
        // so it can never break the transport's stop handshake.
        if (forceOutputMute.load(std::memory_order_acquire))
            buffer.clear();

        // Output buffer already points to outputData, so we're done
    }

    void audioDeviceAboutToStart(juce::AudioIODevice* device) override
    {
        double sr = device->getCurrentSampleRate();
        int bs = device->getCurrentBufferSizeSamples();

        currentDeviceSampleRate = sr > 0.0 ? sr : 48000.0;
        currentDeviceBufferSizeSamples = bs > 0 ? bs : 512;
        transportSource.prepareToPlay(bs, sr);
        foaDecoder.prepare(currentDeviceSampleRate);
        // Route changes restart the device, so this self-refreshes on
        // plugging/unplugging headphones (binaural vs speaker mid/side).
        headphonesRoute.store(goodmeter_outputIsHeadphones(), std::memory_order_relaxed);

        processor.setPlayConfigDetails(0, 2, sr, bs);
        processor.prepareToPlay(sr, bs);
    }

    void audioDeviceStopped() override
    {
        transportSource.releaseResources();
        processor.releaseResources();
    }

    void changeListenerCallback(juce::ChangeBroadcaster*) override
    {
        // End-of-file rewinds are handled by page-level transport code after a
        // declick fade. Seeking here bypasses that gate and can click on iOS.
        //
        // BUT: JUCE's AudioTransportSource does NOT clear its playing flag when
        // the source hits EOF — it stays "playing but finished". For the pure
        // audio path (no video page monitoring the end) that means our audio
        // callback keeps running the full per-sample meter DSP on silence
        // FOREVER after the file finishes. That runaway churn is a real heat /
        // battery sink ("the phone warms up a while after playback ends").
        //
        // So when the stream has genuinely finished while still marked playing,
        // park the transport. This runs on the message thread; stop() is called
        // outside the audio callback lock (the callback keeps pulling the
        // transport, so stop() acknowledges immediately). Position is left as
        // is — play() rewinds from the end on the next start, and the video
        // page owns its own transport position.
        if (fileLoaded
            && transportSource.hasStreamFinished()
            && transportSource.isPlaying()
            && !transportStopPending.load(std::memory_order_acquire))
        {
            ++transportCommandSerial;
            transportSource.stop();
            silenceOutputEnvelope();
        }
    }

    void resetOutputEnvelope()
    {
        outputEnvelopeGain = 1.0f;
        outputEnvelopeTargetGain = 1.0f;
        outputEnvelopeSamplesRemaining = 0;
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        forceOutputMute.store(false, std::memory_order_release);
    }

    int getOutputFadeSamples() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        return juce::jlimit(512, 8192, (int) std::round(sr * 0.090));
    }

    void beginOutputFade(float targetGain, FadeCompletionAction completionAction, double seekAfterFade)
    {
        outputEnvelopeTargetGain = juce::jlimit(0.0f, 1.0f, targetGain);
        outputEnvelopeSamplesRemaining = getOutputFadeSamples();
        fadeCompletionAction = completionAction;
        fadeCompletionSeekPosition = seekAfterFade;
    }

    void silenceOutputEnvelope()
    {
        outputEnvelopeGain = 0.0f;
        outputEnvelopeTargetGain = 0.0f;
        outputEnvelopeSamplesRemaining = 0;
        fadeCompletionAction = FadeCompletionAction::none;
        fadeCompletionSeekPosition = -1.0;
        forceOutputMute.store(true, std::memory_order_release);
    }

    int getOutputFadeMilliseconds() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        const auto bufferMs = 1000.0 * (double) juce::jmax(1, currentDeviceBufferSizeSamples) / sr;
        return juce::jlimit(20, 180,
                            (int) std::ceil(1000.0 * (double) getOutputFadeSamples() / sr
                                             + bufferMs + 8.0));
    }

    int getPostMuteDrainMilliseconds() const
    {
        const auto sr = currentDeviceSampleRate > 0.0 ? currentDeviceSampleRate : 48000.0;
        const auto bufferMs = 1000.0 * (double) juce::jmax(1, currentDeviceBufferSizeSamples) / sr;
        return juce::jlimit(32, 120, (int) std::ceil(bufferMs * 2.0 + 24.0));
    }

    void beginDeferredTransportStop(uint32_t serial, FadeCompletionAction action, double seekAfterFade)
    {
        transportStopPending.store(true, std::memory_order_release);
        forceOutputMute.store(false, std::memory_order_release);
        beginOutputFade(0.0f, FadeCompletionAction::none, -1.0);
        scheduleDeferredTransportStop(serial, action, seekAfterFade);
    }

    void scheduleDeferredTransportStop(uint32_t serial, FadeCompletionAction action, double seekAfterFade)
    {
        // Wait long enough for the fade to have fully reached zero AND for at
        // least one extra hardware buffer of guaranteed-silent output.
        juce::Timer::callAfterDelay(getOutputFadeMilliseconds() + getPostMuteDrainMilliseconds(),
            [this, serial, action, seekAfterFade]()
            {
                // Serial check is message-thread-safe without the lock:
                // play()/seek()/pause() all mutate the serial on this same
                // thread, and the audio callback never touches it.
                if (transportCommandSerial.load(std::memory_order_relaxed) != serial)
                    return;

                // CRITICAL: stop() must be called WITHOUT holding the audio
                // callback lock. It spin-waits for getNextAudioBlock() to
                // acknowledge the stop; holding the lock here blocks the
                // render callback, so the ack never arrives, stop() burns
                // its full ~1s timeout, and CoreAudio is starved the whole
                // time — that starvation burst was the pause crackle.
                // The envelope has been at zero for a full drain period, so
                // any transport-side discontinuity is multiplied by 0.
                transportSource.stop();

                const juce::ScopedLock callbackLock(deviceManager.getAudioCallbackLock());

                if (transportCommandSerial.load(std::memory_order_relaxed) != serial)
                    return;

                if (action == FadeCompletionAction::stopAtStart)
                    transportSource.setPosition(0.0);
                else if (seekAfterFade >= 0.0)
                    transportSource.setPosition(seekAfterFade);

                transportStopPending.store(false, std::memory_order_release);
                silenceOutputEnvelope();
            });
    }

    void maybeBeginEndFade()
    {
        if (!fileLoaded || !transportSource.isPlaying()
            || fadeCompletionAction != FadeCompletionAction::none
            || outputEnvelopeTargetGain <= 0.0f)
        {
            return;
        }

        const double totalLength = getTotalLength();
        if (totalLength <= 0.1)
            return;

        const double remainingSeconds = totalLength - transportSource.getCurrentPosition();
        if (remainingSeconds > 0.0 && remainingSeconds <= 0.18)
            beginOutputFade(0.0f, FadeCompletionAction::none, -1.0);
    }

    void applyOutputEnvelope(juce::AudioBuffer<float>& buffer, int numSamples)
    {
        const float userGain = playbackGain.load(std::memory_order_relaxed);

        if (outputEnvelopeSamplesRemaining <= 0)
        {
            outputEnvelopeGain = outputEnvelopeTargetGain;
            buffer.applyGain(userGain * outputEnvelopeGain);
            return;
        }

        int sample = 0;
        while (sample < numSamples && outputEnvelopeSamplesRemaining > 0)
        {
            const float step = (outputEnvelopeTargetGain - outputEnvelopeGain)
                / (float) outputEnvelopeSamplesRemaining;
            outputEnvelopeGain += step;
            --outputEnvelopeSamplesRemaining;

            const float gain = userGain * outputEnvelopeGain;
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);

            ++sample;
        }

        if (outputEnvelopeSamplesRemaining <= 0)
        {
            outputEnvelopeGain = outputEnvelopeTargetGain;

            if (outputEnvelopeTargetGain <= 0.0f)
            {
                if (sample < numSamples)
                    buffer.clear(sample, numSamples - sample);
                return;
            }
        }

        if (sample < numSamples)
            buffer.applyGain(sample, numSamples - sample, userGain * outputEnvelopeGain);
    }

    GOODMETERAudioProcessor& processor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    bool deviceInitialised = false;

    juce::String currentFileName;
    juce::String currentFilePath;
    bool fileLoaded = false;
    double fileDurationSeconds = 0.0;
    double fileSampleRate = 0.0;
    int64_t fileLengthSamples = 0;
    std::atomic<float> playbackGain { 0.8f };
    double currentDeviceSampleRate = 48000.0;
    int currentDeviceBufferSizeSamples = 512;
    float outputEnvelopeGain = 1.0f;
    float outputEnvelopeTargetGain = 1.0f;
    int outputEnvelopeSamplesRemaining = 0;
    FadeCompletionAction fadeCompletionAction = FadeCompletionAction::none;
    double fadeCompletionSeekPosition = -1.0;
    std::atomic<uint32_t> transportCommandSerial { 1 };
    std::atomic<bool> transportStopPending { false };
    std::atomic<bool> forceOutputMute { false };
    std::atomic<bool> meteringSuspended { false };
    int idleMeterDecayBlocksRemaining = 0; // audio-thread only

    // FOA (4ch ambisonic) playback -> binaural rendering
    FOABinauralDecoder foaDecoder;
    juce::AudioBuffer<float> foaScratch;   // preallocated 4ch pull buffer

    // Virtual mic monitor params (UI thread writes, audio thread reads)
    std::atomic<bool>  vmicEnabled { false };
    std::atomic<float> vmicAz { 0.0f }, vmicEl { 0.0f }, vmicPattern { 0.5f };
    std::atomic<bool>  headphonesRoute { true };   // false => speaker mid/side
    bool foaPlaybackActive = false;        // guarded by the audio callback lock
    bool fumaPlaybackActive = false;       // .amb import: FuMa->ambiX on the fly
    bool mcPlaybackActive = false;         // >2ch non-FOA: stereo downmix path
    int  mcChannels = 0;
    juce::AudioBuffer<float> mcScratch;    // preallocated 16ch pull buffer
};
