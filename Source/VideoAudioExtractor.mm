/*
  ==============================================================================
    VideoAudioExtractor.mm
    GOODMETER - AVFoundation-based video → lossless WAV extraction (macOS only)

    Uses AVAssetReader to decode the video's audio track into raw PCM (Float32),
    then streams it through JUCE's WavAudioFormat writer to produce a 24-bit
    uncompressed WAV file. Never loads the entire file into memory.
  ==============================================================================
*/

#include "VideoAudioExtractor.h"

#if JUCE_MAC

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>

//==============================================================================
// Internal: synchronous lossless WAV extraction (runs on background thread)
//==============================================================================
static bool performWavExtraction(const juce::String& inputPathStr,
                                 const juce::String& outputPathStr)
{
    @autoreleasepool
    {
        // ── 1. Open asset and locate audio track ──
        NSString* inputPath = [NSString stringWithUTF8String:inputPathStr.toRawUTF8()];
        NSURL* inputURL = [NSURL fileURLWithPath:inputPath];

        AVAsset* asset = [AVAsset assetWithURL:inputURL];

        NSArray<AVAssetTrack*>* audioTracks = [asset tracksWithMediaType:AVMediaTypeAudio];
        if (audioTracks.count == 0)
        {
            DBG("VideoAudioExtractor: no audio track found");
            return false;
        }

        AVAssetTrack* audioTrack = audioTracks[0];

        // ── 2. Create AVAssetReader ──
        NSError* error = nil;
        AVAssetReader* reader = [[AVAssetReader alloc] initWithAsset:asset error:&error];
        if (!reader || error)
        {
            DBG("VideoAudioExtractor: failed to create AVAssetReader");
            return false;
        }

        // Decode to interleaved Float32 PCM (let AVFoundation handle all codec details)
        NSDictionary* outputSettings = @{
            AVFormatIDKey:               @(kAudioFormatLinearPCM),
            AVLinearPCMBitDepthKey:      @32,
            AVLinearPCMIsFloatKey:       @YES,
            AVLinearPCMIsBigEndianKey:   @NO,
            AVLinearPCMIsNonInterleaved: @NO
        };

        AVAssetReaderTrackOutput* trackOutput =
            [[AVAssetReaderTrackOutput alloc] initWithTrack:audioTrack
                                            outputSettings:outputSettings];

        if (![reader canAddOutput:trackOutput])
        {
            DBG("VideoAudioExtractor: cannot add track output");
            return false;
        }

        [reader addOutput:trackOutput];

        if (![reader startReading])
        {
            DBG("VideoAudioExtractor: startReading failed — "
                + juce::String([reader.error.localizedDescription UTF8String]));
            return false;
        }

        // ── 3. Determine sample rate & channel count from track format ──
        double sampleRate = 44100.0;
        int numChannels = 2;

        NSArray* formatDescriptions = audioTrack.formatDescriptions;
        if (formatDescriptions.count > 0)
        {
            CMFormatDescriptionRef formatDesc =
                (__bridge CMFormatDescriptionRef)formatDescriptions[0];
            const AudioStreamBasicDescription* asbd =
                CMAudioFormatDescriptionGetStreamBasicDescription(formatDesc);

            if (asbd != nullptr)
            {
                sampleRate  = asbd->mSampleRate;
                numChannels = static_cast<int>(asbd->mChannelsPerFrame);
            }
        }

        // ── 4. Create JUCE WAV writer (24-bit, professional quality) ──
        juce::File outputFile(outputPathStr);
        auto outputStream = outputFile.createOutputStream();
        if (!outputStream)
        {
            DBG("VideoAudioExtractor: failed to create output stream");
            [reader cancelReading];
            return false;
        }

        juce::WavAudioFormat wavFormat;
        // createWriterFor takes ownership of the OutputStream
        std::unique_ptr<juce::AudioFormatWriter> writer(
            wavFormat.createWriterFor(outputStream.release(),
                                      sampleRate,
                                      static_cast<unsigned int>(numChannels),
                                      24,     // 24-bit lossless
                                      {},     // no metadata
                                      0));    // quality option

        if (!writer)
        {
            DBG("VideoAudioExtractor: failed to create WAV writer");
            [reader cancelReading];
            return false;
        }

        // ── 5. Stream: read CMSampleBuffers → deinterleave → write WAV ──
        const int juceBlockSize = 8192;
        juce::AudioBuffer<float> juceBuffer(numChannels, juceBlockSize);

        while (reader.status == AVAssetReaderStatusReading)
        {
            @autoreleasepool
            {
                CMSampleBufferRef sampleBuffer = [trackOutput copyNextSampleBuffer];
                if (!sampleBuffer)
                    break;

                CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
                if (blockBuffer == nullptr)
                {
                    // No data buffer — video-only frame or empty sample; skip safely
                    CFRelease(sampleBuffer);
                    continue;
                }

                size_t totalLength = 0;
                char* dataPointer = nullptr;
                OSStatus status = CMBlockBufferGetDataPointer(
                    blockBuffer, 0, nullptr, &totalLength, &dataPointer);

                if (status != kCMBlockBufferNoErr || dataPointer == nullptr || totalLength == 0)
                {
                    CFRelease(sampleBuffer);
                    continue;
                }

                // Validate totalLength is aligned to float frames
                if (totalLength < sizeof(float) * static_cast<size_t>(numChannels))
                {
                    CFRelease(sampleBuffer);
                    continue;
                }

                // Data is interleaved Float32
                const float* floatData = reinterpret_cast<const float*>(dataPointer);
                int totalSamples = static_cast<int>(totalLength / sizeof(float));
                int numFrames = totalSamples / numChannels;

                // Deinterleave into JUCE AudioBuffer and write in chunks
                int framesProcessed = 0;
                while (framesProcessed < numFrames)
                {
                    int framesToProcess = juce::jmin(juceBlockSize, numFrames - framesProcessed);
                    juceBuffer.setSize(numChannels, framesToProcess, false, false, true);

                    // Deinterleave: interleaved [L0 R0 L1 R1 ...] → per-channel arrays
                    for (int frame = 0; frame < framesToProcess; ++frame)
                    {
                        int srcIdx = (framesProcessed + frame) * numChannels;
                        for (int ch = 0; ch < numChannels; ++ch)
                            juceBuffer.setSample(ch, frame, floatData[srcIdx + ch]);
                    }

                    writer->writeFromAudioSampleBuffer(juceBuffer, 0, framesToProcess);
                    framesProcessed += framesToProcess;
                }

                CFRelease(sampleBuffer);
            }
        }

        // ── 6. Flush and close ──
        writer.reset();

        bool ok = (reader.status == AVAssetReaderStatusCompleted);
        if (!ok)
        {
            DBG("VideoAudioExtractor: reader finished with status "
                + juce::String((int)reader.status));
            if (reader.error)
                DBG("  error: " + juce::String([reader.error.localizedDescription UTF8String]));
        }

        return ok;
    }
}

//==============================================================================
void VideoAudioExtractor::extractAudio(const juce::File& videoFile,
                                       const juce::File& outputFile,
                                       std::function<void(bool success)> onComplete)
{
    // Ensure output directory exists
    outputFile.getParentDirectory().createDirectory();

    // Delete any existing file at output path
    if (outputFile.existsAsFile())
        outputFile.deleteFile();

    // Capture paths and callback for the background block.
    // The callback is wrapped in shared_ptr for ObjC++ block capture safety.
    // The caller is responsible for ensuring their captured state is valid
    // (e.g. using Component::SafePointer in the lambda passed as onComplete).
    juce::String inputPath  = videoFile.getFullPathName();
    juce::String outputPath = outputFile.getFullPathName();
    auto callback = std::make_shared<std::function<void(bool)>>(std::move(onComplete));

    // Run heavy I/O on a GCD background thread
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        bool success = performWavExtraction(inputPath, outputPath);

        // Callback on the JUCE message thread — guard against post-shutdown callAsync
        auto cb = callback;
        if (juce::MessageManager::getInstanceWithoutCreating() != nullptr)
        {
            juce::MessageManager::callAsync([cb, success]()
            {
                if (*cb) (*cb)(success);
            });
        }
    });
}

#else

// Non-macOS stub
void VideoAudioExtractor::extractAudio(const juce::File&,
                                       const juce::File&,
                                       std::function<void(bool success)> onComplete)
{
    juce::MessageManager::callAsync([onComplete]()
    {
        if (onComplete) onComplete(false);
    });
}

#endif
