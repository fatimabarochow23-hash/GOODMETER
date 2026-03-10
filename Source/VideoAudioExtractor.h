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
    /** Asynchronously extract the audio track from a video file.
     *  @param videoFile   Input video (.mp4, .mov, .m4v)
     *  @param outputFile  Output audio file (.m4a)
     *  @param onComplete  Called on the JUCE message thread when done (true = success)
     */
    static void extractAudio(const juce::File& videoFile,
                             const juce::File& outputFile,
                             std::function<void(bool success)> onComplete);
};
