 GOODMETER Standalone: 4-Module Feature Blueprint

 Architecture Overview

 Four interconnected modules for the Standalone desktop-pet version. All new code guarded by #if JucePlugin_Build_Standalone where necessary. Plugin (VST3/AU) remains completely unaffected.

 Key Files Modified:
 | File                   | Modules |
 |------------------------|---------|
 | StandaloneNonoEditor.h | 1, 2, 4 |
 | HoloNonoComponent.h    | 1       |
 | MeterCardComponent.h   | 2, 4    |
 | PluginProcessor.h/.cpp | 3       |
 | StandaloneApp.cpp      | 3       |
 | NEW: AudioRecorder.h   | 3       |

 ---
 Module 1: Hover UI & Interaction Logic

 1.1 Architecture

 Three icon buttons (Gear, Tape, Shard) rendered in StandaloneNonoEditor::paint() — NOT as child components. They are purely graphical elements managed by the editor, with hit-testing handled in
  the editor's existing hitTest() override.

 Rationale: Child components would fight with the transparent window's click-through engine. Painting + custom hit-test is the established pattern (see how Nono body/test tube use Euclidean hit
 checks, not child components).

 1.2 Button Geometry

   ┌──────────────────┐
   │                   │
   │   Nono's body     │
   │                   │
   └──────────────────┘
          ↓ 8px gap
   ┌──┐  ┌──┐  ┌──┐
   │⚙ │  │⏺│  │💎│     ← 3 buttons, 36×36px each, 12px gaps
   └──┘  └──┘  └──┘

 Positions computed relative to Nono's body center (the same cx, cy, bodyR from hitTest()):

 // Button strip: 3 × 36px buttons, 12px gaps → total width = 132px
 float stripW = 3 * 36 + 2 * 12;  // 132px
 float stripX = nonoCX - stripW / 2;
 float stripY = nonoCY + bodyR * 1.5f + 8;  // below body hit circle

 1.3 State Machine

 enum class HoverButtonState {
     hidden,      // Buttons not visible
     appearing,   // Fly-in animation (0→1 over 12 frames)
     visible,     // Buttons fully visible, interactive
     retracting   // Fly-back animation (1→0 over 10 frames)
 };

 HoverButtonState hoverBtnState = HoverButtonState::hidden;
 float hoverBtnProgress = 0.0f;   // 0=hidden, 1=visible
 int hoverBtnHotIndex = -1;       // -1=none, 0=gear, 1=tape, 2=shard

 1.4 Safe Hover Zone (Critical UX)

 The "safe zone" is a union rectangle: Nono's hit circle bounding box + button strip area. Mouse must leave this ENTIRE zone before retraction begins.

 juce::Rectangle<float> getHoverSafeZone() const
 {
     // Nono body bounding box (from hitTest math)
     float hitR = bodyR * 1.5f;
     auto nonoRect = juce::Rectangle<float>(
         nonoCX - hitR, nonoCY - hitR, hitR * 2, hitR * 2);

     // Button strip rect
     auto btnRect = juce::Rectangle<float>(
         stripX, stripY, stripW, 36.0f);

     return nonoRect.getUnion(btnRect);
 }

 1.5 Hover Detection

 HoloNonoComponent currently has NO mouseEnter/mouseExit. Add them:

 In HoloNonoComponent.h:
 std::function<void()> onBodyHoverEnter;   // NEW callback
 std::function<void()> onBodyHoverExit;    // NEW callback

 void mouseEnter(const juce::MouseEvent&) override
 {
     if (onBodyHoverEnter) onBodyHoverEnter();
 }

 In StandaloneNonoEditor: Wire callbacks + 60Hz poll for safe zone exit:

 holoNono->onBodyHoverEnter = [this]() {
     if (hoverBtnState == HoverButtonState::hidden)
     {
         hoverBtnState = HoverButtonState::appearing;
         hoverBtnProgress = 0.0f;
     }
 };

 Exit detection runs in timerCallback() — poll global mouse position against safe zone. If mouse is outside safe zone AND no button is pressed → begin retraction.

 1.6 Appear/Retract Animation

 - Appear: Buttons fly up from Nono's center. Each button starts at (nonoCX, nonoCY) and eases to its final position over 12 frames (0.2s). Staggered: button 0 starts at frame 0, button 1 at
 frame 2, button 2 at frame 4.
 - Retract: Reverse — buttons fly back to Nono center over 10 frames (0.17s). All simultaneous.
 - Drawing: Scale + alpha from 0→1 (appear) or 1→0 (retract). Use easeOutCubic for appear, easeInCubic for retract.

 1.7 Button Visuals

 Each button: 36×36 circle with:
 - Background: Colour(0xFF2A2A35).withAlpha(0.75f) (dark frosted)
 - Icon: White Unicode glyph, 16px
 - Hover: background → accentPink.withAlpha(0.85f), icon brighter
 - Active icons: ⚙ (U+2699), ⏺ (filled circle for record), ◆ (U+25C6 for shard)

 1.8 hitTest() Extension

 Add button hit checks BEFORE the existing Nono/card checks:

 bool hitTest(int x, int y) override
 {
     // 0. Hover buttons (highest priority when visible)
     if (hoverBtnState == HoverButtonState::visible)
     {
         for (int i = 0; i < 3; ++i)
             if (getButtonRect(i).contains(fx, fy))
                 return true;
     }

     // ... existing Nono body, test tube, cards ...
 }

 1.9 Double-Click Override

 When mouseDoubleClick fires on Nono body (triggering smile orbit), if hover buttons are visible:
 1. Set hoverBtnState = hidden instantly
 2. Set hoverBtnProgress = 0
 3. Continue with normal double-click logic (no interference)

 1.10 Phase Awareness

 Buttons appear in ALL interactive phases (compact, settled, floating). The button strip position is always computed relative to Nono's current body center, which naturally moves as Nono moves.

 ---
 Module 2: Settings Menu + Free-Resize + Auto-Match Snap

 2.1 Settings PopupMenu (Gear Button Click)

 ┌─────────────────────────────────┐
 │  ✓ Normal Size                  │
 │    Mini Size                    │
 │ ─────────────────────────────── │
 │  Audio Input ▸                  │
 │    ┌─────────────────────────┐  │
 │    │ ✓ MacBook Pro Speakers  │  │
 │    │   External Headphones   │  │
 │    │   Soundflower (2ch)     │  │
 │    └─────────────────────────┘  │
 └─────────────────────────────────┘

 Implementation:

 void showSettingsMenu()
 {
     juce::PopupMenu menu;

     // Size toggle
     menu.addItem(1, "Normal Size", true, !isMiniMode);
     menu.addItem(2, "Mini Size", true, isMiniMode);
     menu.addSeparator();

     // Audio Input submenu
     juce::PopupMenu audioMenu;
     auto& deviceManager = getDeviceManager();
     auto* currentDevice = deviceManager.getCurrentAudioDevice();
     // ... enumerate available input devices ...
     menu.addSubMenu("Audio Input", audioMenu);

     menu.showMenuAsync(juce::PopupMenu::Options()
         .withTargetScreenArea(getButtonScreenRect(0)));
 }

 2.2 Accessing AudioDeviceManager

 StandalonePluginHolder owns the AudioDeviceManager. The editor needs a reference path:

 Path: StandaloneNonoEditor → getTopLevelComponent() → cast to DesktopPetWindow → pluginHolder->deviceManager

 Add a helper in StandaloneNonoEditor:

 juce::AudioDeviceManager& getDeviceManager()
 {
     auto* topLevel = getTopLevelComponent();
     auto* petWindow = dynamic_cast<goodmeter::DesktopPetWindow*>(topLevel);
     return petWindow->pluginHolder->deviceManager;
 }

 This requires making DesktopPetWindow and StandalonePluginHolder::deviceManager accessible. StandalonePluginHolder::deviceManager is already public in JUCE's implementation.

 2.3 Mini/Normal Size Toggle

 isMiniMode flag already exists on MeterCardComponent. The settings toggle:

 1. Sets a baseMiniMode flag in StandaloneNonoEditor
 2. Propagates to all docked cards: card->isMiniMode = baseMiniMode
 3. Updates folded card dimensions: Mini = 160×32, Normal = 220×56
 4. Re-runs layoutSettled() or layoutFloating()

 2.4 Free-Resize for Floating Cards

 When a card is floating (cardFloatState[i].isFloating == true AND card->isExpanded()), enable edge/corner resize.

 Implementation in MeterCardComponent:

 Add a resize corner grip (bottom-right, 12×12px):

 // In MeterCardComponent::paint() — draw resize grip when floating + expanded
 if (!isDocked && isExpanded && !inJiggleMode)
 {
     auto cr = getCardRect();
     float gx = cr.getRight() - 12.0f;
     float gy = cr.getBottom() - 12.0f;
     // Draw 3 diagonal dots (grip indicator)
     g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(0.4f));
     for (int i = 0; i < 3; ++i)
         g.fillEllipse(gx + i * 4.0f, gy + (2 - i) * 4.0f, 2.5f, 2.5f);
 }

 Mouse handling — add resize detection in MeterCardComponent:

 bool isResizeCornerHit(const juce::MouseEvent& e) const
 {
     if (isDocked || !isExpanded) return false;
     auto cr = getCardRect();
     auto corner = juce::Rectangle<float>(
         cr.getRight() - 16.0f, cr.getBottom() - 16.0f, 16.0f, 16.0f);
     return corner.contains(e.position);
 }

 In mouseDown: if isResizeCornerHit(), set isResizing = true + store start bounds.
 In mouseDrag: if isResizing, compute new width/height with minimum constraints:
 - Min expanded width: 180px
 - Min expanded height: headerHeight + 100px
 - Max: no limit (screen bounded)

 In mouseUp: if isResizing, finalize and notify parent via onHeightChanged().

 2.5 Auto-Match Width on Snap ("Contagion" Effect)

 In StandaloneNonoEditor::commitSnap(), after positioning the drag card:

 void commitSnap(const PendingSnap& snap)
 {
     // ... existing snap positioning + group formation ...

     // ★ CONTAGION: Match width to anchor card's width
     int finalGroupID = cardFloatState[snap.dragCardIndex].snapGroupID;
     for (auto& group : snapGroups)
     {
         if (group.groupID != finalGroupID) continue;
         if (group.members.empty()) break;

         // Anchor = first member (the one already in position)
         auto* anchor = getCard(group.members[0]);
         if (!anchor) break;

         int anchorW = anchor->getWidth();
         for (int idx : group.members)
         {
             auto* mc = getCard(idx);
             if (mc && mc->getWidth() != anchorW)
             {
                 mc->setSize(anchorW, mc->getHeight());
                 mc->resized();  // Content re-layouts to new width
             }
         }
         recalcGroupOffsets(finalGroupID);
         break;
     }
 }

 2.6 Content Component Resize Behavior

 All meter components (LevelsMeter, VUMeter, Band3, etc.) already use relative coordinate drawing in their paint() methods (they draw relative to getLocalBounds()). Their resized() methods will
 naturally re-layout when card dimensions change. No per-component changes needed.

 ---
 Module 3: Native Audio Recording Engine

 3.1 Architecture Overview

 Audio Thread (processBlock)
     │
     ├─ [std::atomic<bool> isRecording]
     │
     ├─ Push L/R samples → RecordingFIFO (lock-free ring buffer)
     │
     │                         ↓ (polled at ~100Hz)
     │
     │              AudioRecorderThread (juce::Thread)
     │                    │
     │                    ├─ Pop from FIFO
     │                    ├─ Write to WavAudioFormat (24-bit)
     │                    └─ On stop: flush + close file
     │
     └─ Normal metering pipeline (unchanged)

 3.2 New File: AudioRecorder.h

 #pragma once
 #include <JuceHeader.h>

 class AudioRecorder : public juce::Thread
 {
 public:
     AudioRecorder() : Thread("GOODMETER Recorder") {}
     ~AudioRecorder() override { stopRecording(); }

     // Called from UI thread
     void startRecording(double sampleRate, int numChannels);
     void stopRecording();
     bool isCurrentlyRecording() const;

     // Called from audio thread (lock-free)
     void pushSamples(const float* const* channelData,
                      int numChannels, int numSamples);

     // Recent recordings
     juce::StringArray getRecentFiles() const;
     juce::File getRecordingDirectory() const;

     // Current format info
     juce::String getFormatString() const;

 private:
     void run() override;  // Writer thread main loop

     // Lock-free ring buffer (stereo interleaved)
     static constexpr int fifoSize = 131072;  // ~1.4s at 96kHz stereo
     float fifoBuffer[fifoSize];
     juce::AbstractFifo abstractFifo { fifoSize };

     std::atomic<bool> recording { false };
     std::atomic<double> currentSampleRate { 48000.0 };
     std::atomic<int> currentNumChannels { 2 };

     std::unique_ptr<juce::FileOutputStream> fileStream;
     std::unique_ptr<juce::AudioFormatWriter> writer;
     juce::File currentFile;

     // Recent recordings (max 10)
     juce::StringArray recentFiles;

     juce::CriticalSection writerLock;  // Only for start/stop transitions
 };

 3.3 Recording FIFO (Audio Thread)

 In PluginProcessor::processBlock(), add at the END (after all metering):

 #if JucePlugin_Build_Standalone
     if (audioRecorder != nullptr && audioRecorder->isCurrentlyRecording())
     {
         const float* channels[] = {
             buffer.getReadPointer(0),
             buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0)
         };
         audioRecorder->pushSamples(channels, 2, buffer.getNumSamples());
     }
 #endif

 3.4 Writer Thread

 void AudioRecorder::run()
 {
     float tempBuffer[4096];  // Batch read from FIFO

     while (!threadShouldExit())
     {
         int available = abstractFifo.getNumReady();
         if (available > 0)
         {
             int toRead = juce::jmin(available, 4096);
             int start1, size1, start2, size2;
             abstractFifo.prepareToRead(toRead, start1, size1, start2, size2);

             // Copy from ring buffer
             if (size1 > 0) memcpy(tempBuffer, fifoBuffer + start1, size1 * sizeof(float));
             if (size2 > 0) memcpy(tempBuffer + size1, fifoBuffer + start2, size2 * sizeof(float));

             abstractFifo.finishedRead(toRead);

             // De-interleave and write
             // ... write to AudioFormatWriter ...
         }
         else
         {
             Thread::sleep(5);  // ~200Hz poll rate
         }
     }
 }

 3.5 File Naming & Storage

 - Directory: ~/Music/GOODMETER/
 - Filename: GOODMETER_2026-03-07_14-30-15.wav
 - Format: WAV, 24-bit integer, device sample rate, stereo
 - Recent files list: stored in ApplicationProperties (max 10 entries)

 3.6 macOS Native Menu Bar

 Using juce::MenuBarModel::setMacMainMenu() — sets the app-level menu bar without requiring a DocumentWindow.

 New class in StandaloneApp.cpp:

 class GoodMeterMenuBar : public juce::MenuBarModel
 {
 public:
     GoodMeterMenuBar(AudioRecorder& recorder,
                      GOODMETERAudioProcessor& processor)
         : audioRecorder(recorder), audioProcessor(processor)
     {}

     juce::StringArray getMenuBarNames() override
     {
         return { "Recording" };
     }

     juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override
     {
         juce::PopupMenu menu;
         if (menuIndex == 0)
         {
             // Toggle recording
             bool isRec = audioRecorder.isCurrentlyRecording();
             menu.addItem(1, isRec ? "Stop Recording" : "Start Recording");
             menu.addSeparator();

             // Format info (read-only, greyed out)
             double sr = audioProcessor.getSampleRate();
             juce::String fmt = "Format: 24-bit / "
                 + juce::String(static_cast<int>(sr)) + " Hz (Device Locked)";
             menu.addItem(2, fmt, false);  // disabled = read-only
             menu.addSeparator();

             // Recent recordings submenu
             juce::PopupMenu recentMenu;
             auto files = audioRecorder.getRecentFiles();
             for (int i = 0; i < files.size(); ++i)
             {
                 juce::File f(files[i]);
                 recentMenu.addItem(100 + i, f.getFileName());
             }
             if (files.isEmpty())
                 recentMenu.addItem(-1, "(No recordings)", false);
             menu.addSubMenu("Recent Recordings", recentMenu);
         }
         return menu;
     }

     void menuItemSelected(int id, int) override
     {
         if (id == 1)  // Toggle recording
         {
             if (audioRecorder.isCurrentlyRecording())
                 audioRecorder.stopRecording();
             else
                 audioRecorder.startRecording(
                     audioProcessor.getSampleRate(), 2);
         }
         else if (id >= 100)  // Recent file → reveal in Finder
         {
             auto files = audioRecorder.getRecentFiles();
             int idx = id - 100;
             if (idx < files.size())
                 juce::File(files[idx]).revealToUser();
         }
     }
 };

 Integration in GoodMeterStandaloneApp::initialise():

 // After creating mainWindow...
 menuBar = std::make_unique<GoodMeterMenuBar>(
     *audioRecorder, *processor);
 juce::MenuBarModel::setMacMainMenu(menuBar.get());

 3.7 Recording Toggle from Hover Button

 The Tape hover button (Module 1) triggers recording via:

 // In StandaloneNonoEditor — when tape button clicked:
 void handleTapeButtonClick()
 {
     auto& recorder = audioProcessor.getAudioRecorder();
     if (recorder.isCurrentlyRecording())
         recorder.stopRecording();
     else
         recorder.startRecording(audioProcessor.getSampleRate(), 2);
 }

 Visual feedback: When recording, the tape button pulses red (sinusoidal alpha 0.6→1.0 at 2Hz).

 3.8 PluginProcessor Changes

 // In PluginProcessor.h — add:
 #if JucePlugin_Build_Standalone
     std::unique_ptr<AudioRecorder> audioRecorder;
     AudioRecorder& getAudioRecorder();
 #endif

 // In PluginProcessor constructor:
 #if JucePlugin_Build_Standalone
     audioRecorder = std::make_unique<AudioRecorder>();
 #endif

 ---
 Module 4: Thanos Snap Stow/Recall System

 4.1 Product Logic (One-Click "Thanos Snap")

 1. Click Shard button → evaluate all 8 cards
 2. Target condition: card->isDocked || !card->isExpanded()
 3. Action: All targets play shatter VFX simultaneously → marked stowed
 4. Recall: Click Nono body → stowed cards fly out as orbs → dock to shelf

 4.2 State Management: Active/Inactive Roster

 // In StandaloneNonoEditor:
 bool cardStowed[numCards] = {};  // true = stowed inside Nono

 int countStowedCards() const
 {
     int n = 0;
     for (int i = 0; i < numCards; ++i)
         if (cardStowed[i]) n++;
     return n;
 }

 int countActiveCards() const { return numCards - countStowedCards(); }

 4.3 Stow Trigger (Shard Button Click)

 void handleShardButtonClick()
 {
     if (phase != AnimPhase::settled && phase != AnimPhase::floating)
         return;

     bool anyStowed = false;
     for (int i = 0; i < numCards; ++i)
     {
         if (cardStowed[i]) continue;  // already stowed

         auto* card = getCard(i);
         if (!card) continue;

         // Target condition: docked OR not expanded
         bool shouldStow = card->isDocked || !card->getExpanded();
         if (shouldStow)
         {
             cardStowed[i] = true;
             triggerShatterVFX(i);
             anyStowed = true;
         }
     }

     if (anyStowed)
     {
         // After shatter animation completes, hide stowed cards
         // and relayout remaining cards
     }
 }

 4.4 Shatter/Fragmentation VFX

 Adapted from HoloNono's existing ExpShard explosion system. Per-card particle burst.

 struct CardShardParticle
 {
     float x, y;          // position offset from card center
     float vx, vy;        // velocity
     float angle, spin;   // rotation
     float size;          // fragment size
     juce::Colour colour; // card's status colour
 };

 struct CardShatterState
 {
     bool active = false;
     float progress = 0.0f;  // 0→1 over ~40 frames (0.67s)
     CardShardParticle shards[12];   // 12 triangular fragments
     CardShardParticle sparkles[8];  // 8 small sparkle dots
     float cardCenterX, cardCenterY; // origin point
     juce::Colour cardColour;
 };

 CardShatterState shatterStates[numCards];

 Initialization (when stow triggers):

 void triggerShatterVFX(int cardIndex)
 {
     auto* card = getCard(cardIndex);
     if (!card) return;

     auto& state = shatterStates[cardIndex];
     state.active = true;
     state.progress = 0.0f;
     state.cardCenterX = card->getBounds().getCentreX();
     state.cardCenterY = card->getBounds().getCentreY();
     state.cardColour = getCardColour(cardIndex);

     auto& rng = juce::Random::getSystemRandom();
     for (int i = 0; i < 12; ++i)
     {
         auto& s = state.shards[i];
         float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
         float speed = 2.0f + rng.nextFloat() * 4.0f;
         s.x = 0; s.y = 0;
         s.vx = std::cos(angle) * speed;
         s.vy = std::sin(angle) * speed - 1.5f;  // slight upward bias
         s.angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
         s.spin = (rng.nextFloat() - 0.5f) * 0.3f;
         s.size = 8.0f + rng.nextFloat() * 16.0f;
         s.colour = state.cardColour;
     }
     for (int i = 0; i < 8; ++i)
     {
         auto& sp = state.sparkles[i];
         float angle = rng.nextFloat() * juce::MathConstants<float>::twoPi;
         float speed = 3.0f + rng.nextFloat() * 5.0f;
         sp.x = 0; sp.y = 0;
         sp.vx = std::cos(angle) * speed;
         sp.vy = std::sin(angle) * speed;
         sp.size = 3.0f + rng.nextFloat() * 5.0f;
         sp.colour = juce::Colours::white;
     }

     // Immediately hide the card (it "shatters")
     card->setVisible(false);
 }

 Physics tick (in timerCallback()):

 for (int i = 0; i < numCards; ++i)
 {
     auto& state = shatterStates[i];
     if (!state.active) continue;

     state.progress += 1.0f / 40.0f;  // ~0.67s at 60Hz
     if (state.progress >= 1.0f)
     {
         state.active = false;
         continue;
     }

     float gravity = 0.15f;
     for (auto& s : state.shards)
     {
         s.x += s.vx;
         s.y += s.vy;
         s.vy += gravity;
         s.angle += s.spin;
     }
     for (auto& sp : state.sparkles)
     {
         sp.x += sp.vx;
         sp.y += sp.vy;
         sp.vy += gravity * 0.5f;
     }
 }
 repaint();

 Rendering (in paint()):

 void drawShatterEffects(juce::Graphics& g)
 {
     for (int i = 0; i < numCards; ++i)
     {
         auto& state = shatterStates[i];
         if (!state.active) continue;

         float fade = 1.0f - state.progress;  // alpha fades out

         // Draw shards (rotated triangles)
         for (const auto& s : state.shards)
         {
             float sx = state.cardCenterX + s.x;
             float sy = state.cardCenterY + s.y;

             juce::Path tri;
             tri.addTriangle(-s.size * 0.5f, -s.size * 0.3f,
                             s.size * 0.5f, -s.size * 0.2f,
                             0.0f, s.size * 0.4f);
             tri.applyTransform(
                 juce::AffineTransform::rotation(s.angle)
                     .translated(sx, sy));

             g.setColour(s.colour.withAlpha(fade * 0.8f));
             g.fillPath(tri);
             g.setColour(juce::Colours::white.withAlpha(fade * 0.3f));
             g.strokePath(tri, juce::PathStrokeType(1.0f));
         }

         // Draw sparkles (small glowing dots)
         for (const auto& sp : state.sparkles)
         {
             float sx = state.cardCenterX + sp.x;
             float sy = state.cardCenterY + sp.y;
             float r = sp.size * (1.0f - state.progress * 0.5f);

             g.setColour(sp.colour.withAlpha(fade * 0.6f));
             g.fillEllipse(sx - r, sy - r, r * 2, r * 2);
         }
     }
 }

 4.5 Selective Recall Animation

 When Nono body is clicked (smile orbit), if any cards are stowed:

 // Modify onSmileOrbitTriggered callback:
 holoNono->onSmileOrbitTriggered = [this]()
 {
     if (phase == AnimPhase::compact)
     {
         // Normal first deployment
         triggerAnimationSequence();
     }
     else if (phase == AnimPhase::floating && countStowedCards() > 0)
     {
         // Selective recall: only stowed cards
         triggerSelectiveRecall();
     }
     else if (phase == AnimPhase::floating)
     {
         // No stowed cards: full recall (existing behavior)
         triggerRecall();
     }
     else if (phase == AnimPhase::settled && countStowedCards() > 0)
     {
         // From settled: expand canvas, emit stowed orbs, dock to shelf
         triggerSelectiveRecall();
     }
 };

 Selective recall animation — emits ONLY stowed orbs:

 void triggerSelectiveRecall()
 {
     // Expand canvas if in settled mode
     if (phase == AnimPhase::settled)
         expandCanvasForAnimation();

     phase = AnimPhase::orbFlyOut;

     // Only launch orbs for STOWED cards
     for (int i = 0; i < numCards; ++i)
     {
         cardLaunched[i] = false;  // Reset all
         cardFlyProgress[i] = 0.0f;

         if (cardStowed[i])
         {
             // This card will be launched during tickOrbFlyOut
             // (sequential launch via nextCardIndex logic)
             auto* card = getCard(i);
             if (card) card->setVisible(false);
         }
     }

     // nextCardIndex will iterate, but only launch stowed cards
     // Modify tickOrbFlyOut to skip non-stowed cards
     animFrameCounter = 0;
     nextCardIndex = 0;
     wheelProgress = 0.0f;
     trailHead = 0;
     cardFadeInAlpha = 0.0f;

     // Initialize orb trails for stowed cards
     float centerX = /* Nono center */;
     float centerY = /* Nono center */;
     for (int i = 0; i < numCards; ++i)
     {
         orbStates[i] = { centerX, centerY, getCardColour(i) };
         for (int j = 0; j < trailLen; ++j)
             orbTrail[i][j] = { centerX, centerY };
     }
 }

 Modified tickOrbFlyOut — skip non-stowed cards during selective recall:

 void tickOrbFlyOut()
 {
     animFrameCounter++;

     // Launch orbs sequentially every 2 frames
     if (animFrameCounter % 2 == 0 && nextCardIndex < numCards)
     {
         // Skip non-target cards (not stowed during selective recall)
         while (nextCardIndex < numCards && !shouldLaunchCard(nextCardIndex))
             nextCardIndex++;

         if (nextCardIndex < numCards)
         {
             cardLaunched[nextCardIndex] = true;
             nextCardIndex++;
         }
     }
     // ... rest unchanged ...
 }

 bool shouldLaunchCard(int index) const
 {
     if (isSelectiveRecall)
         return cardStowed[index];
     return true;  // Full deploy: launch all
 }

 4.6 After Recall Completes

 When selective recall animation finishes (cardFadeIn → canvasShrink):

 // In tickRecall or post-canvasShrink cleanup:
 for (int i = 0; i < numCards; ++i)
 {
     if (cardStowed[i])
     {
         cardStowed[i] = false;  // Un-stow
         auto* card = getCard(i);
         if (card)
         {
             card->setVisible(true);
             card->isDocked = true;
         }
     }
 }

 4.7 Layout Adjustment for Stowed Cards

 When cards are stowed, the bookshelf (settled layout) should only show active cards:

 void layoutSettled()
 {
     int activeCount = countActiveCards();
     int totalShelfH = activeCount * foldedCardH + juce::jmax(0, activeCount - 1) * shelfGap;
     int h = getHeight();
     int shelfStartY = (h - totalShelfH) / 2;

     int slot = 0;
     for (int i = 0; i < numCards; ++i)
     {
         auto* card = getCard(i);
         if (!card) continue;

         if (cardStowed[i])
         {
             card->setVisible(false);
             continue;
         }

         card->setTransform(juce::AffineTransform());
         card->setBounds(settledPadding,
                         shelfStartY + slot * (foldedCardH + shelfGap),
                         foldedCardW, foldedCardH);
         slot++;
     }
 }

 Similarly modify layoutFloating() to skip stowed cards in the docked shelf.

 ---
 Implementation Order

 | Phase | Module                              | Dependency             | Estimated Steps |
 |-------|-------------------------------------|------------------------|-----------------|
 | 1     | Module 1: Hover UI                  | None                   | 8 steps         |
 | 2     | Module 2: Settings Menu (gear only) | Module 1               | 5 steps         |
 | 3     | Module 4: Thanos Snap Stow/Recall   | Module 1               | 10 steps        |
 | 4     | Module 2: Free-Resize + Auto-Match  | None (can parallel)    | 8 steps         |
 | 5     | Module 3: Recording Engine          | Module 1 (tape button) | 12 steps        |

 Phase 1: Hover UI (Module 1) — Foundation

 1. Add onBodyHoverEnter/onBodyHoverExit to HoloNonoComponent
 2. Add HoverButtonState enum + animation variables to StandaloneNonoEditor
 3. Wire hover callbacks in constructor
 4. Implement drawHoverButtons() in paint()
 5. Implement safe zone calculation + timerCallback polling
 6. Add button hit zones to hitTest()
 7. Implement appear/retract animation in timerCallback
 8. Add double-click override (hide buttons instantly)

 Phase 2: Settings Menu (Module 2 partial)

 9. Implement showSettingsMenu() with PopupMenu
 10. Add AudioDeviceManager access helper
 11. Wire gear button click → showSettingsMenu()
 12. Implement Mini/Normal size toggle
 13. Propagate size change to cards + relayout

 Phase 3: Thanos Snap (Module 4)

 14. Add cardStowed[numCards] roster array
 15. Add CardShatterState + particle structs
 16. Implement triggerShatterVFX() initialization
 17. Add shatter physics tick to timerCallback
 18. Implement drawShatterEffects() in paint()
 19. Wire shard button click → handleShardButtonClick()
 20. Modify layoutSettled()/layoutFloating() to skip stowed cards
 21. Implement triggerSelectiveRecall()
 22. Modify tickOrbFlyOut() with shouldLaunchCard() filter
 23. Post-recall cleanup: un-stow cards, resize window

 Phase 4: Free-Resize + Auto-Match (Module 2 continued)

 24. Add resize grip drawing to MeterCardComponent::paint()
 25. Add isResizeCornerHit() detection
 26. Implement resize drag handling in MeterCardComponent
 27. Add per-card custom size tracking (customWidth, customHeight)
 28. Implement "Contagion" effect in commitSnap()
 29. Update getCardVisualRect() for variable-width cards
 30. Test edge cases: resize → snap → detach → resize again

 Phase 5: Recording Engine (Module 3)

 31. Create AudioRecorder.h with FIFO + writer thread
 32. Implement startRecording()/stopRecording() lifecycle
 33. Add pushSamples() lock-free method
 34. Implement writer thread main loop
 35. Add recording tap point in processBlock()
 36. Create GoodMeterMenuBar class
 37. Integrate menu bar in StandaloneApp.cpp
 38. Implement "Start/Stop Recording" menu action
 39. Implement format info display (read-only)
 40. Implement recent recordings list + revealToUser()
 41. Wire tape hover button → recording toggle
 42. Add recording pulse animation to tape button

 ---
 JUCE Classes Used

 | Class                        | Purpose                                      |
 |------------------------------|----------------------------------------------|
 | juce::PopupMenu              | Settings gear menu, audio input submenu      |
 | juce::AudioDeviceManager     | Audio input device enumeration/switching     |
 | juce::MenuBarModel           | macOS native menu bar ("Recording" dropdown) |
 | juce::AbstractFifo           | Lock-free ring buffer for recording FIFO     |
 | juce::WavAudioFormat         | 24-bit WAV file writing                      |
 | juce::AudioFormatWriter      | Stream audio samples to disk                 |
 | juce::FileOutputStream       | WAV file output stream                       |
 | juce::Thread                 | Background writer thread                     |
 | juce::File::revealToUser()   | Open Finder to recording location            |
 | juce::ApplicationProperties  | Persist recent recordings list               |
 | juce::StandalonePluginHolder | Access to deviceManager (existing)           |

