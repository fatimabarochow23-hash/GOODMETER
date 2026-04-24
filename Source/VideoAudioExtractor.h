/*
  ==============================================================================
    VideoAudioExtractor.h
    GOODMETER - Extract audio track from video files via AVFoundation

    Usage:
      VideoAudioExtractor::extractAudio(videoFile, outputFile, [](bool ok) { ... });
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class VideoAudioExtractor
{
public:
    struct AudioTimingInfo
    {
        double sampleRate = 0.0;
        double durationSeconds = 0.0;
        double markerTimecodeFps = 30.0;
        double timecodeStartSeconds = 0.0;
        int channels = 0;
        int bitsPerSample = 0;
        int timecodeFrameQuanta = 30;
        int64_t startFrameNumber = 0;
        bool hasTimecodeMetadata = false;
        bool hasReadableStartTimecode = false;
        bool usesDropFrameTimecode = false;
        bool frameRateEstimated = false;
        juce::String formatName;
        juce::String codecName;
        juce::String startTimecodeText;
        juce::String technicalSummary;
    };

    struct VideoTimingInfo
    {
        double nominalFrameRate = 30.0;
        double timecodeStartSeconds = 0.0;
        int timecodeFrameQuanta = 30;
        int64_t startFrameNumber = 0;
        int width = 0;
        int height = 0;
        bool hasTimecodeMetadata = false;
        bool hasReadableStartTimecode = false;
        bool usesDropFrameTimecode = false;
        juce::String startTimecodeText;
        juce::String codecName;
        juce::String technicalSummary;
    };

    /** Asynchronously extract the audio track from a video file.
     *  @param videoFile   Input video (.mp4, .mov, .m4v)
     *  @param outputFile  Output audio file (.m4a)
     *  @param onComplete  Called on the JUCE message thread when done (true = success)
     */
    static void extractAudio(const juce::File& videoFile,
                             const juce::File& outputFile,
                             std::function<void(bool success)> onComplete);

    /** Asynchronously extract a frame image at the given time from a video file. */
    static void extractFrameImage(const juce::File& videoFile,
                                  double seconds,
                                  const juce::File& outputFile,
                                  std::function<void(bool success)> onComplete);

    /** Inspect the video track and report timing-related information for marker display. */
    static VideoTimingInfo getVideoTimingInfo(const juce::File& videoFile);

    /** Inspect audio files for timing, timecode and technical metadata. */
    static AudioTimingInfo getAudioTimingInfo(const juce::File& audioFile);
};
