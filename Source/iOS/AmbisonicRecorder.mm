/*
  ==============================================================================
    AmbisonicRecorder.mm
    GOODMETER iOS - Built-in-mic spatial audio recorder (implementation)

    See AmbisonicRecorder.h for the architecture overview.
  ==============================================================================
*/

#include "AmbisonicRecorder.h"

#if JUCE_IOS

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <AudioToolbox/AudioToolbox.h>

//==============================================================================
@interface GOODMETERAmbiCaptureDelegate : NSObject <AVCaptureAudioDataOutputSampleBufferDelegate>
@property (nonatomic, assign) AmbisonicRecorder::Impl* owner;
@end

//==============================================================================
struct AmbisonicRecorder::Impl
{
    Impl()
    {
        captureQueue = dispatch_queue_create("goodmeter.ambirec.capture", DISPATCH_QUEUE_SERIAL);
        delegate = [[GOODMETERAmbiCaptureDelegate alloc] init];
        delegate.owner = this;

        for (auto& p : channelPeaks)
            p.store(0.0f, std::memory_order_relaxed);
    }

    ~Impl()
    {
        lifeToken->store(false, std::memory_order_release);

        if (session != nil && session.running)
            [session stopRunning];

        [audioOutput setSampleBufferDelegate:nil queue:nil];
        delegate.owner = nullptr;

        shutdownWriter();

        session = nil;
        audioInput = nil;
        audioOutput = nil;
        delegate = nil;
    }

    //==========================================================================
    bool computeFOASupport()
    {
        // The real-time FOA PCM path (AudioDataOutput + spatial layout tag)
        // only exists on iOS 26+. iOS 18's MovieFileOutput-only route can't
        // feed our own WAV writer, so we don't count it.
        if (@available(iOS 26.0, *))
        {
            AVCaptureDevice* mic = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
            if (mic == nil)
                return false;

            NSError* err = nil;
            AVCaptureDeviceInput* probe = [AVCaptureDeviceInput deviceInputWithDevice:mic error:&err];
            if (probe == nil || err != nil)
                return false;

            return [probe isMultichannelAudioModeSupported:AVCaptureMultichannelAudioModeFirstOrderAmbisonics];
        }

        return false;
    }

    bool isFOASupported()
    {
        if (!foaSupportComputed)
        {
            foaSupported = computeFOASupport();
            foaSupportComputed = true;
        }
        return foaSupported;
    }

    //==========================================================================
    // Runs ON THE CAPTURE QUEUE only. Tears down any existing graph and builds
    // a fresh one for the requested mode. Rationale: mutating
    // spatialAudioChannelLayoutTag on a session that has already run throws an
    // AVFoundation exception on some paths — the "swipe to ambeo -> instant
    // crash" bug. So on a mode switch we rebuild from scratch, and EVERY
    // AVFoundation configuration call is wrapped in @try/@catch: worst case a
    // take fails cleanly instead of killing the app.
    bool buildSessionOnQueue(bool wantFOA)
    {
        @try
        {
            if (session != nil && session.running)
                [session stopRunning];

            if (audioOutput != nil)
                [audioOutput setSampleBufferDelegate:nil queue:nil];

            session = nil; audioInput = nil; audioOutput = nil;
            sessionBuilt = false;

            AVCaptureSession* s = [[AVCaptureSession alloc] init];

            AVCaptureDevice* mic = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
            if (mic == nil)
                return false;

            NSError* nsError = nil;
            AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:mic error:&nsError];
            if (input == nil || nsError != nil || ![s canAddInput:input])
                return false;
            [s addInput:input];

            spatialAvailable = false;
            if (@available(iOS 26.0, *))
            {
                if ([input isMultichannelAudioModeSupported:AVCaptureMultichannelAudioModeFirstOrderAmbisonics])
                {
                    input.multichannelAudioMode = AVCaptureMultichannelAudioModeFirstOrderAmbisonics;
                    spatialAvailable = true;
                }
            }

            AVCaptureAudioDataOutput* out = [[AVCaptureAudioDataOutput alloc] init];
            if (@available(iOS 26.0, *))
            {
                if (spatialAvailable)
                    out.spatialAudioChannelLayoutTag = wantFOA
                        ? (kAudioChannelLayoutTag_HOA_ACN_SN3D | 4)
                        : kAudioChannelLayoutTag_Stereo;
            }

            if (![s canAddOutput:out])
                return false;
            [s addOutput:out];
            [out setSampleBufferDelegate:delegate queue:captureQueue];

            session = s;
            audioInput = input;
            audioOutput = out;
            configuredFOA = wantFOA;
            sessionBuilt = true;
            return true;
        }
        @catch (NSException* e)
        {
            juce::ignoreUnused(e);
            session = nil; audioInput = nil; audioOutput = nil;
            sessionBuilt = false;
            return false;
        }
    }

    bool start(const juce::File& file, AmbisonicRecorder::CaptureMode mode, juce::String& error)
    {
        if (recordingFlag.load(std::memory_order_acquire))
        {
            error = "Already recording";
            return false;
        }

        const auto auth = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
        if (auth == AVAuthorizationStatusDenied || auth == AVAuthorizationStatusRestricted)
        {
            error = "Microphone access is denied. Enable it in Settings.";
            return false;
        }

        outputFile = file;
        if (outputFile.existsAsFile())
            outputFile.deleteFile();

        samplesWritten.store(0, std::memory_order_relaxed);
        writerFailed.store(false, std::memory_order_relaxed);
        activeChannels.store(0, std::memory_order_relaxed);
        for (auto& p : channelPeaks)
            p.store(0.0f, std::memory_order_relaxed);

        const bool requestFOA = (mode == AmbisonicRecorder::CaptureMode::foa);
        foaActive = requestFOA && isFOASupported();
        recordingFlag.store(true, std::memory_order_release);

        // Everything session-related happens on the SAME serial capture queue
        // that stop() uses — strictly ordered after any previous take's
        // teardown, never racing it from the message thread.
        auto life = lifeToken;
        const bool wantFOA = foaActive;

        auto launchBlock = ^{
            if (!life->load(std::memory_order_acquire))
                return;

            // (Re)build the graph on first use or when the mode changed.
            if (!sessionBuilt || session == nil || configuredFOA != wantFOA)
            {
                if (!buildSessionOnQueue(wantFOA))
                {
                    recordingFlag.store(false, std::memory_order_release);
                    return;
                }
            }

            // Fresh writer thread per take, created/destroyed only on this
            // queue so it never races the message thread.
            writerThread = std::make_unique<juce::TimeSliceThread>("AmbiRec Writer");
            writerThread->startThread();

            @try
            {
                if (!session.running)
                    [session startRunning];
            }
            @catch (NSException* e)
            {
                juce::ignoreUnused(e);
                recordingFlag.store(false, std::memory_order_release);
                sessionBuilt = false; // force a clean rebuild next take
            }
        };

        if (auth == AVAuthorizationStatusNotDetermined)
        {
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio
                                     completionHandler:^(BOOL granted)
            {
                if (!life->load(std::memory_order_acquire))
                    return;

                if (granted)
                    dispatch_async(captureQueue, launchBlock);
                else
                    recordingFlag.store(false, std::memory_order_release);
            }];
        }
        else
        {
            dispatch_async(captureQueue, launchBlock);
        }

        return true;
    }

    //==========================================================================
    void stop(std::function<void(bool, juce::File)> onFinished)
    {
        if (!recordingFlag.load(std::memory_order_acquire))
        {
            if (onFinished)
                juce::MessageManager::callAsync([onFinished]() { onFinished(false, {}); });
            return;
        }

        recordingFlag.store(false, std::memory_order_release);

        auto life = lifeToken;
        auto finishedCopy = std::make_shared<std::function<void(bool, juce::File)>>(std::move(onFinished));

        // Everything below runs on the capture queue, strictly AFTER any
        // in-flight sample callbacks (same serial queue), so the writer can
        // be finalised without racing the last buffer. The session pointer is
        // read inside the block too — it is only mutated on this same queue.
        dispatch_async(captureQueue, ^
        {
            @try
            {
                if (session != nil && session.running)
                    [session stopRunning];
            }
            @catch (NSException* e)
            {
                juce::ignoreUnused(e);
                sessionBuilt = false;
            }

            if (!life->load(std::memory_order_acquire))
                return;

            const bool ok = finalizeWriter();
            const juce::File finished = outputFile;

            juce::MessageManager::callAsync([finishedCopy, ok, finished]()
            {
                if (*finishedCopy)
                    (*finishedCopy)(ok, finished);
            });
        });
    }

    //==========================================================================
    // Capture queue only
    //==========================================================================
    void handleSampleBuffer(CMSampleBufferRef sampleBuffer)
    {
        if (!recordingFlag.load(std::memory_order_acquire)
            || writerFailed.load(std::memory_order_relaxed))
            return;

        CMFormatDescriptionRef fmt = CMSampleBufferGetFormatDescription(sampleBuffer);
        const AudioStreamBasicDescription* asbd =
            fmt != nullptr ? CMAudioFormatDescriptionGetStreamBasicDescription(fmt) : nullptr;

        if (asbd == nullptr || asbd->mChannelsPerFrame == 0)
            return;

        const int numChannels = juce::jlimit(1, 8, (int) asbd->mChannelsPerFrame);

        if (threadedWriter == nullptr && !createWriter(asbd->mSampleRate, numChannels))
        {
            writerFailed.store(true, std::memory_order_relaxed);
            return;
        }

        // Pull the samples out via the AudioBufferList route so both
        // interleaved and planar layouts are handled uniformly.
        size_t ablSizeNeeded = 0;
        if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
                sampleBuffer, &ablSizeNeeded, nullptr, 0,
                nullptr, nullptr, 0, nullptr) != noErr && ablSizeNeeded == 0)
            return;

        if (ablStorage.size() < ablSizeNeeded)
            ablStorage.resize(ablSizeNeeded);

        auto* abl = reinterpret_cast<AudioBufferList*>(ablStorage.data());
        CMBlockBufferRef retainedBlock = nullptr;

        if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
                sampleBuffer, nullptr, abl, ablSizeNeeded,
                kCFAllocatorDefault, kCFAllocatorDefault,
                kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment,
                &retainedBlock) != noErr)
            return;

        const int numFrames = (int) CMSampleBufferGetNumSamples(sampleBuffer);
        if (numFrames <= 0)
        {
            if (retainedBlock != nullptr) CFRelease(retainedBlock);
            return;
        }

        if (scratch.getNumChannels() < numChannels || scratch.getNumSamples() < numFrames)
            scratch.setSize(numChannels, juce::jmax(numFrames, 4096), false, false, true);

        const bool isFloat = (asbd->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
        const bool planar = (asbd->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
        const int bytesPerSample = (int) (asbd->mBitsPerChannel / 8);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* dest = scratch.getWritePointer(ch);

            const ::AudioBuffer* src = planar
                ? &abl->mBuffers[juce::jmin((UInt32) ch, abl->mNumberBuffers - 1)]
                : &abl->mBuffers[0];

            const int stride = planar ? 1 : numChannels;
            const int offset = planar ? 0 : ch;

            if (isFloat && bytesPerSample == 4)
            {
                const float* s = reinterpret_cast<const float*>(src->mData) + offset;
                for (int i = 0; i < numFrames; ++i)
                    dest[i] = s[(size_t) i * (size_t) stride];
            }
            else if (!isFloat && bytesPerSample == 2)
            {
                const int16_t* s = reinterpret_cast<const int16_t*>(src->mData) + offset;
                constexpr float scale = 1.0f / 32768.0f;
                for (int i = 0; i < numFrames; ++i)
                    dest[i] = (float) s[(size_t) i * (size_t) stride] * scale;
            }
            else if (!isFloat && bytesPerSample == 4)
            {
                const int32_t* s = reinterpret_cast<const int32_t*>(src->mData) + offset;
                constexpr float scale = 1.0f / 2147483648.0f;
                for (int i = 0; i < numFrames; ++i)
                    dest[i] = (float) s[(size_t) i * (size_t) stride] * scale;
            }
            else
            {
                juce::FloatVectorOperations::clear(dest, numFrames);
            }

            // Peak feed for the UI meters
            float peak = 0.0f;
            for (int i = 0; i < numFrames; ++i)
                peak = juce::jmax(peak, std::abs(dest[i]));

            if (ch < (int) channelPeaks.size())
            {
                auto& slot = channelPeaks[(size_t) ch];
                float current = slot.load(std::memory_order_relaxed);
                while (peak > current
                       && !slot.compare_exchange_weak(current, peak, std::memory_order_relaxed))
                {}
            }
        }

        if (retainedBlock != nullptr)
            CFRelease(retainedBlock);

        // NOTE (2026-07-03): a portrait "scene rotation" used to live here, based
        // on the theory that Apple's FOA frame follows the landscape video
        // convention. A calibrated clap-walk test disproved it: measured DOAs
        // show the frame is already scene-correct in portrait (front=+X,
        // back=-X, up=+Z, left=+Y, no mirroring). Files are written exactly
        // as Apple delivers them. The real issue found instead: lateral (Y)
        // amplitude is systematically low — compensated at DECODE time, see
        // FOABinauralDecoder::lateralBoost.

        // Live-meter tap (capture queue). Give the host a view of exactly the
        // channels we captured; it reads ch0/ch1 for L/R metering.
        if (meterTap)
        {
            juce::AudioBuffer<float> view(scratch.getArrayOfWritePointers(), numChannels, numFrames);
            meterTap(view);
        }

        const float* channelPtrs[8];
        for (int ch = 0; ch < numChannels; ++ch)
            channelPtrs[ch] = scratch.getReadPointer(ch);

        if (threadedWriter->write(channelPtrs, numFrames))
            samplesWritten.fetch_add((int64_t) numFrames, std::memory_order_relaxed);
        else
            writerFailed.store(true, std::memory_order_relaxed);
    }

    bool createWriter(double sampleRate, int numChannels)
    {
        auto stream = outputFile.createOutputStream();
        if (stream == nullptr)
            return false;

        juce::WavAudioFormat wav;
        auto* rawWriter = wav.createWriterFor(stream.get(),
                                              sampleRate > 0.0 ? sampleRate : 48000.0,
                                              (unsigned int) numChannels,
                                              32, {}, 0);
        if (rawWriter == nullptr)
            return false;

        stream.release(); // writer owns it now

        threadedWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(
            rawWriter, *writerThread, 1 << 17);

        writerSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        activeChannels.store(numChannels, std::memory_order_release);
        return true;
    }

    bool finalizeWriter()
    {
        const bool wroteSomething = samplesWritten.load(std::memory_order_relaxed) > 0;
        shutdownWriter();
        return wroteSomething
            && !writerFailed.load(std::memory_order_relaxed)
            && outputFile.existsAsFile();
    }

    void shutdownWriter()
    {
        threadedWriter.reset(); // flushes queued blocks

        if (writerThread != nullptr)
        {
            writerThread->stopThread(4000);
            writerThread.reset();
        }
    }

    //==========================================================================
    AVCaptureSession* session = nil;
    AVCaptureDeviceInput* audioInput = nil;
    AVCaptureAudioDataOutput* audioOutput = nil;
    GOODMETERAmbiCaptureDelegate* delegate = nil;
    dispatch_queue_t captureQueue = nullptr;

    std::unique_ptr<juce::TimeSliceThread> writerThread;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> threadedWriter;
    juce::AudioBuffer<float> scratch;              // capture queue only
    std::vector<uint8_t> ablStorage;               // capture queue only
    juce::File outputFile;
    double writerSampleRate = 48000.0;

    std::atomic<bool> recordingFlag { false };
    std::atomic<bool> writerFailed { false };
    std::atomic<int64_t> samplesWritten { 0 };
    std::atomic<int> activeChannels { 0 };
    std::array<std::atomic<float>, 4> channelPeaks;
    bool foaActive = false;
    bool foaSupported = false;
    bool foaSupportComputed = false;
    bool spatialAvailable = false;   // capture-queue: current session FOA capability
    bool sessionBuilt = false;       // capture-queue: graph valid
    bool configuredFOA = false;      // capture-queue: mode the graph was built for
    std::function<void(const juce::AudioBuffer<float>&)> meterTap; // set at start, read on capture queue
    std::shared_ptr<std::atomic<bool>> lifeToken = std::make_shared<std::atomic<bool>>(true);
};

//==============================================================================
@implementation GOODMETERAmbiCaptureDelegate
- (void)captureOutput:(AVCaptureOutput*)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection*)connection
{
    juce::ignoreUnused(output, connection);
    if (self.owner != nullptr)
        self.owner->handleSampleBuffer(sampleBuffer);
}
@end

//==============================================================================
AmbisonicRecorder::AmbisonicRecorder() : impl(std::make_unique<Impl>()) {}
AmbisonicRecorder::~AmbisonicRecorder() = default;

bool AmbisonicRecorder::isFOACaptureSupported() const     { return impl->isFOASupported(); }
bool AmbisonicRecorder::isRecording() const               { return impl->recordingFlag.load(std::memory_order_acquire); }
bool AmbisonicRecorder::isCurrentRecordingFOA() const     { return impl->foaActive; }
int  AmbisonicRecorder::getNumActiveChannels() const      { return impl->activeChannels.load(std::memory_order_acquire); }

double AmbisonicRecorder::getElapsedSeconds() const
{
    const auto sr = impl->writerSampleRate > 0.0 ? impl->writerSampleRate : 48000.0;
    return (double) impl->samplesWritten.load(std::memory_order_relaxed) / sr;
}

float AmbisonicRecorder::getAndClearChannelPeak(int channelIndex)
{
    if (channelIndex < 0 || channelIndex >= (int) impl->channelPeaks.size())
        return 0.0f;
    return impl->channelPeaks[(size_t) channelIndex].exchange(0.0f, std::memory_order_relaxed);
}

bool AmbisonicRecorder::startRecording(const juce::File& f, CaptureMode mode, juce::String& e)
{
    impl->meterTap = onCaptureBlock;   // snapshot the tap for the capture queue
    return impl->start(f, mode, e);
}
void AmbisonicRecorder::stopRecording(std::function<void(bool, juce::File)> cb) { impl->stop(std::move(cb)); }

#else // !JUCE_IOS — inert stub so non-iOS targets link if they ever include this TU

struct AmbisonicRecorder::Impl {};
AmbisonicRecorder::AmbisonicRecorder() : impl(std::make_unique<Impl>()) {}
AmbisonicRecorder::~AmbisonicRecorder() = default;
bool AmbisonicRecorder::isFOACaptureSupported() const { return false; }
bool AmbisonicRecorder::startRecording(const juce::File&, CaptureMode, juce::String& e) { e = "iOS only"; return false; }
void AmbisonicRecorder::stopRecording(std::function<void(bool, juce::File)> cb) { if (cb) cb(false, {}); }
bool AmbisonicRecorder::isRecording() const { return false; }
double AmbisonicRecorder::getElapsedSeconds() const { return 0.0; }
int AmbisonicRecorder::getNumActiveChannels() const { return 0; }
bool AmbisonicRecorder::isCurrentRecordingFOA() const { return false; }
float AmbisonicRecorder::getAndClearChannelPeak(int) { return 0.0f; }

#endif

//==============================================================================
// Output-route probe for the FOA playback decoder choice: binaural needs
// personal listening (headphones/BT); loudspeakers get mid/side stereo
// instead (crosstalk destroys binaural cues). Lives here because this TU is
// already ObjC++ and registered — no new files, no new frameworks.
#if JUCE_IOS
bool goodmeter_outputIsHeadphones()
{
    @autoreleasepool
    {
        AVAudioSession* session = [AVAudioSession sharedInstance];
        for (AVAudioSessionPortDescription* port in session.currentRoute.outputs)
        {
            NSString* t = port.portType;
            if ([t isEqualToString:AVAudioSessionPortHeadphones]
                || [t isEqualToString:AVAudioSessionPortBluetoothA2DP]
                || [t isEqualToString:AVAudioSessionPortBluetoothHFP]
                || [t isEqualToString:AVAudioSessionPortBluetoothLE]
                || [t isEqualToString:AVAudioSessionPortUSBAudio])
                return true;
        }
        return false;
    }
}
#endif

//==============================================================================
// Headphone head-tracking (AirPods etc.) WITHOUT linking CoreMotion at build
// time: dlopen the public system framework, resolve CMHeadphoneMotionManager
// via the ObjC runtime. Gracefully unavailable on unsupported hardware.
#if JUCE_IOS
#include <dlfcn.h>
static std::atomic<float> gHeadYaw { 0.0f };
static id gHeadMotionMgr = nil;
static NSOperationQueue* gHeadMotionQueue = nil;

void goodmeter_headTracker_start()
{
    if (gHeadMotionMgr != nil)
        return;
    dlopen("/System/Library/Frameworks/CoreMotion.framework/CoreMotion", RTLD_LAZY);
    Class cls = NSClassFromString(@"CMHeadphoneMotionManager");
    if (cls == nil)
        return;
    id mgr = [[cls alloc] init];
    if (mgr == nil || ! [mgr isDeviceMotionAvailable])
        return;
    gHeadMotionQueue = [[NSOperationQueue alloc] init];
    [mgr startDeviceMotionUpdatesToQueue: gHeadMotionQueue
                             withHandler: ^(id motion, NSError* error)
    {
        if (motion != nil && error == nil)
        {
            id attitude = [motion valueForKey: @"attitude"];
            if (attitude != nil)
                gHeadYaw.store((float) [[attitude valueForKey: @"yaw"] doubleValue],
                               std::memory_order_relaxed);
        }
    }];
    gHeadMotionMgr = mgr;
}

void goodmeter_headTracker_stop()
{
    if (gHeadMotionMgr != nil)
    {
        [gHeadMotionMgr stopDeviceMotionUpdates];
        gHeadMotionMgr = nil;
        gHeadMotionQueue = nil;
    }
    gHeadYaw.store(0.0f, std::memory_order_relaxed);
}

float goodmeter_headTracker_yaw() { return gHeadYaw.load(std::memory_order_relaxed); }
#endif

//==============================================================================
// Apple system spatializer bridge: play a multichannel PCM file through
// AVAudioEngine with its channel layout intact — iOS renders it with the
// SAME spatial audio engine Apple Music uses (incl. AirPods head tracking).
// No Dolby codecs involved (plain PCM), so no licensing anywhere.
#if JUCE_IOS
static AVAudioEngine* gSpatEngine = nil;
static AVAudioPlayerNode* gSpatPlayer = nil;

void goodmeter_appleSpatial_stop();   // fwd (play() resets first)

bool goodmeter_appleSpatial_play(const char* path)
{
    @autoreleasepool
    {
        goodmeter_appleSpatial_stop();
        NSURL* url = [NSURL fileURLWithPath: [NSString stringWithUTF8String: path]];
        NSError* err = nil;
        AVAudioFile* file = [[AVAudioFile alloc] initForReading: url error: &err];
        if (file == nil || err != nil)
            return false;

        AVAudioSession* session = [AVAudioSession sharedInstance];
        [session setCategory: AVAudioSessionCategoryPlayback error: nil];
        if (@available(iOS 15.0, *))
            [session setSupportsMultichannelContent: YES error: nil];
        [session setActive: YES error: nil];

        gSpatEngine = [[AVAudioEngine alloc] init];
        gSpatPlayer = [[AVAudioPlayerNode alloc] init];
        [gSpatEngine attachNode: gSpatPlayer];
        // Keep the file's processing format (multichannel) up to the mixer:
        // the system spatializes multichannel content on supported routes.
        [gSpatEngine connect: gSpatPlayer
                          to: gSpatEngine.mainMixerNode
                      format: file.processingFormat];
        if (! [gSpatEngine startAndReturnError: &err] || err != nil)
        {
            goodmeter_appleSpatial_stop();
            return false;
        }
        [gSpatPlayer scheduleFile: file atTime: nil completionHandler: nil];
        [gSpatPlayer play];
        return true;
    }
}

void goodmeter_appleSpatial_stop()
{
    if (gSpatPlayer != nil) { [gSpatPlayer stop]; gSpatPlayer = nil; }
    if (gSpatEngine != nil) { [gSpatEngine stop]; gSpatEngine = nil; }
}

bool goodmeter_appleSpatial_isPlaying()
{
    return gSpatPlayer != nil && gSpatPlayer.isPlaying;
}
#endif
