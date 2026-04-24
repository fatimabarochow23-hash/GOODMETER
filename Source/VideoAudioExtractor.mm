/*
  ==============================================================================
    VideoAudioExtractor.mm
    GOODMETER - AVFoundation-based video → lossless WAV extraction (macOS / iOS)

    Uses AVAssetReader to decode the video's audio track into raw PCM (Float32),
    then streams it through JUCE's WavAudioFormat writer to produce a 24-bit
    uncompressed WAV file. Never loads the entire file into memory.
  ==============================================================================
*/

#include "VideoAudioExtractor.h"
#include "iOS/IOSShareHelpers.h"

#if JUCE_MAC || JUCE_IOS

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#if JUCE_IOS
 #import <UIKit/UIKit.h>
#endif

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

static bool performFrameExtraction(const juce::String& inputPathStr,
                                   double seconds,
                                   const juce::String& outputPathStr)
{
    @autoreleasepool
    {
        NSString* inputPath = [NSString stringWithUTF8String:inputPathStr.toRawUTF8()];
        NSURL* inputURL = [NSURL fileURLWithPath:inputPath];
        AVAsset* asset = [AVAsset assetWithURL:inputURL];

        NSArray<AVAssetTrack*>* videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
        if (videoTracks.count == 0)
            return false;

        AVAssetTrack* videoTrack = [videoTracks objectAtIndex:0];
        const double durationSeconds = juce::jmax(0.0, CMTimeGetSeconds(asset.duration));
        double nominalFrameRate = (double) videoTrack.nominalFrameRate;
        if (!std::isfinite(nominalFrameRate) || nominalFrameRate <= 1.0)
            nominalFrameRate = 30.0;

        const double frameStep = 1.0 / nominalFrameRate;
        const double clampedSeconds = durationSeconds > frameStep
            ? juce::jlimit(0.0, durationSeconds - frameStep, seconds)
            : juce::jmax(0.0, seconds);

        AVAssetImageGenerator* imageGenerator = [[AVAssetImageGenerator alloc] initWithAsset:asset];
        imageGenerator.appliesPreferredTrackTransform = YES;
        imageGenerator.apertureMode = AVAssetImageGeneratorApertureModeEncodedPixels;
        imageGenerator.maximumSize = CGSizeMake(1920.0, 1920.0);
        imageGenerator.requestedTimeToleranceBefore = CMTimeMakeWithSeconds(frameStep * 0.5, 6000);
        imageGenerator.requestedTimeToleranceAfter = CMTimeMakeWithSeconds(frameStep * 0.5, 6000);

        NSError* error = nil;
        CMTime target = CMTimeMakeWithSeconds(clampedSeconds, 6000);
        CMTime actualTime = kCMTimeZero;
        CGImageRef cgImage = [imageGenerator copyCGImageAtTime:target actualTime:&actualTime error:&error];
        if (cgImage == nil || error != nil)
        {
            DBG("VideoAudioExtractor: frame extraction failed");
            return false;
        }

        juce::File outputFile(outputPathStr);
        outputFile.getParentDirectory().createDirectory();
        if (outputFile.existsAsFile())
            outputFile.deleteFile();

        NSString* outputPath = [NSString stringWithUTF8String:outputPathStr.toRawUTF8()];
        if (outputPath == nil)
        {
            CGImageRelease(cgImage);
            return false;
        }

        UIImage* rawImage = [UIImage imageWithCGImage:cgImage];
        const CGSize imageSize = CGSizeMake(CGImageGetWidth(cgImage), CGImageGetHeight(cgImage));
        UIGraphicsBeginImageContextWithOptions(imageSize, YES, 1.0);
        [rawImage drawInRect:CGRectMake(0.0, 0.0, imageSize.width, imageSize.height)];
        UIImage* flattenedImage = UIGraphicsGetImageFromCurrentImageContext();
        UIGraphicsEndImageContext();

        NSData* pngData = UIImagePNGRepresentation(flattenedImage != nil ? flattenedImage : rawImage);
        CGImageRelease(cgImage);

        if (pngData == nil)
            return false;

        return [pngData writeToFile:outputPath atomically:YES];
    }
}

static juce::String formatTimecodeFrameNumber(int64_t frameNumber,
                                              int frameQuanta,
                                              bool dropFrame)
{
    frameQuanta = juce::jmax(1, frameQuanta);
    frameNumber = juce::jmax<int64_t>(0, frameNumber);

    if (dropFrame)
    {
        const int dropFrames = juce::jmax(2, (int) std::round((double) frameQuanta * 0.0666666667));
        const int64_t framesPerHour = (int64_t) frameQuanta * 60 * 60;
        const int64_t framesPer24Hours = framesPerHour * 24;
        const int64_t framesPer10Minutes = (int64_t) frameQuanta * 60 * 10 - (int64_t) dropFrames * 9;
        const int64_t framesPerMinute = (int64_t) frameQuanta * 60 - dropFrames;

        frameNumber %= framesPer24Hours;
        const int64_t tenMinuteBlocks = frameNumber / framesPer10Minutes;
        const int64_t leftover = frameNumber % framesPer10Minutes;

        int64_t adjustedFrameNumber = frameNumber + tenMinuteBlocks * dropFrames * 9;
        if (leftover >= dropFrames)
            adjustedFrameNumber += dropFrames * ((leftover - dropFrames) / framesPerMinute + 1);

        const int hours = (int) (adjustedFrameNumber / framesPerHour);
        const int minutes = (int) ((adjustedFrameNumber / ((int64_t) frameQuanta * 60)) % 60);
        const int seconds = (int) ((adjustedFrameNumber / frameQuanta) % 60);
        const int frames = (int) (adjustedFrameNumber % frameQuanta);
        return juce::String::formatted("%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
    }

    const int64_t framesPerHour = (int64_t) frameQuanta * 60 * 60;
    const int64_t framesPer24Hours = framesPerHour * 24;
    frameNumber %= framesPer24Hours;

    const int hours = (int) (frameNumber / framesPerHour);
    frameNumber %= framesPerHour;
    const int minutes = (int) (frameNumber / ((int64_t) frameQuanta * 60));
    frameNumber %= (int64_t) frameQuanta * 60;
    const int seconds = (int) (frameNumber / frameQuanta);
    const int frames = (int) (frameNumber % frameQuanta);
    return juce::String::formatted("%02d:%02d:%02d:%02d", hours, minutes, seconds, frames);
}

static double normaliseCommonFrameRate(double fps)
{
    if (!std::isfinite(fps) || fps <= 1.0)
        return 30.0;

    if (std::abs(fps - 23.976) < 0.08 || std::abs(fps - 24.0) < 0.08)
        return 24.0;
    if (std::abs(fps - 25.0) < 0.08)
        return 25.0;
    if (std::abs(fps - 29.97) < 0.08 || std::abs(fps - 30.0) < 0.08)
        return 30.0;
    if (std::abs(fps - 50.0) < 0.08)
        return 50.0;
    if (std::abs(fps - 59.94) < 0.08 || std::abs(fps - 60.0) < 0.08)
        return 60.0;

    return std::round(fps);
}

static juce::String formatFrameRateLabel(double fps)
{
    if (!std::isfinite(fps) || fps <= 1.0)
        return {};

    if (std::abs(fps - 23.976) < 0.01)
        return "23.976 fps";
    if (std::abs(fps - 29.97) < 0.01)
        return "29.97 fps";
    if (std::abs(fps - 59.94) < 0.01)
        return "59.94 fps";

    if (std::abs(fps - std::round(fps)) < 0.01)
        return juce::String((int) std::round(fps)) + " fps";

    return juce::String(fps, 2) + " fps";
}

static juce::String formatSampleRateLabel(double sampleRate)
{
    if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
        return {};

    if (std::abs(sampleRate - std::round(sampleRate)) < 0.01)
        return juce::String((int) std::round(sampleRate)) + " Hz";

    return juce::String(sampleRate, 1) + " Hz";
}

static juce::String formatChannelLabel(int channels)
{
    if (channels <= 0)
        return {};

    return juce::String(channels) + " Ch";
}

static juce::String uppercaseContainerLabel(const juce::File& file)
{
    auto ext = file.getFileExtension().toUpperCase();
    if (ext.startsWithChar('.'))
        ext = ext.substring(1);
    return ext;
}

static juce::String mapVideoCodecName(FourCharCode codec)
{
    switch (codec)
    {
        case 'avc1': return "H.264";
        case 'h264': return "H.264";
        case 'hvc1': return "H.265";
        case 'hev1': return "H.265";
        case 'apch':
        case 'apcn':
        case 'apcs':
        case 'apco':
        case 'ap4h':
        case 'ap4x': return "ProRes";
        case 'jpeg': return "JPEG";
        default: break;
    }

    char fourcc[] =
    {
        (char) ((codec >> 24) & 0xFF),
        (char) ((codec >> 16) & 0xFF),
        (char) ((codec >> 8) & 0xFF),
        (char) (codec & 0xFF),
        0
    };

    juce::String raw(fourcc);
    raw = raw.trim();
    return raw.isNotEmpty() ? raw : "Video";
}

struct ParsedTimecode
{
    bool valid = false;
    bool dropFrame = false;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    int frames = 0;
    juce::String originalText;
};

static ParsedTimecode parseTimecodeText(const juce::String& rawText)
{
    ParsedTimecode tc;
    auto text = rawText.trim();
    if (text.isEmpty())
        return tc;

    text = text.retainCharacters("0123456789:;");
    if (text.isEmpty())
        return tc;

    tc.dropFrame = text.containsChar(';');
    text = text.replaceCharacter(';', ':');

    juce::StringArray parts;
    parts.addTokens(text, ":", {});
    parts.trim();
    parts.removeEmptyStrings();

    if (parts.size() != 4)
        return tc;

    tc.hours = parts[0].getIntValue();
    tc.minutes = parts[1].getIntValue();
    tc.seconds = parts[2].getIntValue();
    tc.frames = parts[3].getIntValue();
    tc.originalText = rawText.trim();
    tc.valid = tc.hours >= 0 && tc.minutes >= 0 && tc.seconds >= 0 && tc.frames >= 0;
    return tc;
}

static int64_t timecodeToFrameNumber(const ParsedTimecode& tc, int frameQuanta, bool dropFrame)
{
    frameQuanta = juce::jmax(1, frameQuanta);
    const int64_t base = (((int64_t) tc.hours * 60 * 60)
                        + ((int64_t) tc.minutes * 60)
                        + (int64_t) tc.seconds) * frameQuanta
                        + (int64_t) tc.frames;

    if (!dropFrame)
        return base;

    const int dropFrames = juce::jmax(2, (int) std::round((double) frameQuanta * 0.0666666667));
    const int totalMinutes = tc.hours * 60 + tc.minutes;
    return base - (int64_t) dropFrames * (totalMinutes - totalMinutes / 10);
}

static double parseFrameRateHint(const juce::StringPairArray& metadata)
{
    auto parseNumber = [](juce::String text) -> double
    {
        text = text.retainCharacters("0123456789.");
        return text.isNotEmpty() ? text.getDoubleValue() : 0.0;
    };

    for (int i = 0; i < metadata.size(); ++i)
    {
        const auto key = metadata.getAllKeys()[i];
        const auto value = metadata.getAllValues()[i];
        if (key.containsIgnoreCase("fps")
            || key.containsIgnoreCase("frame")
            || value.containsIgnoreCase("fps"))
        {
            const double parsed = parseNumber(value);
            if (parsed > 1.0)
                return normaliseCommonFrameRate(parsed);
        }
    }

    return 0.0;
}

static double inferAudioTimecodeFps(const ParsedTimecode* startTc,
                                    const ParsedTimecode* endTc,
                                    double durationSeconds,
                                    double bwavStartSeconds,
                                    double explicitHint)
{
    if (explicitHint > 1.0)
        return normaliseCommonFrameRate(explicitHint);

    const bool wantsDropFrame = (startTc != nullptr && startTc->dropFrame)
                             || (endTc != nullptr && endTc->dropFrame);

    const std::array<double, 5> candidates { 24.0, 25.0, 30.0, 50.0, 60.0 };
    double bestFps = 0.0;
    double bestError = std::numeric_limits<double>::max();

    for (const auto fpsValue : candidates)
    {
        if (wantsDropFrame && fpsValue != 30.0 && fpsValue != 60.0)
            continue;

        const int frameQuanta = (int) std::round(fpsValue);
        const bool drop = wantsDropFrame && (frameQuanta == 30 || frameQuanta == 60);
        double totalError = 0.0;
        bool hasScore = false;

        if (startTc != nullptr && endTc != nullptr && durationSeconds > 0.0)
        {
            const auto startFrame = timecodeToFrameNumber(*startTc, frameQuanta, drop);
            const auto endFrame = timecodeToFrameNumber(*endTc, frameQuanta, drop);
            const double tcDuration = (double) (endFrame - startFrame) / fpsValue;
            totalError += std::abs(tcDuration - durationSeconds);
            hasScore = true;
        }

        if (startTc != nullptr && bwavStartSeconds > 0.0)
        {
            const auto startFrame = timecodeToFrameNumber(*startTc, frameQuanta, drop);
            const double startSeconds = (double) startFrame / fpsValue;
            totalError += std::abs(startSeconds - bwavStartSeconds);
            hasScore = true;
        }

        if (endTc != nullptr && startTc == nullptr && durationSeconds > 0.0)
        {
            const auto endFrame = timecodeToFrameNumber(*endTc, frameQuanta, drop);
            const double tcDuration = (double) endFrame / fpsValue;
            totalError += std::abs(tcDuration - durationSeconds);
            hasScore = true;
        }

        if (hasScore && totalError < bestError)
        {
            bestError = totalError;
            bestFps = fpsValue;
        }
    }

    if (bestFps > 1.0 && bestError < 1.2)
        return normaliseCommonFrameRate(bestFps);

    if (wantsDropFrame)
        return 30.0;

    const auto fallbackTc = startTc != nullptr ? startTc : endTc;
    if (fallbackTc != nullptr)
    {
        if (fallbackTc->frames >= 25)
            return 30.0;
        return 25.0;
    }

    return 30.0;
}

static juce::String buildAudioTechnicalSummary(const juce::File& file,
                                               const VideoAudioExtractor::AudioTimingInfo& info)
{
    juce::StringArray parts;
    const auto container = uppercaseContainerLabel(file);
    if (container.isNotEmpty())
        parts.add(container);

    if (info.codecName.isNotEmpty() && info.codecName != container)
        parts.add(info.codecName);

    const auto sampleRateLabel = formatSampleRateLabel(info.sampleRate);
    if (sampleRateLabel.isNotEmpty())
        parts.add(sampleRateLabel);

    const auto channelLabel = formatChannelLabel(info.channels);
    if (channelLabel.isNotEmpty())
        parts.add(channelLabel);

    if (info.bitsPerSample > 0)
        parts.add(juce::String(info.bitsPerSample) + "-bit");

    if (info.startTimecodeText.isNotEmpty())
        parts.add("TC " + info.startTimecodeText);

    return parts.joinIntoString(" · ");
}

static juce::String buildVideoTechnicalSummary(const juce::File& file,
                                               const VideoAudioExtractor::VideoTimingInfo& info)
{
    juce::StringArray parts;
    const auto container = uppercaseContainerLabel(file);
    if (container.isNotEmpty())
        parts.add(container);

    if (info.codecName.isNotEmpty())
        parts.add(info.codecName);

    if (info.width > 0 && info.height > 0)
        parts.add(juce::String(info.width) + "x" + juce::String(info.height));

    const auto fpsLabel = formatFrameRateLabel(info.nominalFrameRate);
    if (fpsLabel.isNotEmpty())
        parts.add(fpsLabel);

    if (info.startTimecodeText.isNotEmpty())
        parts.add("TC " + info.startTimecodeText);

    return parts.joinIntoString(" · ");
}

static VideoAudioExtractor::VideoTimingInfo readVideoTimingInfo(const juce::String& inputPathStr)
{
    VideoAudioExtractor::VideoTimingInfo info;
    const juce::File sourceFile(inputPathStr);

    @autoreleasepool
    {
        NSString* inputPath = [NSString stringWithUTF8String:inputPathStr.toRawUTF8()];
        NSURL* inputURL = [NSURL fileURLWithPath:inputPath];
        AVAsset* asset = [AVAsset assetWithURL:inputURL];

        NSArray<AVAssetTrack*>* videoTracks = [asset tracksWithMediaType:AVMediaTypeVideo];
        if (videoTracks.count > 0)
        {
            AVAssetTrack* videoTrack = videoTracks[0];
            const float nominal = videoTrack.nominalFrameRate;
            if (std::isfinite((double) nominal) && nominal > 1.0f)
                info.nominalFrameRate = nominal;

            const auto natural = videoTrack.naturalSize;
            info.width = std::abs((int) std::round(natural.width));
            info.height = std::abs((int) std::round(natural.height));

            NSArray* formatDescriptions = videoTrack.formatDescriptions;
            if (formatDescriptions.count > 0)
            {
                CMFormatDescriptionRef formatDesc = (__bridge CMFormatDescriptionRef) formatDescriptions[0];
                info.codecName = mapVideoCodecName(CMFormatDescriptionGetMediaSubType(formatDesc));
            }
        }

#ifdef AVMediaTypeTimecode
        NSArray<AVAssetTrack*>* timecodeTracks = [asset tracksWithMediaType:AVMediaTypeTimecode];
        info.hasTimecodeMetadata = timecodeTracks.count > 0;
        if (timecodeTracks.count > 0)
        {
            NSError* readerError = nil;
            AVAssetReader* reader = [[AVAssetReader alloc] initWithAsset:asset error:&readerError];
            AVAssetTrack* timecodeTrack = [timecodeTracks objectAtIndex:0];

            if (reader != nil && timecodeTrack != nil)
            {
                AVAssetReaderTrackOutput* output = [AVAssetReaderTrackOutput assetReaderTrackOutputWithTrack:timecodeTrack
                                                                                              outputSettings:nil];
                if ([reader canAddOutput:output])
                {
                    [reader addOutput:output];

                    if ([reader startReading])
                    {
                        CMSampleBufferRef sampleBuffer = [output copyNextSampleBuffer];
                        if (sampleBuffer != nullptr)
                        {
                            CMBlockBufferRef blockBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
                            CMFormatDescriptionRef formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);

                            if (blockBuffer != nullptr && formatDescription != nullptr)
                            {
                                size_t totalLength = 0;
                                char* rawData = nullptr;
                                const OSStatus status = CMBlockBufferGetDataPointer(blockBuffer, 0, nullptr, &totalLength, &rawData);

                                if (status == kCMBlockBufferNoErr && rawData != nullptr)
                                {
                                    const auto formatType = CMTimeCodeFormatDescriptionGetFormatType((CMTimeCodeFormatDescriptionRef) formatDescription);
                                    info.timecodeFrameQuanta = (int) CMTimeCodeFormatDescriptionGetFrameQuanta((CMTimeCodeFormatDescriptionRef) formatDescription);
                                    info.usesDropFrameTimecode = (CMTimeCodeFormatDescriptionGetTimeCodeFlags((CMTimeCodeFormatDescriptionRef) formatDescription) & kCMTimeCodeFlag_DropFrame) != 0;
                                    info.timecodeStartSeconds = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer));

                                    int64_t frameNumber = 0;
                                    bool parsed = false;

                                    if (formatType == kCMTimeCodeFormatType_TimeCode32 && totalLength >= sizeof(int32_t))
                                    {
                                        int32_t beValue = 0;
                                        std::memcpy(&beValue, rawData, sizeof(beValue));
                                        frameNumber = (int64_t) EndianS32_BtoN(beValue);
                                        parsed = true;
                                    }
#ifdef kCMTimeCodeFormatType_TimeCode64
                                    else if (formatType == kCMTimeCodeFormatType_TimeCode64 && totalLength >= sizeof(int64_t))
                                    {
                                        int64_t beValue = 0;
                                        std::memcpy(&beValue, rawData, sizeof(beValue));
                                        frameNumber = (int64_t) EndianS64_BtoN(beValue);
                                        parsed = true;
                                    }
#endif

                                    if (parsed)
                                    {
                                        info.startFrameNumber = frameNumber;
                                        info.hasReadableStartTimecode = true;
                                        info.startTimecodeText = formatTimecodeFrameNumber(frameNumber,
                                                                                           juce::jmax(1, info.timecodeFrameQuanta),
                                                                                           info.usesDropFrameTimecode);
                                        if (info.timecodeFrameQuanta > 1)
                                            info.nominalFrameRate = info.timecodeFrameQuanta;
                                    }
                                }
                            }

                            CFRelease(sampleBuffer);
                        }
                    }
                }
            }
        }
#endif
    }

    info.nominalFrameRate = normaliseCommonFrameRate(info.nominalFrameRate);
    info.technicalSummary = buildVideoTechnicalSummary(sourceFile, info);

    return info;
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

void VideoAudioExtractor::extractFrameImage(const juce::File& videoFile,
                                            double seconds,
                                            const juce::File& outputFile,
                                            std::function<void(bool success)> onComplete)
{
    outputFile.getParentDirectory().createDirectory();

    juce::String inputPath = videoFile.getFullPathName();
    juce::String outputPath = outputFile.getFullPathName();
    auto callback = std::make_shared<std::function<void(bool)>>(std::move(onComplete));

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        bool success = performFrameExtraction(inputPath, seconds, outputPath);
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

VideoAudioExtractor::VideoTimingInfo VideoAudioExtractor::getVideoTimingInfo(const juce::File& videoFile)
{
    return readVideoTimingInfo(videoFile.getFullPathName());
}

VideoAudioExtractor::AudioTimingInfo VideoAudioExtractor::getAudioTimingInfo(const juce::File& audioFile)
{
    VideoAudioExtractor::AudioTimingInfo info;

    if (!audioFile.existsAsFile())
        return info;

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (reader == nullptr)
        return info;

    info.formatName = reader->getFormatName();
    info.sampleRate = reader->sampleRate;
    info.channels = (int) reader->numChannels;
    info.bitsPerSample = (int) reader->bitsPerSample;
    info.durationSeconds = reader->sampleRate > 0.0
        ? (double) reader->lengthInSamples / reader->sampleRate
        : 0.0;

    const bool isPcmLike = info.formatName.containsIgnoreCase("wav")
                        || info.formatName.containsIgnoreCase("aiff")
                        || info.formatName.containsIgnoreCase("caf");
    if (reader->usesFloatingPointData)
        info.codecName = "Float PCM";
    else if (isPcmLike)
        info.codecName = "Linear PCM";
    else
        info.codecName = info.formatName;

    const auto& metadata = reader->metadataValues;
    const auto bwavTimeReference = metadata.getValue(juce::WavAudioFormat::bwavTimeReference, {});
    const auto startTimecodeText = metadata.getValue(juce::WavAudioFormat::riffInfoStartTimecode, {});
    const auto inlineTimecodeText = metadata.getValue(juce::WavAudioFormat::riffInfoTimeCode, {});
    const auto endTimecodeText = metadata.getValue(juce::WavAudioFormat::riffInfoEndTimecode, {});

    const auto parsedStart = parseTimecodeText(startTimecodeText.isNotEmpty() ? startTimecodeText : inlineTimecodeText);
    const auto parsedEnd = parseTimecodeText(endTimecodeText);

    const bool hasBwavReference = bwavTimeReference.isNotEmpty();
    const double bwavStartSeconds = (hasBwavReference && info.sampleRate > 0.0)
        ? ((double) bwavTimeReference.getLargeIntValue() / info.sampleRate)
        : 0.0;

    const double frameRateHint = parseFrameRateHint(metadata);
    const bool hasTcString = parsedStart.valid || parsedEnd.valid;
    info.hasTimecodeMetadata = hasBwavReference || hasTcString;

    if (info.hasTimecodeMetadata)
    {
        info.markerTimecodeFps = inferAudioTimecodeFps(parsedStart.valid ? &parsedStart : nullptr,
                                                       parsedEnd.valid ? &parsedEnd : nullptr,
                                                       info.durationSeconds,
                                                       bwavStartSeconds,
                                                       frameRateHint);
        info.markerTimecodeFps = normaliseCommonFrameRate(info.markerTimecodeFps);
        info.timecodeFrameQuanta = juce::jmax(1, (int) std::round(info.markerTimecodeFps));
        info.usesDropFrameTimecode = parsedStart.dropFrame || parsedEnd.dropFrame;
        info.frameRateEstimated = frameRateHint <= 1.0;

        if (parsedStart.valid)
        {
            info.startFrameNumber = timecodeToFrameNumber(parsedStart,
                                                          info.timecodeFrameQuanta,
                                                          info.usesDropFrameTimecode);
            info.hasReadableStartTimecode = true;
        }
        else if (parsedEnd.valid && info.durationSeconds > 0.0)
        {
            const auto endFrame = timecodeToFrameNumber(parsedEnd,
                                                        info.timecodeFrameQuanta,
                                                        info.usesDropFrameTimecode);
            info.startFrameNumber = juce::jmax<int64_t>(0,
                endFrame - (int64_t) std::llround(info.durationSeconds * info.markerTimecodeFps));
            info.hasReadableStartTimecode = true;
        }
        else if (hasBwavReference)
        {
            info.startFrameNumber = juce::jmax<int64_t>(0,
                (int64_t) std::llround(bwavStartSeconds * info.markerTimecodeFps));
            info.hasReadableStartTimecode = true;
        }

        if (info.hasReadableStartTimecode)
            info.startTimecodeText = formatTimecodeFrameNumber(info.startFrameNumber,
                                                               info.timecodeFrameQuanta,
                                                               info.usesDropFrameTimecode);
    }

    info.technicalSummary = buildAudioTechnicalSummary(audioFile, info);
    return info;
}

void GoodMeterIOSShareHelpers::shareFile(const juce::File& file)
{
    if (!file.exists())
        return;

    @autoreleasepool
    {
        NSString* outputPath = [NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()];
        if (outputPath == nil)
            return;

        NSURL* fileURL = [NSURL fileURLWithPath:outputPath];
        if (fileURL == nil)
            return;

        dispatch_async(dispatch_get_main_queue(), ^
        {
            UIWindow* targetWindow = nil;

            if (@available(iOS 13.0, *))
            {
                for (UIScene* scene in [UIApplication sharedApplication].connectedScenes)
                {
                    if (![scene isKindOfClass:[UIWindowScene class]])
                        continue;

                    UIWindowScene* windowScene = (UIWindowScene*) scene;
                    for (UIWindow* window in windowScene.windows)
                    {
                        if (window.isKeyWindow)
                        {
                            targetWindow = window;
                            break;
                        }
                    }

                    if (targetWindow != nil)
                        break;
                }
            }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            if (targetWindow == nil)
                targetWindow = [UIApplication sharedApplication].keyWindow;
#pragma clang diagnostic pop

            UIViewController* presenter = targetWindow.rootViewController;
            while (presenter.presentedViewController != nil)
                presenter = presenter.presentedViewController;

            if (presenter == nil)
                return;

            UIActivityViewController* activity =
                [[UIActivityViewController alloc] initWithActivityItems:@[ fileURL ]
                                                  applicationActivities:nil];

            UIPopoverPresentationController* popover = activity.popoverPresentationController;
            if (popover != nil)
            {
                popover.sourceView = presenter.view;
                popover.sourceRect = CGRectMake(CGRectGetMidX(presenter.view.bounds),
                                                CGRectGetMaxY(presenter.view.bounds) - 40.0f,
                                                1.0f,
                                                1.0f);
            }

            [presenter presentViewController:activity animated:YES completion:nil];
        });
    }
}

#else

// Unsupported-platform stub
void VideoAudioExtractor::extractAudio(const juce::File&,
                                       const juce::File&,
                                       std::function<void(bool success)> onComplete)
{
    juce::MessageManager::callAsync([onComplete]()
    {
        if (onComplete) onComplete(false);
    });
}

void VideoAudioExtractor::extractFrameImage(const juce::File&,
                                            double,
                                            const juce::File&,
                                            std::function<void(bool success)> onComplete)
{
    juce::MessageManager::callAsync([onComplete]()
    {
        if (onComplete) onComplete(false);
    });
}

void GoodMeterIOSShareHelpers::shareFile(const juce::File&)
{
}

#endif
