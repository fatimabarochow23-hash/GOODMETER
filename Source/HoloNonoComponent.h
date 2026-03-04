/*
  ==============================================================================
    HoloNonoComponent.h
    GOODMETER - Cyber-Alchemist Companion "NONO" - ULTIMATE INTERACTIVE FORM

    Interactive robot with offline audio analysis:
    - Double-click to flip (front <-> back)
    - Back face: "+" icon, click or drag audio file to analyze
    - Analysis: blue ripple animation, async LUFS/peak thread
    - Results: peak dBFS, momentary/short-term max, integrated LUFS
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "PluginProcessor.h"

//==============================================================================
struct NonoAnalysisResult
{
    float peakDBFS          = -100.0f;
    float momentaryMaxLUFS  = -100.0f;
    float shortTermMaxLUFS  = -100.0f;
    float integratedLUFS    = -100.0f;
};

//==============================================================================
class HoloNonoComponent : public juce::Component,
                           public juce::Timer,
                           public juce::FileDragAndDropTarget
{
public:
    //==========================================================================
    HoloNonoComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        setSize(100, 200);
        startTimerHz(60);

        auto& rng = juce::Random::getSystemRandom();
        for (auto& b : tubeBubbles)
        {
            b.xOff = rng.nextFloat() * 0.6f - 0.3f;
            b.phase = rng.nextFloat();
            b.sz = 1.5f + rng.nextFloat() * 2.5f;
            b.spd = 0.008f + rng.nextFloat() * 0.012f;
        }
    }

    ~HoloNonoComponent() override
    {
        stopTimer();
        if (analysisThread)
        {
            analysisThread->stopThread(3000);
            analysisThread.reset();
        }
    }

    //==========================================================================
    // Card fold/unfold callbacks (only active in Front state)
    //==========================================================================

    // Callback: double-click test tube → enter jiggle/edit mode
    std::function<void()> onTestTubeDoubleClicked;

    // Callback: double-click NONO body while in edit mode → exit jiggle mode
    std::function<void()> onExitJiggleMode;

    // Callback: right-double-click → toggle Mini Mode
    std::function<void()> onRightDoubleClick;

    // Edit mode flag (set by PluginEditor)
    bool isEditMode = false;

    // Wink state (set by PluginEditor when swap is ready)
    bool isWinking = false;
    void setWinking(bool w) { isWinking = w; repaint(); }

    void onCardFolded(const juce::String& cardName)
    {
        if (nonoState != NonoState::Front) return;
        if (cardName == "SPECTROGRAM")
        {
            triggerCollision(CollisionDir::Up);
            return;
        }
        if (cardName == "STEREO")
        {
            triggerCollision(CollisionDir::Left);
            return;
        }
        triggerFrontAnim(FrontAnim::FlashEyes);
    }

    void onCardUnfolded(const juce::String& /*cardName*/)
    {
        if (nonoState != NonoState::Front) return;
        triggerFrontAnim(FrontAnim::FlashEyes);
    }

    //==========================================================================
    // Mouse: double-click body = flip/clear, double-click tube = edit mode
    //==========================================================================
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        // Right-double-click → toggle Mini Mode
        if (e.mods.isRightButtonDown())
        {
            if (onRightDoubleClick != nullptr)
                onRightDoubleClick();
            return;
        }

        auto pos = e.position;

        // Test tube region → enter edit mode (only when NOT in edit mode)
        if (tubeHitRect.contains(pos))
        {
            if (!isEditMode && onTestTubeDoubleClicked)
                onTestTubeDoubleClicked();
            return;
        }

        // Body region
        if (!bodyHitRect.contains(pos))
            return;

        // If in edit mode, body double-click = EXIT edit mode
        if (isEditMode)
        {
            if (onExitJiggleMode)
                onExitJiggleMode();
            return;
        }

        // Normal mode: flip/clear logic
        switch (nonoState)
        {
            case NonoState::Front:
                flipTarget = 1.0f;
                nonoState = NonoState::Flipping;
                flipDestination = NonoState::Back;
                break;

            case NonoState::Back:
                flipTarget = 0.0f;
                nonoState = NonoState::Flipping;
                flipDestination = NonoState::Front;
                break;

            case NonoState::ShowingResults:
                nonoState = NonoState::ClearingData;
                targetPourAngle = juce::degreesToRadians(120.0f);
                bubbleFadeAlpha = 1.0f;
                break;

            default: break;
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (nonoState == NonoState::Back && bodyHitRect.contains(e.position))
            openFileChooser();
    }

    //==========================================================================
    // Drag & Drop
    //==========================================================================
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (const auto& f : files)
        {
            auto ext = juce::File(f).getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" ||
                ext == ".aif" || ext == ".flac" || ext == ".ogg")
                return true;
        }
        return false;
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        isDragHovering = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        isDragHovering = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        isDragHovering = false;

        for (const auto& f : files)
        {
            juce::File file(f);
            auto ext = file.getFileExtension().toLowerCase();
            if (ext == ".wav" || ext == ".mp3" || ext == ".aiff" ||
                ext == ".aif" || ext == ".flac" || ext == ".ogg")
            {
                // If on front, flip to back first then analyze
                if (nonoState == NonoState::Front || nonoState == NonoState::ShowingResults)
                {
                    pendingAnalysisFile = file;
                    flipTarget = 1.0f;
                    nonoState = NonoState::Flipping;
                    flipDestination = NonoState::Analyzing;
                }
                else
                {
                    startAnalysis(file);
                }
                return;
            }
        }
    }

    //==========================================================================
    // Paint
    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.isEmpty() || bounds.getHeight() < 40.0f)
            return;

        // ===== 动态空间绝对切割 =====
        bool showBubble = ((nonoState == NonoState::ShowingResults
                            || nonoState == NonoState::ClearingData) && hasResults);
        juce::Rectangle<float> bubbleArea;
        auto nonoDrawArea = bounds;

        if (showBubble)
            bubbleArea = nonoDrawArea.removeFromBottom(nonoDrawArea.getHeight() * 0.38f);

        const float unit = juce::jmin(nonoDrawArea.getWidth(), nonoDrawArea.getHeight());
        const float radius = unit * 0.18f;

        const float cx = nonoDrawArea.getCentreX() - radius * 0.6f + idleOffsetX + collisionOffsetX;
        const float cy = nonoDrawArea.getCentreY() - radius * 0.3f + idleOffsetY + collisionOffsetY;

        // Update hit test regions
        bodyHitRect = { cx - radius, cy - radius, radius * 2.0f, radius * 2.0f };
        float htX = cx + radius * 1.7f;
        float htY = cy - radius * 0.1f;
        tubeHitRect = { htX - radius * 0.3f, htY - radius * 0.6f, radius * 0.6f, radius * 1.2f };

        // Compute flip visual scale
        float hScale = 1.0f;
        bool showFront = true;

        if (nonoState == NonoState::Flipping)
        {
            if (flipProgress < 0.5f)
            {
                hScale = 1.0f - flipProgress * 2.0f;
                showFront = (flipDestination == NonoState::Back || flipDestination == NonoState::Analyzing);
            }
            else
            {
                hScale = (flipProgress - 0.5f) * 2.0f;
                showFront = !(flipDestination == NonoState::Back || flipDestination == NonoState::Analyzing);
            }
        }
        else
        {
            showFront = (nonoState == NonoState::Front || nonoState == NonoState::ShowingResults
                         || nonoState == NonoState::ClearingData);
            hScale = 1.0f;
        }

        hScale = juce::jmax(0.02f, hScale);

        // Draw layers
        drawAntiGravityGlow(g, cx, cy, radius);
        drawShadow(g, cx, cy, radius);
        drawHolographicEars(g, cx, cy, radius, hScale);
        drawBody(g, cx, cy, radius, hScale);

        if (showFront)
        {
            drawVisor(g, cx, cy, radius, hScale);
            drawEyes(g, cx, cy, radius, hScale);
        }
        else
        {
            drawBackFace(g, cx, cy, radius, hScale);
        }

        // Drag hover highlight
        if (isDragHovering)
        {
            g.setColour(electricBlue.withAlpha(0.25f));
            g.drawEllipse(cx - radius * hScale - 4.0f, cy - radius - 4.0f,
                          radius * hScale * 2.0f + 8.0f, radius * 2.0f + 8.0f, 3.0f);
        }

        // Analysis ripples
        if (nonoState == NonoState::Analyzing)
            drawAnalysisRipples(g, cx, cy, radius);

        // Floating test tube (always visible)
        drawFloatingTestTube(g, cx, cy, radius);

        // Results bubble in dedicated bottom area
        if (showBubble)
            drawResultsBubble(g, bubbleArea, cx);

        drawParticles(g, cx, cy, radius);
    }

    void resized() override {}

private:
    //==========================================================================
    // Inner: Biquad filter for K-weighting
    //==========================================================================
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;

        double process(double x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void reset() { z1 = z2 = 0; }
    };

    static void computeKWeighting(double sampleRate, Biquad& stage1, Biquad& stage2)
    {
        // Stage 1: High shelf pre-filter (ITU-R BS.1770-4)
        {
            const double Vh = 1.58489319246111; // 10^(4/20)
            const double K = std::tan(juce::MathConstants<double>::pi * 1681.974450955533 / sampleRate);
            const double Ksq = K * K;
            const double denom = 1.0 + std::sqrt(2.0) * K + Ksq;
            stage1.b0 = (Vh + std::sqrt(2.0 * Vh) * K + Ksq) / denom;
            stage1.b1 = 2.0 * (Ksq - Vh) / denom;
            stage1.b2 = (Vh - std::sqrt(2.0 * Vh) * K + Ksq) / denom;
            stage1.a1 = 2.0 * (Ksq - 1.0) / denom;
            stage1.a2 = (1.0 - std::sqrt(2.0) * K + Ksq) / denom;
        }
        // Stage 2: High-pass RLB weighting
        {
            const double Q = 0.5003270373238773;
            const double K = std::tan(juce::MathConstants<double>::pi * 38.13547087602444 / sampleRate);
            const double Ksq = K * K;
            const double denom = 1.0 + K / Q + Ksq;
            stage2.b0 = 1.0 / denom;
            stage2.b1 = -2.0 / denom;
            stage2.b2 = 1.0 / denom;
            stage2.a1 = 2.0 * (Ksq - 1.0) / denom;
            stage2.a2 = (1.0 - K / Q + Ksq) / denom;
        }
    }

    //==========================================================================
    // Inner: Analysis Thread (async, EBU R128 compliant)
    //==========================================================================
    class AnalysisThread : public juce::Thread
    {
    public:
        AnalysisThread(const juce::File& file,
                       juce::Component::SafePointer<HoloNonoComponent> owner)
            : Thread("NONO-Analysis"), audioFile(file), safeOwner(owner) {}

        void run() override
        {
            juce::AudioFormatManager fmt;
            fmt.registerBasicFormats();

            std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(audioFile));
            if (!reader)
            {
                callbackResult(NonoAnalysisResult{});
                return;
            }

            const int numCh = juce::jmin((int)reader->numChannels, 2);
            const double sr = reader->sampleRate;
            const juce::int64 maxSamples = (juce::int64)(sr * 600.0); // 10 min cap
            const juce::int64 totalSamples = juce::jmin(reader->lengthInSamples, maxSamples);

            if (totalSamples <= 0 || sr <= 0)
            {
                callbackResult(NonoAnalysisResult{});
                return;
            }

            // ---- K-weighting filters ----
            Biquad s1[2], s2[2];
            computeKWeighting(sr, s1[0], s2[0]);
            if (numCh > 1) computeKWeighting(sr, s1[1], s2[1]);

            // ---- 100ms sub-block accumulators ----
            const int subBlockSize = (int)(sr * 0.1);
            if (subBlockSize <= 0) { callbackResult(NonoAnalysisResult{}); return; }

            std::vector<double> subMS;              // mean-square per 100ms sub-block
            double subBlockPower[2] = { 0.0, 0.0 }; // running sum per channel
            int subBlockPos = 0;                     // sample position within current sub-block

            NonoAnalysisResult result;
            float globalMaxMag = 0.0f;

            // ==== CHUNKED READ LOOP (never loads entire file) ====
            constexpr int blockSize = 65536;
            juce::AudioBuffer<float> buffer(numCh, blockSize);
            juce::int64 samplesRead = 0;

            while (samplesRead < totalSamples)
            {
                if (threadShouldExit()) return;

                const int toRead = (int)juce::jmin((juce::int64)blockSize, totalSamples - samplesRead);
                buffer.clear();
                reader->read(&buffer, 0, toRead, samplesRead, true, numCh > 1);
                samplesRead += toRead;

                // ---- True Peak (before K-weighting) ----
                for (int ch = 0; ch < numCh; ++ch)
                    globalMaxMag = juce::jmax(globalMaxMag,
                                              buffer.getMagnitude(ch, 0, toRead));

                // ---- K-weight in-place + accumulate 100ms sub-blocks ----
                for (int i = 0; i < toRead; ++i)
                {
                    for (int ch = 0; ch < numCh; ++ch)
                    {
                        double x = (double)buffer.getSample(ch, i);
                        x = s1[ch].process(x);
                        x = s2[ch].process(x);
                        subBlockPower[ch] += x * x;
                    }

                    ++subBlockPos;
                    if (subBlockPos >= subBlockSize)
                    {
                        double ms = 0.0;
                        for (int ch = 0; ch < numCh; ++ch)
                        {
                            ms += subBlockPower[ch] / subBlockSize;
                            subBlockPower[ch] = 0.0;
                        }
                        subMS.push_back(ms);
                        subBlockPos = 0;
                    }
                }
            }

            reader.reset(); // release file handle immediately

            // ---- Peak dBFS ----
            result.peakDBFS = juce::Decibels::gainToDecibels(globalMaxMag, -100.0f);

            if (threadShouldExit()) return;

            // ---- LUFS calculations from sub-block data ----
            const int numSubBlocks = (int)subMS.size();

            auto toLUFS = [](double power) -> float {
                return (power > 0.0) ? (float)(-0.691 + 10.0 * std::log10(power)) : -100.0f;
            };

            // Momentary max (400ms = 4 sub-blocks)
            {
                const int win = 4;
                float maxVal = -100.0f;
                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    maxVal = juce::jmax(maxVal, toLUFS(sum / win));
                }
                result.momentaryMaxLUFS = maxVal;
            }

            // Short-term max (3000ms = 30 sub-blocks)
            {
                const int win = 30;
                float maxVal = -100.0f;
                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    maxVal = juce::jmax(maxVal, toLUFS(sum / win));
                }
                result.shortTermMaxLUFS = maxVal;
            }

            // Integrated LUFS (EBU R128 dual gating)
            {
                const int win = 4;
                std::vector<double> winPower;
                std::vector<float> winLUFS;

                for (int i = 0; i <= numSubBlocks - win; ++i)
                {
                    double sum = 0;
                    for (int j = 0; j < win; ++j) sum += subMS[i + j];
                    double pwr = sum / win;
                    winPower.push_back(pwr);
                    winLUFS.push_back(toLUFS(pwr));
                }

                // Absolute gate (-70 LUFS)
                double absSum = 0;
                int absCnt = 0;
                for (size_t i = 0; i < winPower.size(); ++i)
                {
                    if (winLUFS[i] > -70.0f)
                    {
                        absSum += winPower[i];
                        absCnt++;
                    }
                }

                if (absCnt > 0)
                {
                    float relThresh = toLUFS(absSum / absCnt) - 10.0f;

                    double relSum = 0;
                    int relCnt = 0;
                    for (size_t i = 0; i < winPower.size(); ++i)
                    {
                        if (winLUFS[i] > -70.0f && winLUFS[i] > relThresh)
                        {
                            relSum += winPower[i];
                            relCnt++;
                        }
                    }
                    result.integratedLUFS = (relCnt > 0) ? toLUFS(relSum / relCnt) : -100.0f;
                }
                else
                {
                    result.integratedLUFS = -100.0f;
                }
            }

            callbackResult(result);
        }

    private:
        juce::File audioFile;
        juce::Component::SafePointer<HoloNonoComponent> safeOwner;

        void callbackResult(const NonoAnalysisResult& r)
        {
            auto safe = safeOwner;
            auto result = r;
            juce::MessageManager::callAsync([safe, result]()
            {
                if (auto* comp = safe.getComponent())
                    comp->onAnalysisComplete(result);
            });
        }
    };

    //==========================================================================
    // Enums & State
    //==========================================================================
    GOODMETERAudioProcessor& audioProcessor;

    enum class NonoState { Front, Flipping, Back, Analyzing, ShowingResults, ClearingData };
    NonoState nonoState = NonoState::Front;
    NonoState flipDestination = NonoState::Back;

    enum class FrontAnim { Idle, FlashEyes, CollisionHit };
    FrontAnim frontAnim = FrontAnim::Idle;
    int frontAnimTimer = 0;

    enum class CollisionDir { None, Up, Left };
    CollisionDir collisionDir = CollisionDir::None;

    // Flip animation
    float flipProgress = 0.0f;  // 0 = front, 1 = back
    float flipTarget = 0.0f;

    // Idle figure-8
    float idlePhase = 0.0f;
    float idleOffsetX = 0.0f;
    float idleOffsetY = 0.0f;

    // Eye animation
    float eyeOpenness = 0.4f;
    float targetEyeOpenness = 0.4f;
    float eyeGlow = 0.7f;
    float targetEyeGlow = 0.7f;

    // Test tube
    float tubeAngle = 0.0f;
    float tubeLiquidWave = 0.0f;
    float audioLevel = 0.0f;
    float liquidHeight = 1.0f;       // 0 = empty, 1 = full
    float clearPourAngle = 0.0f;     // current pour rotation (radians)
    float targetPourAngle = 0.0f;    // target pour rotation
    float bubbleFadeAlpha = 1.0f;    // results bubble fade
    float neonBreathPhase = 0.0f;    // back-face neon breathing
    float earBobOffset = 0.0f;       // ear floating Y offset

    // Auto-refill state machine (3s cooldown → smooth fill)
    juce::uint32 emptyTimestamp = 0;
    bool isWaitingToRefill = false;
    bool isRefilling = false;

    // Overload explosion state (peak >= 0 dBFS)
    bool isExploded = false;
    float explosionProgress = 0.0f;   // 0→1 animation progress
    struct ExpShard {
        float x, y;          // offset from tube center
        float angle;         // rotation
        float vx, vy;        // velocity
        float spin;          // angular velocity
        float sz;            // size
    };
    ExpShard shards[6];
    struct ExpDrop {
        float x, y;
        float vx, vy;
        float sz;
        float gravity;
    };
    ExpDrop drops[10];

    // Collision
    float collisionOffsetX = 0.0f;
    float collisionOffsetY = 0.0f;
    float targetCollisionX = 0.0f;
    float targetCollisionY = 0.0f;

    // Particles
    bool showParticles = false;
    float particleProgress = 0.0f;
    float particleOriginX = 0.0f;
    float particleOriginY = 0.0f;

    // Analysis state
    std::unique_ptr<AnalysisThread> analysisThread;
    NonoAnalysisResult analysisResult;
    bool hasResults = false;
    float ripplePhase = 0.0f;
    bool isDragHovering = false;
    juce::File pendingAnalysisFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Hit test regions (computed in paint, used in mouse handlers)
    juce::Rectangle<float> bodyHitRect;
    juce::Rectangle<float> tubeHitRect;

    // Edit mode transition (0=normal, 1=editMode → color/bubble change)
    float editModeTransition = 0.0f;

    // Test tube bubbles
    struct TubeBubble
    {
        float xOff = 0.0f;   // horizontal offset within tube (-0.3..0.3)
        float phase = 0.0f;  // vertical phase (0..1, wraps)
        float sz = 2.0f;     // bubble radius
        float spd = 0.01f;   // rise speed per frame
    };
    TubeBubble tubeBubbles[8];

    // Colors
    static inline const juce::Colour electricBlue = juce::Colour(0xFF00AAFF);
    static inline const juce::Colour bodyEdge     = juce::Colour(0xFFD0D0D8);
    static inline const juce::Colour screenDark   = juce::Colour(0xFF0E0E1E);
    static inline const juce::Colour magicPink    = juce::Colour(0xFFFF2A7F);
    static inline const juce::Colour neonGreen    = juce::Colour(0xFF39FF14);

    //==========================================================================
    // Timer
    //==========================================================================
    void timerCallback() override
    {
        // Audio level
        float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
        float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
        float peak = juce::jmax(peakL, peakR);
        float rawLevel = juce::jmap(juce::jlimit(-60.0f, 0.0f, peak), -60.0f, 0.0f, 0.0f, 1.0f);
        audioLevel += (rawLevel - audioLevel) * 0.2f;

        targetEyeOpenness = 0.3f + audioLevel * 0.7f;

        // Idle figure-8
        idlePhase += 0.015f;
        if (idlePhase > 628.0f) idlePhase -= 628.0f;
        idleOffsetX = 1.5f * std::sin(2.0f * idlePhase);
        idleOffsetY = 2.5f * std::sin(idlePhase);

        // Front face animations
        advanceFrontAnim();

        // Smooth lerps
        eyeOpenness += (targetEyeOpenness - eyeOpenness) * 0.15f;
        eyeGlow += (targetEyeGlow - eyeGlow) * 0.2f;

        float collLerp = (frontAnim == FrontAnim::CollisionHit) ? 0.4f : 0.25f;
        collisionOffsetX += (targetCollisionX - collisionOffsetX) * collLerp;
        collisionOffsetY += (targetCollisionY - collisionOffsetY) * collLerp;

        // Flip animation
        if (std::abs(flipTarget - flipProgress) > 0.01f)
        {
            flipProgress += (flipTarget - flipProgress) * 0.18f;
        }
        else if (nonoState == NonoState::Flipping)
        {
            flipProgress = flipTarget;

            if (flipDestination == NonoState::Analyzing)
            {
                nonoState = NonoState::Analyzing;
                if (pendingAnalysisFile.existsAsFile())
                    startAnalysis(pendingAnalysisFile);
            }
            else
            {
                nonoState = flipDestination;
            }
        }

        // Tube shake (dual-sine)
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        if (audioLevel > 0.03f)
        {
            float intensity = juce::jlimit(0.0f, 1.0f, audioLevel * 1.8f);
            float maxAngle = juce::degreesToRadians(18.0f);
            float shake = (std::sin(ms * 0.015f) * 0.7f + std::sin(ms * 0.004f) * 0.3f);
            tubeAngle += (intensity * maxAngle * shake - tubeAngle) * 0.25f;
            tubeLiquidWave = intensity * 3.0f *
                             (std::sin(ms * 0.012f) * 0.6f + std::sin(ms * 0.005f) * 0.4f);
        }
        else
        {
            tubeAngle *= 0.92f;
            tubeLiquidWave *= 0.92f;
        }

        // Ripple phase for analysis animation
        if (nonoState == NonoState::Analyzing)
            ripplePhase += 0.02f;

        // Neon breathing phase (always ticks for back face)
        neonBreathPhase += 0.03f;
        if (neonBreathPhase > juce::MathConstants<float>::twoPi * 100.0f)
            neonBreathPhase -= juce::MathConstants<float>::twoPi * 100.0f;

        // Ear independent bobbing (low-freq sine)
        earBobOffset = std::sin(ms * 0.003f) * 3.0f;

        // Edit mode transition (smooth 0→1 / 1→0)
        {
            float editTarget = isEditMode ? 1.0f : 0.0f;
            editModeTransition += (editTarget - editModeTransition) * 0.08f;
            if (std::abs(editModeTransition - editTarget) < 0.005f)
                editModeTransition = editTarget;
        }

        // Bubble phase cycling (always tick, visible when editModeTransition > 0)
        if (editModeTransition > 0.01f)
        {
            for (auto& b : tubeBubbles)
            {
                b.phase += b.spd * (0.5f + editModeTransition * 0.5f);
                if (b.phase > 1.0f)
                    b.phase -= 1.0f;
            }
        }

        // ClearingData animation: fade bubble → pour tube → drain liquid → restore → Front
        if (nonoState == NonoState::ClearingData)
        {
            // Phase 1: fade out bubble
            bubbleFadeAlpha = juce::jmax(0.0f, bubbleFadeAlpha - 0.08f);

            // Phase 2: tilt tube to pour position
            clearPourAngle += (targetPourAngle - clearPourAngle) * 0.15f;

            // Phase 3: drain liquid once tilted past 60°
            if (clearPourAngle > juce::degreesToRadians(60.0f))
                liquidHeight = juce::jmax(0.0f, liquidHeight - 0.04f);

            // Phase 4: once empty, restore tube upright → go to Front
            if (liquidHeight <= 0.01f)
            {
                targetPourAngle = 0.0f;
                clearPourAngle += (0.0f - clearPourAngle) * 0.12f;

                if (std::abs(clearPourAngle) < 0.02f)
                {
                    clearPourAngle = 0.0f;
                    liquidHeight = 0.0f;
                    hasResults = false;
                    nonoState = NonoState::Front;
                    flipProgress = 0.0f;
                    flipTarget = 0.0f;

                    // Trigger 3-second refill cooldown
                    emptyTimestamp = juce::Time::getMillisecondCounter();
                    isWaitingToRefill = true;
                    isRefilling = false;
                }
            }
        }

        // Auto-refill state machine: 3s cooldown → smooth fill
        if (isWaitingToRefill)
        {
            if (juce::Time::getMillisecondCounter() - emptyTimestamp >= 3000)
            {
                isWaitingToRefill = false;
                isExploded = false;     // 碎玻璃瞬间复原！
                isRefilling = true;
                liquidHeight = 0.0f;    // 确保从空开始注水
            }
        }

        if (isRefilling)
        {
            liquidHeight += 0.04f;
            if (liquidHeight >= 1.0f)
            {
                liquidHeight = 1.0f;
                isRefilling = false;
            }
        }

        // Explosion physics: animate shards + drops
        if (isExploded && explosionProgress < 1.0f)
        {
            explosionProgress += 0.012f; // ~1.2s to full fade at 60Hz
            for (auto& s : shards)
            {
                s.x += s.vx;
                s.y += s.vy;
                s.vy += 0.18f; // gravity
                s.vx *= 0.985f; // air drag
                s.angle += s.spin;
            }
            for (auto& d : drops)
            {
                d.x += d.vx;
                d.y += d.vy;
                d.vy += d.gravity; // per-drop gravity
                d.vx *= 0.98f;
            }
        }

        // Particles
        if (showParticles)
        {
            particleProgress += 0.06f;
            if (particleProgress >= 1.0f)
                showParticles = false;
        }

        repaint();
    }

    //==========================================================================
    // Front-face animation state machine
    //==========================================================================
    void advanceFrontAnim()
    {
        if (frontAnim == FrontAnim::Idle) return;
        frontAnimTimer++;

        switch (frontAnim)
        {
            case FrontAnim::FlashEyes:
            {
                if (frontAnimTimer <= 7)
                    targetEyeGlow = 1.0f;
                else if (frontAnimTimer <= 19)
                    targetEyeGlow = 1.0f - (static_cast<float>(frontAnimTimer - 7) / 12.0f) * 0.3f;
                else
                {
                    targetEyeGlow = 0.7f;
                    frontAnim = FrontAnim::Idle;
                }
                break;
            }
            case FrontAnim::CollisionHit:
            {
                const float maxDash = 45.0f;
                if (frontAnimTimer <= 3)
                {
                    float t = static_cast<float>(frontAnimTimer) / 3.0f;
                    if (collisionDir == CollisionDir::Up) targetCollisionY = -maxDash * t;
                    else targetCollisionX = -maxDash * t;
                }
                else if (frontAnimTimer <= 5)
                {
                    if (frontAnimTimer == 4)
                    {
                        showParticles = true;
                        particleProgress = 0.0f;
                        auto b = getLocalBounds().toFloat();
                        if (collisionDir == CollisionDir::Up)
                            { particleOriginX = b.getCentreX(); particleOriginY = b.getCentreY() + collisionOffsetY; }
                        else
                            { particleOriginX = b.getCentreX() + collisionOffsetX; particleOriginY = b.getCentreY(); }
                    }
                }
                else if (frontAnimTimer <= 11)
                {
                    targetCollisionX = targetCollisionY = 0.0f;
                }
                else
                {
                    targetCollisionX = targetCollisionY = 0.0f;
                    if (!showParticles) frontAnim = FrontAnim::Idle;
                }
                break;
            }
            default: break;
        }
    }

    void triggerFrontAnim(FrontAnim a)
    {
        if (frontAnim == FrontAnim::CollisionHit) return;
        frontAnim = a;
        frontAnimTimer = 0;
    }

    void triggerCollision(CollisionDir dir)
    {
        frontAnim = FrontAnim::CollisionHit;
        collisionDir = dir;
        frontAnimTimer = 0;
        targetCollisionX = targetCollisionY = 0.0f;
    }

    //==========================================================================
    // Analysis control
    //==========================================================================
    void openFileChooser()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Audio File", juce::File{},
            "*.wav;*.mp3;*.aiff;*.aif;*.flac;*.ogg");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto result = fc.getResult();
                if (result.existsAsFile())
                    startAnalysis(result);
            });
    }

    void startAnalysis(const juce::File& file)
    {
        if (analysisThread)
        {
            analysisThread->stopThread(2000);
            analysisThread.reset();
        }

        nonoState = NonoState::Analyzing;
        flipTarget = 1.0f;
        flipProgress = 1.0f;
        hasResults = false;
        ripplePhase = 0.0f;
        liquidHeight = 1.0f;       // refill tube
        bubbleFadeAlpha = 1.0f;    // reset bubble opacity

        auto safeThis = juce::Component::SafePointer<HoloNonoComponent>(this);
        analysisThread = std::make_unique<AnalysisThread>(file, safeThis);
        analysisThread->startThread();
    }

    void onAnalysisComplete(const NonoAnalysisResult& result)
    {
        analysisResult = result;
        hasResults = true;

        // Overload detection: peak >= 0 dBFS → EXPLODE!
        if (result.peakDBFS >= 0.0f)
        {
            isExploded = true;
            explosionProgress = 0.0f;
            liquidHeight = 0.0f;

            // Initialize shards with random velocities
            auto& rng = juce::Random::getSystemRandom();
            for (auto& s : shards)
            {
                s.x = 0.0f;
                s.y = 0.0f;
                s.angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
                s.vx = (rng.nextFloat() - 0.5f) * 8.0f;
                s.vy = -rng.nextFloat() * 6.0f - 2.0f;
                s.spin = (rng.nextFloat() - 0.5f) * 0.4f;
                s.sz = 4.0f + rng.nextFloat() * 8.0f;
            }
            for (auto& d : drops)
            {
                d.x = 0.0f;
                d.y = 0.0f;
                d.vx = (rng.nextFloat() - 0.5f) * 10.0f;
                d.vy = -rng.nextFloat() * 8.0f - 3.0f;
                d.sz = 2.0f + rng.nextFloat() * 6.0f;
                d.gravity = 0.25f + rng.nextFloat() * 0.15f;
            }

            // 3-second cooldown → auto-restore
            emptyTimestamp = juce::Time::getMillisecondCounter();
            isWaitingToRefill = true;
            isRefilling = false;
        }

        // Flip back to front, show results
        flipTarget = 0.0f;
        nonoState = NonoState::Flipping;
        flipDestination = NonoState::ShowingResults;
    }

    //==========================================================================
    // Drawing: Common
    //==========================================================================
    void drawAntiGravityGlow(juce::Graphics& g, float cx, float cy, float r)
    {
        const float glowY = cy + r + r * 0.08f;
        juce::ColourGradient grad(
            electricBlue.withAlpha(0.18f), cx, glowY,
            electricBlue.withAlpha(0.0f), cx, glowY + r * 0.7f, false);
        g.setGradientFill(grad);
        g.fillEllipse(cx - r * 0.8f, glowY, r * 1.6f, r * 0.65f);
    }

    void drawShadow(juce::Graphics& g, float cx, float cy, float r)
    {
        g.setColour(juce::Colour(0x18000000));
        g.fillEllipse(cx - r * 0.8f, cy + r + r * 0.15f, r * 1.6f, r * 0.24f);
    }

    void drawBody(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        float rx = r * hScale;

        // Main body: radial gradient — light source upper-left, shadow lower-right
        juce::ColourGradient bodyGrad(
            juce::Colour(0xFFFFFFFF), cx - rx * 0.35f, cy - r * 0.4f,
            juce::Colour(0xFFC0C4CE), cx + rx * 0.55f, cy + r * 0.6f, true);
        bodyGrad.addColour(0.45, juce::Colour(0xFFF0F1F5));
        bodyGrad.addColour(0.8,  juce::Colour(0xFFD5D8E0));
        g.setGradientFill(bodyGrad);
        g.fillEllipse(cx - rx, cy - r, rx * 2.0f, r * 2.0f);

        // Rim stroke — subtle cool-toned edge
        g.setColour(juce::Colour(0xFFB8BCC8));
        g.drawEllipse(cx - rx, cy - r, rx * 2.0f, r * 2.0f, 1.5f);

        // Specular highlight — crisp white spot upper-left
        if (hScale > 0.5f)
        {
            juce::ColourGradient specGrad(
                juce::Colours::white.withAlpha(0.65f), cx - rx * 0.3f, cy - r * 0.4f,
                juce::Colours::white.withAlpha(0.0f),  cx + rx * 0.05f, cy - r * 0.05f, true);
            g.setGradientFill(specGrad);
            g.fillEllipse(cx - rx * 0.6f, cy - r * 0.7f, rx * 0.7f, r * 0.5f);
        }

        // Rim light — faint cool reflection on lower-right edge
        if (hScale > 0.5f)
        {
            juce::ColourGradient rimGrad(
                juce::Colours::white.withAlpha(0.0f), cx, cy,
                juce::Colour(0xFFDDE0EA).withAlpha(0.35f), cx + rx * 0.7f, cy + r * 0.6f, true);
            g.setGradientFill(rimGrad);
            g.fillEllipse(cx + rx * 0.15f, cy + r * 0.2f, rx * 0.7f, r * 0.55f);
        }
    }

    //==========================================================================
    // Drawing: Holographic Ears (Seer-style solid base + energy blade)
    //==========================================================================
    void drawHolographicEars(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        const juce::Colour baseCol(0xFFE0E5EC);
        const juce::Colour bladeCol(0xFF00E5FF);
        const float tiltAngle = juce::degreesToRadians(15.0f);
        const float sphereTop = cy - r;

        // 正脸对称: 左右耳等大等高
        struct EarSpec { float xOff, yOff, scale, bob; };
        EarSpec specs[2] = {
            { -0.72f, -0.18f, 0.95f, earBobOffset },    // left ear
            {  0.72f, -0.18f, 0.95f, earBobOffset }     // right ear
        };

        for (int i = 0; i < 2; ++i)
        {
            int side = (i == 0) ? -1 : 1;
            float sc = specs[i].scale;

            float baseW = r * 0.22f * hScale * sc;
            float baseH = r * 0.32f * sc;
            float bladeW = r * 0.13f * hScale * sc;
            float bladeH = r * 0.6f * sc;

            float anchorX = cx + r * specs[i].xOff * hScale;
            float anchorY = sphereTop + r * specs[i].yOff + specs[i].bob;
            float angle = tiltAngle * static_cast<float>(side);

            auto xform = juce::AffineTransform::rotation(angle, anchorX, anchorY);

            // --- Solid base (capsule) ---
            juce::Path basePath;
            basePath.addRoundedRectangle(
                anchorX - baseW / 2.0f, anchorY - baseH / 2.0f,
                baseW, baseH, baseW * 0.45f);
            basePath.applyTransform(xform);

            g.setColour(baseCol);
            g.fillPath(basePath);
            g.setColour(bodyEdge);
            g.strokePath(basePath, juce::PathStrokeType(1.5f));

            // --- Holographic blade ---
            float bladeBot = anchorY - baseH * 0.3f;
            float bladeTop = bladeBot - bladeH;
            float bw = bladeW;

            juce::Path bladePath;
            bladePath.startNewSubPath(anchorX - bw / 2.0f, bladeBot);
            bladePath.cubicTo(
                anchorX - bw * 0.55f, bladeBot - bladeH * 0.4f,
                anchorX - bw * 0.2f,  bladeBot - bladeH * 0.75f,
                anchorX,              bladeTop);
            bladePath.cubicTo(
                anchorX + bw * 0.2f,  bladeBot - bladeH * 0.75f,
                anchorX + bw * 0.55f, bladeBot - bladeH * 0.4f,
                anchorX + bw / 2.0f,  bladeBot);
            bladePath.closeSubPath();
            bladePath.applyTransform(xform);

            // Glow layer 1
            g.setColour(bladeCol.withAlpha(0.1f));
            g.strokePath(bladePath, juce::PathStrokeType(8.0f * sc));
            // Glow layer 2
            g.setColour(bladeCol.withAlpha(0.25f));
            g.strokePath(bladePath, juce::PathStrokeType(4.0f * sc));

            // Core gradient fill
            auto gradBot = juce::Point<float>(anchorX, bladeBot).transformedBy(xform);
            auto gradTop = juce::Point<float>(anchorX, bladeTop).transformedBy(xform);
            juce::ColourGradient bladeGrad(
                bladeCol.withAlpha(0.85f), gradBot.x, gradBot.y,
                bladeCol.withAlpha(0.3f),  gradTop.x, gradTop.y, false);
            g.setGradientFill(bladeGrad);
            g.fillPath(bladePath);

            // Bright edge
            g.setColour(bladeCol.withAlpha(0.7f));
            g.strokePath(bladePath, juce::PathStrokeType(1.2f));
        }
    }

    //==========================================================================
    // Drawing: Front face
    //==========================================================================
    void drawVisor(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        float vw = r * 1.7f * hScale;
        float vh = r * 1.4f;
        float vcy = cy - r * 0.1f;

        g.setColour(screenDark);
        g.fillEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh);

        // 面罩边缘一圈蓝色荧光 (3层辉光)
        g.setColour(electricBlue.withAlpha(0.08f));
        g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 8.0f);
        g.setColour(electricBlue.withAlpha(0.30f));
        g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 3.5f);
        g.setColour(electricBlue.withAlpha(0.85f));
        g.drawEllipse(cx - vw / 2.0f, vcy - vh / 2.0f, vw, vh, 1.5f);
    }

    void drawEyes(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.3f) return;

        float vcy = cy - r * 0.1f;
        float spacing = r * 0.30f * hScale;
        float ew = r * 0.13f * hScale;

        float maxH = r * 0.5f, minH = r * 0.08f;
        float eh = juce::jmap(juce::jlimit(0.2f, 1.0f, eyeOpenness), 0.2f, 1.0f, minH, maxH);

        struct EyeParams { float x, w, corner; };
        EyeParams eyes[2] = {
            { cx - spacing, ew, ew * 0.4f },   // left eye
            { cx + spacing, ew, ew * 0.4f }    // right eye
        };

        if (isWinking)
        {
            // ===== Coordinate separation =====
            // Dividing line: cx (NONO center). Left eye stays LEFT, right eye stays RIGHT.
            // Left eye center: eyes[0].x = cx - spacing
            // Right eye center: eyes[1].x = cx + spacing
            // Gap between centers = 2 * spacing = r * 0.6 * hScale
            // We use cx as the hard boundary — nothing crosses it.

            // ----- Left eye: vertical egg ellipse (fillEllipse) -----
            {
                float eggW = ew * 1.3f;                     // 1.3x wider
                float eggH = eh * 1.8f;                     // 1.8x taller (egg shape)
                float eggCX = eyes[0].x;                    // left eye center X
                float eggLeft = eggCX - eggW * 0.5f;
                float eggTop = vcy - eggH * 0.5f;

                // Glow bloom behind egg
                float bloomR = eggH * 0.9f;
                juce::ColourGradient bloom(
                    electricBlue.withAlpha(0.35f), eggCX, vcy,
                    electricBlue.withAlpha(0.0f), eggCX + bloomR, vcy, true);
                g.setGradientFill(bloom);
                g.fillEllipse(eggCX - bloomR, vcy - bloomR, bloomR * 2.0f, bloomR * 2.0f);

                // Core egg
                g.setColour(electricBlue.withAlpha(eyeGlow));
                g.fillEllipse(eggLeft, eggTop, eggW, eggH);

                // Specular highlight on upper third
                g.setColour(juce::Colours::white.withAlpha(0.55f));
                float specW = eggW * 0.4f, specH = eggH * 0.12f;
                g.fillEllipse(eggCX - specW * 0.5f,
                              eggTop + eggH * 0.1f,
                              specW, specH);
            }

            // ----- Right eye: sharp "<" wink chevron -----
            {
                float chevW = spacing * 0.85f;              // width = 85% of half-face, stays in right zone
                float chevH = eh * 2.2f;                    // tall for drama
                // Anchor: chevron tip (fold point) sits at cx + spacing*0.35 (well past center)
                float tipX = cx + spacing * 0.35f;          // fold vertex — far from left eye
                float tipY = vcy;                           // vertically centered
                float openX = tipX + chevW;                 // right open end

                juce::Path winkPath;
                winkPath.startNewSubPath(openX, tipY - chevH * 0.5f);   // top-right
                winkPath.lineTo(tipX, tipY);                             // fold point
                winkPath.lineTo(openX, tipY + chevH * 0.5f);            // bottom-right

                // 3-layer glow: outer → mid → core
                g.setColour(electricBlue.withAlpha(0.12f));
                g.strokePath(winkPath, juce::PathStrokeType(14.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(electricBlue.withAlpha(0.35f));
                g.strokePath(winkPath, juce::PathStrokeType(9.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
                g.setColour(electricBlue.withAlpha(eyeGlow));
                g.strokePath(winkPath, juce::PathStrokeType(7.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
            }

            return; // Skip normal eye rendering
        }

        for (int idx = 0; idx < 2; ++idx)
        {
            auto& eye = eyes[idx];

            // Normal eye rendering
            if (eyeGlow > 0.72f)
            {
                float br = juce::jmax(eye.w, eh) * 2.0f * (eyeGlow - 0.3f);
                juce::ColourGradient bg(
                    electricBlue.withAlpha((eyeGlow - 0.5f) * 1.5f), eye.x, vcy,
                    electricBlue.withAlpha(0.0f), eye.x + br, vcy, true);
                g.setGradientFill(bg);
                g.fillEllipse(eye.x - br, vcy - br, br * 2.0f, br * 2.0f);
            }

            g.setColour(electricBlue.withAlpha(eyeGlow));
            g.fillRoundedRectangle(eye.x - eye.w / 2.0f, vcy - eh / 2.0f, eye.w, eh, eye.corner);

            g.setColour(juce::Colours::white.withAlpha(eyeGlow * 0.5f));
            float sw = eye.w * 0.45f, sh = eh * 0.1f;
            g.fillRoundedRectangle(eye.x - sw / 2.0f, vcy - eh / 2.0f + eh * 0.08f, sw, sh, sw * 0.3f);
        }
    }

    //==========================================================================
    // Drawing: Back face — Neon Holographic Drop Zone
    //==========================================================================
    void drawBackFace(juce::Graphics& g, float cx, float cy, float r, float hScale)
    {
        if (hScale < 0.2f) return;

        // Darker back panel
        float rx = r * hScale * 0.85f;
        g.setColour(juce::Colour(0xFF1A1A28));
        g.fillEllipse(cx - rx, cy - r * 0.85f, rx * 2.0f, r * 1.7f);

        if (nonoState != NonoState::Back && nonoState != NonoState::Analyzing)
            return;

        // Breathing alpha
        float breath = 0.65f + 0.35f * std::sin(neonBreathPhase);
        float ringR = r * 0.55f * hScale;

        // --- Holographic ring: 3-layer glow ---
        juce::Path circlePath;
        circlePath.addEllipse(cx - ringR, cy - ringR, ringR * 2.0f, ringR * 2.0f);

        // Layer 1: wide soft glow
        g.setColour(electricBlue.withAlpha(0.12f * breath));
        g.strokePath(circlePath, juce::PathStrokeType(8.0f * hScale));
        // Layer 2: medium glow
        g.setColour(electricBlue.withAlpha(0.35f * breath));
        g.strokePath(circlePath, juce::PathStrokeType(4.0f * hScale));
        // Layer 3: bright core
        g.setColour(electricBlue.withAlpha(0.9f * breath));
        g.strokePath(circlePath, juce::PathStrokeType(1.5f * hScale));

        // --- Neon cross "+" : 3-layer glow ---
        float crossLen = ringR * 0.55f;
        float crossThickBase = r * 0.04f * hScale;

        // Horizontal bar
        juce::Path hBar;
        hBar.addRoundedRectangle(cx - crossLen, cy - crossThickBase / 2.0f,
                                  crossLen * 2.0f, crossThickBase, crossThickBase * 0.3f);
        // Vertical bar
        juce::Path vBar;
        vBar.addRoundedRectangle(cx - crossThickBase / 2.0f, cy - crossLen,
                                  crossThickBase, crossLen * 2.0f, crossThickBase * 0.3f);

        // Cross layer 1: wide soft glow
        g.setColour(electricBlue.withAlpha(0.15f * breath));
        g.strokePath(hBar, juce::PathStrokeType(6.0f * hScale));
        g.strokePath(vBar, juce::PathStrokeType(6.0f * hScale));
        // Cross layer 2: medium glow
        g.setColour(electricBlue.withAlpha(0.4f * breath));
        g.strokePath(hBar, juce::PathStrokeType(3.0f * hScale));
        g.strokePath(vBar, juce::PathStrokeType(3.0f * hScale));
        // Cross layer 3: bright core fill
        g.setColour(electricBlue.withAlpha(0.95f * breath));
        g.fillPath(hBar);
        g.fillPath(vBar);
    }

    //==========================================================================
    // Drawing: Analysis ripples
    //==========================================================================
    void drawAnalysisRipples(juce::Graphics& g, float cx, float cy, float r)
    {
        const int numRipples = 4;
        for (int i = 0; i < numRipples; ++i)
        {
            float phase = std::fmod(ripplePhase + static_cast<float>(i) / static_cast<float>(numRipples), 1.0f);
            float rippleR = r * 0.4f + phase * r * 2.5f;
            float alpha = (1.0f - phase) * 0.35f;
            g.setColour(electricBlue.withAlpha(alpha));
            g.drawEllipse(cx - rippleR, cy - rippleR, rippleR * 2.0f, rippleR * 2.0f, 2.0f);
        }
    }

    //==========================================================================
    // Drawing: Results panel
    //==========================================================================
    void drawResultsBubble(juce::Graphics& g, juce::Rectangle<float> area, float nonoX)
    {
        const float fade = bubbleFadeAlpha;
        if (fade <= 0.01f) return;

        const float tailH = 14.0f;

        // Tail pointing up toward NONO
        juce::Path tail;
        float tailBaseY = area.getY() + tailH;
        tail.addTriangle(nonoX - 12.0f, tailBaseY,
                         nonoX + 12.0f, tailBaseY,
                         nonoX, area.getY());

        g.setColour(juce::Colour(0xFFF5F7FA).withMultipliedAlpha(fade));
        g.fillPath(tail);

        // Bubble body fills remaining area
        auto body = area.withTrimmedTop(tailH).reduced(4.0f, 0.0f);
        g.setColour(juce::Colour(0xFFF5F7FA).withMultipliedAlpha(fade));
        g.fillRoundedRectangle(body, 8.0f);
        g.setColour(electricBlue.withAlpha(0.5f * fade));
        g.drawRoundedRectangle(body, 8.0f, 1.5f);
        g.strokePath(tail, juce::PathStrokeType(1.5f));

        // ===== 2x2 Grid: 自适应文字排版 =====
        auto textArea = body.reduced(10.0f, 6.0f);
        float fontSize = juce::jlimit(14.0f, 26.0f, textArea.getHeight() * 0.2f);

        float halfW = textArea.getWidth() / 2.0f;
        float halfH = textArea.getHeight() / 2.0f;

        struct MetricInfo { const char* label; float value; const char* unit; };
        MetricInfo metrics[4] = {
            { u8"\u5cf0\u503c",             analysisResult.peakDBFS,         "dBFS" },
            { u8"\u77ac\u65f6\u6700\u5927", analysisResult.momentaryMaxLUFS, "LUFS" },
            { u8"\u77ed\u671f\u6700\u5927", analysisResult.shortTermMaxLUFS, "LUFS" },
            { u8"\u5e73\u5747\u54cd\u5ea6", analysisResult.integratedLUFS,   "LUFS" }
        };

        // Grid: [0,0] [1,0] / [0,1] [1,1]
        int gridPos[4][2] = { {0,0}, {1,0}, {0,1}, {1,1} };

        for (int i = 0; i < 4; ++i)
        {
            float cellX = textArea.getX() + gridPos[i][0] * halfW;
            float cellY = textArea.getY() + gridPos[i][1] * halfH;
            auto cell = juce::Rectangle<float>(cellX, cellY, halfW, halfH).reduced(3.0f);

            auto labelRect = cell.removeFromLeft(cell.getWidth() * 0.42f);
            auto valueRect = cell;

            // Label (muted grey)
            g.setFont(juce::Font(fontSize * 0.8f, juce::Font::bold));
            g.setColour(GoodMeterLookAndFeel::textMuted.withMultipliedAlpha(fade));
            g.drawText(juce::String(juce::CharPointer_UTF8(metrics[i].label)),
                       labelRect.toNearestInt(), juce::Justification::centredLeft, false);

            // Value (electric blue)
            g.setFont(juce::Font(fontSize, juce::Font::bold));
            g.setColour(electricBlue.withMultipliedAlpha(fade));
            juce::String valStr = (metrics[i].value <= -99.0f)
                                   ? juce::String(juce::CharPointer_UTF8(u8"\u2013\u221e"))
                                   : juce::String(metrics[i].value, 1);
            g.drawText(valStr + " " + metrics[i].unit,
                       valueRect.toNearestInt(), juce::Justification::centredLeft, false);
        }
    }

    //==========================================================================
    // Drawing: Floating test tube (always visible)
    //==========================================================================
    void drawFloatingTestTube(juce::Graphics& g, float cx, float cy, float r)
    {
        const float tubeX = cx + r * 1.7f;
        const float tubeY = cy - r * 0.1f;
        const float tubeW = r * 0.28f;
        const float tubeH = r * 1.1f;
        const float bulbR = tubeW * 0.65f;
        const float rimH = r * 0.07f;

        float tL = tubeX - tubeW / 2.0f, tR = tubeX + tubeW / 2.0f;
        float tTop = tubeY - tubeH / 2.0f + rimH, tBot = tubeY + tubeH / 2.0f;

        // ============================================================
        // EXPLOSION MODE: broken tube stump + flying shards + splashes
        // ============================================================
        if (isExploded)
        {
            float fade = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.3f);

            // --- 1. Jagged broken tube base (bottom 40%) ---
            float breakY = tTop + (tBot - tTop) * 0.55f;
            juce::Path brokenTube;
            brokenTube.startNewSubPath(tL, tBot - bulbR);
            brokenTube.cubicTo(tL, tBot, tR, tBot, tR, tBot - bulbR);
            brokenTube.lineTo(tR, breakY + tubeH * 0.05f);
            brokenTube.lineTo(tubeX + tubeW * 0.15f, breakY + tubeH * 0.15f);
            brokenTube.lineTo(tubeX + tubeW * 0.05f, breakY - tubeH * 0.02f);
            brokenTube.lineTo(tubeX - tubeW * 0.1f, breakY + tubeH * 0.12f);
            brokenTube.lineTo(tL, breakY + tubeH * 0.08f);
            brokenTube.closeSubPath();

            // Glass fill
            g.setColour(juce::Colours::white.withAlpha(0.3f * fade));
            g.fillPath(brokenTube);
            g.setColour(bodyEdge.withAlpha(0.7f * fade));
            g.strokePath(brokenTube, juce::PathStrokeType(1.5f));

            // Jagged crack glow (danger red)
            g.setColour(juce::Colour(0xFFFF0055).withAlpha(0.4f * fade));
            g.strokePath(brokenTube, juce::PathStrokeType(3.0f));

            // Residual rim (cracked)
            juce::Path rimStub;
            rimStub.addRoundedRectangle(tubeX - tubeW * 0.65f, tubeY - tubeH / 2.0f,
                                         tubeW * 1.3f, rimH, 2.0f);
            g.setColour(bodyEdge.withAlpha(0.5f * fade));
            g.fillPath(rimStub);

            // --- 2. Flying glass shards ---
            for (const auto& s : shards)
            {
                float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.8f);
                if (alpha < 0.01f) continue;

                float sx = tubeX + s.x;
                float sy = tubeY + s.y;

                juce::Path shard;
                float half = s.sz * 0.5f;
                shard.addTriangle(-half, -half * 0.6f,
                                   half, -half * 0.3f,
                                   0.0f, half);
                shard.applyTransform(juce::AffineTransform::rotation(s.angle)
                    .translated(sx, sy));

                // Glass shard: white core + cyan edge glow
                g.setColour(juce::Colours::white.withAlpha(0.7f * alpha));
                g.fillPath(shard);
                g.setColour(electricBlue.withAlpha(0.5f * alpha));
                g.strokePath(shard, juce::PathStrokeType(1.5f));
                // Outer glow
                g.setColour(electricBlue.withAlpha(0.15f * alpha));
                g.strokePath(shard, juce::PathStrokeType(4.0f));
            }

            // --- 3. Liquid splatter drops ---
            const juce::Colour dangerPink(0xFFFF0055);
            for (const auto& d : drops)
            {
                float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - explosionProgress * 0.7f);
                if (alpha < 0.01f) continue;

                float dx = tubeX + d.x;
                float dy = tubeY + d.y;
                float dsz = d.sz * (1.0f - explosionProgress * 0.3f);

                // Outer glow
                g.setColour(dangerPink.withAlpha(0.2f * alpha));
                g.fillEllipse(dx - dsz * 1.5f, dy - dsz * 1.5f, dsz * 3.0f, dsz * 3.0f);
                // Core drop
                g.setColour(dangerPink.withAlpha(0.85f * alpha));
                g.fillEllipse(dx - dsz * 0.5f, dy - dsz * 0.5f, dsz, dsz);
                // Specular
                g.setColour(juce::Colours::white.withAlpha(0.4f * alpha));
                g.fillEllipse(dx - dsz * 0.15f, dy - dsz * 0.25f, dsz * 0.3f, dsz * 0.25f);
            }

            // --- 4. Central explosion flash (first 30% of animation) ---
            if (explosionProgress < 0.3f)
            {
                float flashAlpha = (0.3f - explosionProgress) / 0.3f;
                float flashR = r * 0.6f * (1.0f + explosionProgress * 2.0f);
                juce::ColourGradient flash(
                    dangerPink.withAlpha(0.6f * flashAlpha), tubeX, tubeY,
                    dangerPink.withAlpha(0.0f), tubeX + flashR, tubeY, true);
                g.setGradientFill(flash);
                g.fillEllipse(tubeX - flashR, tubeY - flashR, flashR * 2.0f, flashR * 2.0f);
            }

            return; // Skip normal tube drawing
        }

        // ============================================================
        // NORMAL MODE: intact test tube
        // ============================================================

        // Combine normal shake + pouring angle; pivot at tube bottom
        float totalAngle = tubeAngle + clearPourAngle;
        float pivotX = tubeX;
        float pivotY = tubeY + tubeH / 2.0f;
        auto rotation = juce::AffineTransform::rotation(totalAngle, pivotX, pivotY);

        // Glass shell
        juce::Path tubePath;
        tubePath.startNewSubPath(tL, tTop);
        tubePath.lineTo(tL, tBot - bulbR);
        tubePath.cubicTo(tL, tBot, tR, tBot, tR, tBot - bulbR);
        tubePath.lineTo(tR, tTop);
        tubePath.closeSubPath();
        tubePath.applyTransform(rotation);

        // Edit mode glow around tube
        if (editModeTransition > 0.05f)
        {
            g.setColour(neonGreen.withAlpha(0.15f * editModeTransition));
            g.strokePath(tubePath, juce::PathStrokeType(6.0f));
            g.setColour(neonGreen.withAlpha(0.08f * editModeTransition));
            g.strokePath(tubePath, juce::PathStrokeType(12.0f));
        }

        g.setColour(juce::Colours::white.withAlpha(0.22f));
        g.fillPath(tubePath);
        g.setColour(bodyEdge.withAlpha(0.65f));
        g.strokePath(tubePath, juce::PathStrokeType(1.3f));

        // Liquid (controlled by liquidHeight: 0=empty, 1=full)
        if (liquidHeight > 0.01f)
        {
            float fullLiqH = tubeH * 0.6f;
            float liqH = fullLiqH * liquidHeight;
            float liqTop = tBot - liqH;
            float wave = tubeLiquidWave * liquidHeight;
            float lL = tL + 1.5f, lR = tR - 1.5f, lB = tBot - bulbR;

            juce::Path liqPath;
            liqPath.startNewSubPath(lL, liqTop + wave * 0.4f);
            liqPath.cubicTo(lL + tubeW * 0.3f, liqTop - wave,
                            lR - tubeW * 0.3f, liqTop + wave,
                            lR, liqTop - wave * 0.4f);
            liqPath.lineTo(lR, lB);
            liqPath.cubicTo(lR, tBot - 1.5f, lL, tBot - 1.5f, lL, lB);
            liqPath.closeSubPath();
            liqPath.applyTransform(rotation);

            // Color transition: magicPink → neonGreen based on editModeTransition
            juce::Colour liqColTop = magicPink.interpolatedWith(neonGreen, editModeTransition);
            juce::Colour liqColBot = magicPink.darker(0.4f).interpolatedWith(
                neonGreen.darker(0.3f), editModeTransition);

            juce::ColourGradient liqGrad(
                liqColTop.withAlpha(0.9f), tubeX, liqTop,
                liqColBot.withAlpha(0.95f), tubeX, tBot, false);
            g.setGradientFill(liqGrad);
            g.fillPath(liqPath);

            // Boiling bubbles (visible when editModeTransition > 0)
            if (editModeTransition > 0.02f)
            {
                float bubbleAlpha = editModeTransition * 0.7f;
                float tubeInnerW = (lR - lL);
                float tubeCX = (lL + lR) * 0.5f;

                for (const auto& b : tubeBubbles)
                {
                    // Bubble Y: rises from bottom to top of liquid
                    float bubbleY = tBot - bulbR * 0.5f - b.phase * liqH * 0.9f;
                    float bubbleX = tubeCX + b.xOff * tubeInnerW;

                    // Transform bubble position
                    auto bp = juce::Point<float>(bubbleX, bubbleY).transformedBy(rotation);

                    float bsz = b.sz * editModeTransition;
                    float bAlpha = bubbleAlpha * (1.0f - b.phase * 0.6f);  // fade near top

                    // Outer glow
                    g.setColour(neonGreen.withAlpha(bAlpha * 0.3f));
                    g.fillEllipse(bp.x - bsz * 1.5f, bp.y - bsz * 1.5f, bsz * 3.0f, bsz * 3.0f);
                    // Core
                    g.setColour(juce::Colours::white.withAlpha(bAlpha * 0.85f));
                    g.fillEllipse(bp.x - bsz * 0.5f, bp.y - bsz * 0.5f, bsz, bsz);
                }
            }
        }

        // Rim
        juce::Path rimPath;
        rimPath.addRoundedRectangle(tubeX - tubeW * 0.65f, tubeY - tubeH / 2.0f,
                                     tubeW * 1.3f, rimH, 2.0f);
        rimPath.applyTransform(rotation);
        g.setColour(bodyEdge);
        g.fillPath(rimPath);
    }

    //==========================================================================
    // Drawing: Particles
    //==========================================================================
    void drawParticles(juce::Graphics& g, float /*cx*/, float /*cy*/, float r)
    {
        if (!showParticles || particleProgress >= 1.0f) return;

        const int numP = 10;
        float alpha = (1.0f - particleProgress) * 0.85f;
        float dist = particleProgress * r * 1.8f;
        float sz = r * 0.05f * (1.0f - particleProgress * 0.6f);

        for (int i = 0; i < numP; ++i)
        {
            float a = static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(numP);
            float px = particleOriginX + dist * std::cos(a);
            float py = particleOriginY + dist * std::sin(a);
            g.setColour(((i % 2 == 0) ? GoodMeterLookAndFeel::accentPink : electricBlue).withAlpha(alpha));
            g.fillEllipse(px - sz, py - sz, sz * 2.0f, sz * 2.0f);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HoloNonoComponent)
};
