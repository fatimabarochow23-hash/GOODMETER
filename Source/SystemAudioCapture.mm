/*
  ==============================================================================
    SystemAudioCapture.mm
    GOODMETER - CoreAudio Process Tap System Audio Capture (Obj-C++ implementation)

    Architecture:
      - CATapDescription creates a stereo global tap (excluding own process)
      - AudioHardwareCreateProcessTap → tapObjectID
      - Aggregate Device wraps the tap as an input source
      - IOProc callback on hardware audio thread reads Float32 PCM
      - Samples pushed to AbstractFifo ring buffer (lock-free SPSC)
      - processBlock reads from AbstractFifo

    Permission: NSAudioCaptureUsageDescription → "仅系统录音 (System Audio Recording Only)"
    Requires macOS 14.2+ (Sonoma) for AudioHardwareCreateProcessTap.
  ==============================================================================
*/

#include "SystemAudioCapture.h"

#if JUCE_MAC && JucePlugin_Build_Standalone

#import <Foundation/Foundation.h>
#import <CoreAudio/CoreAudio.h>
#import <CoreAudio/AudioHardwareTapping.h>
#import <CoreAudio/CATapDescription.h>

//==============================================================================
// C++ PIMPL Implementation — CoreAudio Process Tap
//==============================================================================
struct SystemAudioCapture::Impl
{
    static constexpr int fifoSize = 131072;  // ~2.7s at 48kHz

    // Ring buffer (lock-free SPSC bridge: IOProc → processBlock)
    std::unique_ptr<juce::AbstractFifo> fifo;
    std::vector<float> ringBuffer;  // stereo interleaved: [L0, R0, L1, R1, ...]

    // CoreAudio objects
    AudioObjectID tapObjectID          = kAudioObjectUnknown;
    AudioObjectID aggregateDeviceID    = kAudioObjectUnknown;
    AudioDeviceIOProcID ioProcID       = nullptr;
    NSUUID* tapUUID                    = nil;

    // State
    std::atomic<bool> active { false };
    std::atomic<double> sampleRate { 48000.0 };

    Impl()
    {
        fifo = std::make_unique<juce::AbstractFifo>(fifoSize);
        ringBuffer.resize(static_cast<size_t>(fifoSize) * 2, 0.0f);
    }

    ~Impl()
    {
        stopCapture();
    }

    //==========================================================================
    // IOProc callback — runs on hardware audio thread (real-time)
    //==========================================================================
    static OSStatus ioProcCallback(
        AudioObjectID           inDevice,
        const AudioTimeStamp*   inNow,
        const AudioBufferList*  inInputData,
        const AudioTimeStamp*   inInputTime,
        AudioBufferList*        outOutputData,
        const AudioTimeStamp*   inOutputTime,
        void*                   inClientData)
    {
        juce::ignoreUnused(inDevice, inNow, inInputTime, inOutputTime);

        auto* impl = static_cast<Impl*>(inClientData);

        if (!impl->active.load(std::memory_order_relaxed))
            return noErr;

        if (inInputData == nullptr || inInputData->mNumberBuffers < 1)
            return noErr;

        const AudioBuffer& audioBuf = inInputData->mBuffers[0];
        const int numChannels = static_cast<int>(audioBuf.mNumberChannels);
        const float* floatData = static_cast<const float*>(audioBuf.mData);

        if (floatData == nullptr || numChannels < 1)
            return noErr;

        const int totalFrames = static_cast<int>(audioBuf.mDataByteSize)
                              / static_cast<int>(sizeof(float) * numChannels);

        if (totalFrames <= 0)
            return noErr;

        // Write to ring buffer (always store as stereo interleaved)
        const auto scope = impl->fifo->write(totalFrames);

        // Write block 1
        for (int i = 0; i < scope.blockSize1; ++i)
        {
            int srcIdx = i;
            int dstIdx = (scope.startIndex1 + i) * 2;

            if (numChannels >= 2)
            {
                impl->ringBuffer[static_cast<size_t>(dstIdx)]     = floatData[srcIdx * numChannels];
                impl->ringBuffer[static_cast<size_t>(dstIdx + 1)] = floatData[srcIdx * numChannels + 1];
            }
            else
            {
                float sample = floatData[srcIdx];
                impl->ringBuffer[static_cast<size_t>(dstIdx)]     = sample;
                impl->ringBuffer[static_cast<size_t>(dstIdx + 1)] = sample;
            }
        }

        // Write block 2 (wrap-around portion)
        for (int i = 0; i < scope.blockSize2; ++i)
        {
            int srcIdx = scope.blockSize1 + i;
            int dstIdx = (scope.startIndex2 + i) * 2;

            if (numChannels >= 2)
            {
                impl->ringBuffer[static_cast<size_t>(dstIdx)]     = floatData[srcIdx * numChannels];
                impl->ringBuffer[static_cast<size_t>(dstIdx + 1)] = floatData[srcIdx * numChannels + 1];
            }
            else
            {
                float sample = floatData[srcIdx];
                impl->ringBuffer[static_cast<size_t>(dstIdx)]     = sample;
                impl->ringBuffer[static_cast<size_t>(dstIdx + 1)] = sample;
            }
        }

        // Zero output buffers (we are input-only, but must clear output to avoid noise)
        if (outOutputData != nullptr)
        {
            for (UInt32 b = 0; b < outOutputData->mNumberBuffers; ++b)
            {
                if (outOutputData->mBuffers[b].mData != nullptr)
                    memset(outOutputData->mBuffers[b].mData, 0, outOutputData->mBuffers[b].mDataByteSize);
            }
        }

        return noErr;
    }

    //==========================================================================
    // Start capture — creates tap + aggregate device + IOProc
    //==========================================================================
    void startCaptureAsync(double expectedSampleRate)
    {
        if (active.load(std::memory_order_relaxed))
            return;

        juce::Logger::outputDebugString("CoreAudioTap: startCaptureAsync called, sr=" + juce::String(expectedSampleRate));

        // Runtime version check
        if (@available(macOS 14.2, *))
        {
            // Good — API available
        }
        else
        {
            juce::Logger::outputDebugString("CoreAudioTap: requires macOS 14.2+");
            return;
        }

        // Step 1: Get our own process AudioObjectID (to exclude from tap)
        pid_t myPID = getpid();
        AudioObjectID myProcessObjectID = kAudioObjectUnknown;

        {
            AudioObjectPropertyAddress addr = {
                kAudioHardwarePropertyTranslatePIDToProcessObject,
                kAudioObjectPropertyScopeGlobal,
                kAudioObjectPropertyElementMain
            };
            UInt32 size = sizeof(AudioObjectID);
            OSStatus status = AudioObjectGetPropertyData(
                kAudioObjectSystemObject, &addr, sizeof(pid_t), &myPID,
                &size, &myProcessObjectID);

            if (status != noErr)
            {
                juce::Logger::outputDebugString("CoreAudioTap ERROR: TranslatePIDToProcessObject failed, OSStatus=" + juce::String((int)status));
                return;
            }
            juce::Logger::outputDebugString("CoreAudioTap: my PID=" + juce::String((int)myPID)
                + " processObjectID=" + juce::String((int)myProcessObjectID));
        }

        // Step 2: Create CATapDescription — stereo global tap, exclude self
        if (@available(macOS 14.2, *))
        {
            CATapDescription* desc = [[CATapDescription alloc]
                initStereoGlobalTapButExcludeProcesses:@[@(myProcessObjectID)]];

            desc.name = @"GOODMETER-SystemAudioTap";
            tapUUID = [NSUUID UUID];
            desc.UUID = tapUUID;
            [desc setPrivate:YES];
            desc.muteBehavior = CATapUnmuted;

            juce::Logger::outputDebugString("CoreAudioTap: CATapDescription created, UUID="
                + juce::String([tapUUID.UUIDString UTF8String]));

            // Step 3: Create the Process Tap
            AudioObjectID newTapID = kAudioObjectUnknown;
            OSStatus status = AudioHardwareCreateProcessTap(desc, &newTapID);

            if (status != noErr)
            {
                juce::Logger::outputDebugString("CoreAudioTap ERROR: AudioHardwareCreateProcessTap failed, OSStatus=" + juce::String((int)status));
                return;
            }

            tapObjectID = newTapID;
            juce::Logger::outputDebugString("CoreAudioTap: tap created, tapObjectID=" + juce::String((int)tapObjectID));

            // Step 4: Read tap format
            AudioStreamBasicDescription tapFormat = {};
            {
                AudioObjectPropertyAddress fmtAddr = {
                    'tfmt', // kAudioTapPropertyFormat
                    kAudioObjectPropertyScopeGlobal,
                    kAudioObjectPropertyElementMain
                };
                UInt32 fmtSize = sizeof(AudioStreamBasicDescription);
                OSStatus fmtStatus = AudioObjectGetPropertyData(tapObjectID, &fmtAddr, 0, nullptr, &fmtSize, &tapFormat);
                if (fmtStatus == noErr)
                {
                    sampleRate.store(tapFormat.mSampleRate, std::memory_order_relaxed);
                    juce::Logger::outputDebugString("CoreAudioTap: tap format sr=" + juce::String(tapFormat.mSampleRate)
                        + " ch=" + juce::String((int)tapFormat.mChannelsPerFrame)
                        + " bitsPerCh=" + juce::String((int)tapFormat.mBitsPerChannel));
                }
                else
                {
                    juce::Logger::outputDebugString("CoreAudioTap WARNING: could not read tap format, OSStatus=" + juce::String((int)fmtStatus));
                    sampleRate.store(expectedSampleRate, std::memory_order_relaxed);
                }
            }

            // Step 5: Create Aggregate Device with the tap as input
            NSString* tapUUIDStr = tapUUID.UUIDString;
            NSDictionary* aggDesc = @{
                @kAudioAggregateDeviceNameKey      : @"GOODMETER-TapDevice",
                @kAudioAggregateDeviceUIDKey       : @"com.solaris.goodmeter.tapdevice",
                @kAudioAggregateDeviceIsPrivateKey  : @YES,
                @kAudioAggregateDeviceTapListKey    : @[
                    @{
                        @kAudioSubTapUIDKey                : tapUUIDStr,
                        @kAudioSubTapDriftCompensationKey   : @YES,
                    }
                ],
                @kAudioAggregateDeviceTapAutoStartKey : @NO,
                @kAudioAggregateDeviceIsStackedKey    : @NO,
            };

            AudioObjectID newAggDeviceID = kAudioObjectUnknown;
            status = AudioHardwareCreateAggregateDevice(
                (__bridge CFDictionaryRef)aggDesc, &newAggDeviceID);

            if (status != noErr)
            {
                juce::Logger::outputDebugString("CoreAudioTap ERROR: AudioHardwareCreateAggregateDevice failed, OSStatus=" + juce::String((int)status));
                AudioHardwareDestroyProcessTap(tapObjectID);
                tapObjectID = kAudioObjectUnknown;
                return;
            }

            aggregateDeviceID = newAggDeviceID;
            juce::Logger::outputDebugString("CoreAudioTap: aggregate device created, deviceID=" + juce::String((int)aggregateDeviceID));

            // Step 6: Register IOProc callback
            AudioDeviceIOProcID newIOProcID = nullptr;
            status = AudioDeviceCreateIOProcID(aggregateDeviceID, ioProcCallback, this, &newIOProcID);

            if (status != noErr)
            {
                juce::Logger::outputDebugString("CoreAudioTap ERROR: AudioDeviceCreateIOProcID failed, OSStatus=" + juce::String((int)status));
                AudioHardwareDestroyAggregateDevice(aggregateDeviceID);
                aggregateDeviceID = kAudioObjectUnknown;
                AudioHardwareDestroyProcessTap(tapObjectID);
                tapObjectID = kAudioObjectUnknown;
                return;
            }

            ioProcID = newIOProcID;

            // Step 7: Start!
            status = AudioDeviceStart(aggregateDeviceID, ioProcID);

            if (status != noErr)
            {
                juce::Logger::outputDebugString("CoreAudioTap ERROR: AudioDeviceStart failed, OSStatus=" + juce::String((int)status));
                AudioDeviceDestroyIOProcID(aggregateDeviceID, ioProcID);
                ioProcID = nullptr;
                AudioHardwareDestroyAggregateDevice(aggregateDeviceID);
                aggregateDeviceID = kAudioObjectUnknown;
                AudioHardwareDestroyProcessTap(tapObjectID);
                tapObjectID = kAudioObjectUnknown;
                return;
            }

            active.store(true, std::memory_order_relaxed);
            juce::Logger::outputDebugString("CoreAudioTap: capture started successfully!");
        }
    }

    //==========================================================================
    // Stop capture — tear down in reverse order
    //==========================================================================
    void stopCapture()
    {
        active.store(false, std::memory_order_relaxed);

        if (ioProcID != nullptr && aggregateDeviceID != kAudioObjectUnknown)
        {
            OSStatus status = AudioDeviceStop(aggregateDeviceID, ioProcID);
            if (status != noErr)
                juce::Logger::outputDebugString("CoreAudioTap WARNING: AudioDeviceStop OSStatus=" + juce::String((int)status));

            status = AudioDeviceDestroyIOProcID(aggregateDeviceID, ioProcID);
            if (status != noErr)
                juce::Logger::outputDebugString("CoreAudioTap WARNING: AudioDeviceDestroyIOProcID OSStatus=" + juce::String((int)status));
            ioProcID = nullptr;
        }

        if (aggregateDeviceID != kAudioObjectUnknown)
        {
            OSStatus status = AudioHardwareDestroyAggregateDevice(aggregateDeviceID);
            if (status != noErr)
                juce::Logger::outputDebugString("CoreAudioTap WARNING: DestroyAggregateDevice OSStatus=" + juce::String((int)status));
            aggregateDeviceID = kAudioObjectUnknown;
        }

        if (tapObjectID != kAudioObjectUnknown)
        {
            if (@available(macOS 14.2, *))
            {
                OSStatus status = AudioHardwareDestroyProcessTap(tapObjectID);
                if (status != noErr)
                    juce::Logger::outputDebugString("CoreAudioTap WARNING: DestroyProcessTap OSStatus=" + juce::String((int)status));
            }
            tapObjectID = kAudioObjectUnknown;
        }

        tapUUID = nil;

        juce::Logger::outputDebugString("CoreAudioTap: capture stopped");
    }

    //==========================================================================
    // Read samples from ring buffer (called from processBlock)
    //==========================================================================
    int readSamples(float* destL, float* destR, int maxSamples)
    {
        const auto scope = fifo->read(juce::jmin(maxSamples, fifo->getNumReady()));

        // Read block 1
        for (int i = 0; i < scope.blockSize1; ++i)
        {
            int srcIdx = (scope.startIndex1 + i) * 2;
            destL[i] = ringBuffer[static_cast<size_t>(srcIdx)];
            if (destR) destR[i] = ringBuffer[static_cast<size_t>(srcIdx + 1)];
        }

        // Read block 2 (wrap-around)
        for (int i = 0; i < scope.blockSize2; ++i)
        {
            int srcIdx = (scope.startIndex2 + i) * 2;
            int dstIdx = scope.blockSize1 + i;
            destL[dstIdx] = ringBuffer[static_cast<size_t>(srcIdx)];
            if (destR) destR[dstIdx] = ringBuffer[static_cast<size_t>(srcIdx + 1)];
        }

        return scope.blockSize1 + scope.blockSize2;
    }
};

//==============================================================================
// Public API
//==============================================================================

SystemAudioCapture::SystemAudioCapture()
    : pImpl(std::make_unique<Impl>())
{
}

SystemAudioCapture::~SystemAudioCapture() = default;

void SystemAudioCapture::startAsync(double expectedSampleRate)
{
    pImpl->startCaptureAsync(expectedSampleRate);
}

void SystemAudioCapture::stop()
{
    pImpl->stopCapture();
}

bool SystemAudioCapture::isActive() const
{
    return pImpl->active.load(std::memory_order_relaxed);
}

int SystemAudioCapture::readSamples(float* destL, float* destR, int maxSamples)
{
    return pImpl->readSamples(destL, destR, maxSamples);
}

double SystemAudioCapture::getStreamSampleRate() const
{
    return pImpl->sampleRate.load(std::memory_order_relaxed);
}

#endif // JUCE_MAC && JucePlugin_Build_Standalone
