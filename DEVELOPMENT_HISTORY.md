# GOODMETER Development History & Lessons Learned

> **Purpose**: Complete chronological record of development decisions, bugs, fixes,
> and hard-won lessons. Written so that any future AI assistant can understand not just
> WHAT the code does, but WHY it's shaped the way it is, and what pitfalls to avoid.
>
> **Audience**: Claude Code, Cursor, Codex, or any AI picking up this project.
> **Last updated**: 2026-03-07

---

## Part 1: Project Timeline (45 commits over ~8 days)

### Phase 1: DSP Foundation (Feb 27)
```
fa40d0a  Initial GOODMETER project structure
5ad3116  V0.1.0: DSP Foundation Complete
         - PluginProcessor with peak, RMS, LUFS (BS.1770-4), phase correlation
         - K-Weighting filter (high shelf @1500Hz +4dB, HP @38Hz)
         - Lock-free FIFO for FFT data (audio→GUI thread)
```

### Phase 2: UI Foundation (Feb 27-28)
```
7fe5362  V0.2.0: UI Foundation Complete
         - GoodMeterLookAndFeel (dark theme, professional colors)
         - MeterCardComponent (collapsible cards with headers)
b0bde02  V0.3.0: LevelsMeterComponent
c02c4fd  V0.4.0: Phase Correlation Condenser Tube
         ↓ CRASH: dangling pointer in card→content wiring
9caba45  V0.4.1: Critical Crash Fix - Dangling Pointer
822f05c  V0.4.2: FFT memory error fix
261b846  V0.5.0: MeterCardComponent thorough rewrite (Web→JUCE pitfalls)
         ↓ LESSON: JUCE parent paint() executes BEFORE children, not after
```

### Phase 3: Meter Components (Feb 28 - Mar 2)
```
78ce7f2  V0.6.0: Classic VU Meter (AffineTransform needle rotation)
         ↓ BUG: dB double-conversion, needle invisible due to coordinates
1489bfc  Fix: VU coordinate system using AffineTransform rotation
e56305c  Phase 3.4: FFT Spectrum Analyzer (log freq + smooth polygon)
077e7ce  Spectrogram: Pink barcode → Pink cloud (breathing noise)
5527c9e  SpectrogramComponent with ring buffer rendering
```

### Phase 4: Layout Wars (Mar 2-4)
```
f97427d  V0.8.0: Responsive dual-column layout
         ↓ 6 ROUNDS of drag-freeze debugging (see LESSONS_LEARNED.md #9)
         ↓ OpenGL → MML deadlock → Bresenham bitmap → Timer storm
         ↓ HarfBuzz font creation → Offline text caching
66c90e8  Responsive layout rewrite + NONO bubble adaptive
d87100a  Flex-Grow elastic layout + 2-column 4v5 balance
fa7130e  NONO explosion animation + spectrogram overlay scale
```

### Phase 5: Performance Emergency (Mar 4-5)
```
a1e30b0  perf+fix: NONO offline analysis 40s→4s emergency rescue
         - 4-meter perceptual calibration
         - DSP bug fixes
         - handlePaint: 387→43 samples (↓90%)
         - drawText in paint: 312→0 (↓100%)
```

### Phase 6: Standalone Desktop Pet (Mar 5-7) ← CURRENT
```
5c0014e  feat: Standalone desktop pet mode - Phase 1 (transparent Nono)
         - StandaloneApp.cpp: custom JUCEApplication + DesktopPetWindow
         - StandaloneNonoEditor.h: 2800-line mega editor
         - 9-phase animation state machine
         - Snap group engine, shatter VFX, hover buttons
[uncommitted] Multiple critical fixes (see Part 3 below)
```

---

## Part 2: The Original Battle Plan vs Reality

### What Was Planned (Claude作战书.md — 4-Module Blueprint)

The standalone desktop pet was designed as 4 interconnected modules:

| Module | Plan | Status | Notes |
|--------|------|--------|-------|
| 1: Hover UI | 3 icon buttons (gear/tape/shard) as painted graphics | ✅ Complete | Originally 36px with Unicode text, upgraded to 52px PNG |
| 2: Settings | Gear → PopupMenu, Mini/Normal toggle, free-resize | ⚠️ Partial | Settings menu done, mini toggle deferred, free-resize done |
| 3: Recording | Lock-free FIFO recorder + macOS menu bar | ✅ Complete | AudioRecorder.h + "Recording" menu bar |
| 4: Thanos Snap | Shard stow + selective recall + shatter VFX | ✅ Complete | Required 3 bug fixes for state machine deadlock |

### Key Deviations from Plan

**1. Hover Button Icons: Unicode → PNG**

Original plan used Unicode glyphs (⚙, ⏺, ◆) on 36px dark circles.
Changed to 52px PNG images (btn_settings.png, btn_record.png, btn_stow.png)
with radial gradient glow instead of opaque circle backgrounds.

```cpp
// Original plan (NOT implemented):
g.setColour(Colour(0xFF2A2A35).withAlpha(0.75f));
g.fillEllipse(bx, by, 36, 36);
g.drawText("⚙", ...);

// What was actually built:
g.drawImage(*icons[i], destRect, RectanglePlacement::centred);
// + radial gradient glow on hover per button color
```

**2. AudioDeviceManager Access: Cast → Singleton**

Original plan: cast getTopLevelComponent() to DesktopPetWindow.
Actual: uses `juce::StandalonePluginHolder::getInstance()->deviceManager`.
Simpler, no dynamic_cast needed.

```cpp
// Original plan (NOT implemented):
auto* petWindow = dynamic_cast<DesktopPetWindow*>(getTopLevelComponent());
return petWindow->pluginHolder->deviceManager;

// What was actually built:
juce::AudioDeviceManager* getDeviceManager() const
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        return &holder->deviceManager;
    return nullptr;
}
```

**3. MenuBarModel: Separate Class → Integrated**

Original plan: separate `GoodMeterMenuBar` class.
Actual: `GoodMeterStandaloneApp` directly implements `MenuBarModel`.
Simpler, fewer indirections.

**4. Settings Menu: Size Toggle + Audio Input → Audio Input + Audio Settings + Quit**

Original plan included Mini/Normal size toggle in gear menu.
Actual: Audio input device list, "Audio Settings..." dialog, Quit.
The mini mode toggle was deferred.

**5. Selective Recall: Orb Re-emission → Simple Recall Animation**

Original plan: stowed cards re-emit as orbs from Nono, replay the full
orbFlyOut→orbDwell→orbWheelToShelf animation for just the stowed cards.
Actual: simpler approach — unstow, set visible, dock, trigger standard
recall animation (cards fly from current position to shelf).

---

## Part 3: Standalone Phase — Bugs, Fixes, and Lessons

### Bug 1: No Audio Input (Silent Meters) — CRITICAL

**Symptom**: All meters at zero, no audio signal visible.

**Root Cause Chain** (3 layers deep):

1. `.jucer` file lacked `microphonePermissionNeeded="1"`
2. Therefore `Info-Standalone_Plugin.plist` lacked `NSMicrophoneUsageDescription`
3. macOS silently denies microphone access without this key — no dialog, no error

**The Trap**: Editing `.jucer` XML alone does NOT propagate changes. Projucer must
be re-saved (`--resave`) to regenerate Xcode project files.

**Fix**:
```xml
<!-- In GOODMETER.jucer <XCODE_MAC> tag: -->
microphonePermissionNeeded="1"
microphonePermissionsText="GOODMETER needs microphone access to analyze audio in real-time."
```
Then: `Projucer --resave GOODMETER.jucer`

**Verification**: `plutil -p Info-Standalone_Plugin.plist | grep micro`

**Extra step**: `tccutil reset Microphone com.solaris.GOODMETER` to force
macOS to re-prompt for permission.

**LESSON**:
```
ALWAYS verify the COMPILED Info.plist after any .jucer permission change.
The .jucer is just XML source — it means nothing until Projucer processes it.
```

### Bug 2: Feedback Loop (Howling/啸叫) — CRITICAL

**Symptom**: Ear-piercing howling immediately on launch with speakers.

**Root Cause**: `processBlock()` passes audio through by default. In standalone
mode: mic → processBlock → speakers → mic → infinite loop.

**Fix**: Clear output buffer after all metering data is extracted.
```cpp
// At END of processBlock(), after all metering calculations:
#if JucePlugin_Build_Standalone
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
#endif
```

**Why not at the beginning?** All metering code reads from the input buffer.
Clearing first would kill the signal before measurement.

**LESSON**:
```
Standalone metering apps must be "listen-only" — analyze input, mute output.
Plugin versions must NOT clear the buffer (audio must pass through the chain).
Use #if JucePlugin_Build_Standalone to guard the mute.
```

### Bug 3: Microphone vs System Audio — ARCHITECTURAL

**Symptom**: Even with mic permission, GOODMETER can only meter microphone input,
not system audio (music from Spotify, DAW output, etc.).

**Root Cause**: This is by design in JUCE's audio architecture. `StandalonePluginHolder`
uses `AudioDeviceManager` which connects to physical hardware I/O only.

**Competitor Analysis**: MiniMeters uses macOS `ScreenCaptureKit` API to capture
system audio output directly. This requires "System Audio Recording" permission
(purple icon in Privacy settings), not "Microphone" (orange icon).

**Current Workaround**: Users must install a virtual loopback device (BlackHole)
and route system audio through it as GOODMETER's input source.

**Future V2.0**: Implement ScreenCaptureKit bridge in Objective-C++ to capture
system audio natively. This is a major feature requiring:
- `NSScreenCaptureUsageDescription` in Info.plist
- ObjC++ bridge code (ScreenCaptureKit API)
- Parallel audio capture path alongside JUCE's AudioDeviceManager

### Bug 4: State Machine Deadlock (Stow → Can't Recall) — CRITICAL

**Symptom**: After stowing cards with Thanos snap, clicking Nono does nothing.

**Root Cause**: Two-layer state machine gap:

Layer 1: `onSmileOrbitTriggered` callback only handled `compact` and `floating`:
```cpp
// BROKEN (original):
if (phase == AnimPhase::compact)      triggerAnimationSequence();
else if (phase == AnimPhase::floating) triggerRecall();
// ❌ settled phase → NOTHING HAPPENS
```

Layer 2: `triggerRecall()` hard-rejects non-floating phases:
```cpp
void triggerRecall() {
    if (phase != AnimPhase::floating) return; // ❌ blocks settled
}
```

**Fix**: Added `promoteSettledToFloating()` bridge method + `triggerSettledRecall()`:

```cpp
// FIXED callback:
holoNono->onSmileOrbitTriggered = [this]()
{
    if (phase == AnimPhase::compact)      triggerAnimationSequence();
    else if (phase == AnimPhase::floating) triggerRecall();
    else if (phase == AnimPhase::settled)  triggerSettledRecall();
};

// New method: promotes settled→floating, then recalls
void triggerSettledRecall()
{
    int stowedCount = 0;
    for (int i = 0; i < numCards; ++i)
        if (cardStowed[i]) stowedCount++;

    if (stowedCount > 0)
        triggerSelectiveRecall();  // includes promoteSettledToFloating()
    else
    {
        promoteSettledToFloating();
        triggerRecall();
    }
}

// Bridge: records screen positions, expands to full screen
void promoteSettledToFloating()
{
    // Records nonoScreenPos + all cardScreenPositions BEFORE expansion
    // Sets phase = floating, expands window to full screen
    // Restores card positions in new coordinate space
}
```

**LESSON**:
```
Every AnimPhase transition must be explicitly handled.
When adding a new phase or modifying callbacks, enumerate ALL phases
and verify each has a valid code path. Dead phases = silent failures.
The state machine has 9 phases — draw a transition diagram before coding.
```

### Bug 5: Menu Bar Design Mismatch — FUNCTIONAL

**Symptom**: Menu showed "File" + "Audio" instead of planned "Recording" menu.

**Root Cause**: First implementation used generic template menus. The original
battle plan specified a single "Recording" menu.

**Fix**: Replaced entire MenuBarModel:
```cpp
juce::StringArray getMenuBarNames() override { return { "Recording" }; }
```

With: Start/Stop Recording, Recent Recordings submenu, Reveal in Finder.

---

## Part 4: Critical JUCE Lessons (Carry Forward to ALL Projects)

### 4.1 paint() Path Iron Rules (from 6-round drag-freeze debugging)

```
=== macOS JUCE Plugin paint() Path Rules ===

1. NEVER use OpenGL in JUCE plugins
   → MessageManagerLock deadlocks with NSEventTrackingRunLoopMode

2. NEVER call drawText() in paint()
   → JUCE 8 HarfBuzz creates CTFont every single call (uncacheable)
   → Pre-render ALL text to SoftwareImageType() offline Images
   → Static text: cache on resize
   → Dynamic text: cache in timerCallback/updateMetrics

3. NEVER draw dense line segments in paint() via CoreGraphics
   → Use BitmapData + Bresenham in timerCallback, paint() only blits

4. ALWAYS use SoftwareImageType() for offline Images
   → NativeImage may use Metal, triggering MML from background threads

5. Timer strategy: 60Hz normal + half-rate during mouse-down
   → if (ModifierKeys::currentModifiers.isAnyMouseButtonDown()) skip odd frames

6. Plugin editor: setOpaque(true) to reduce CA compositing
   → (Standalone desktop pet uses setOpaque(false) for transparency)
```

### 4.2 macOS Transparency + Click-Through

```
=== Desktop Pet Transparent Window Rules ===

1. Use plain Component, NOT DocumentWindow/ResizableWindow
   → DocumentWindow has opaque background layers that can't be disabled

2. hitTest() alone is NOT sufficient for click-through
   → macOS routes clicks by pixel alpha, not hitTest
   → Any glow/shadow pixel (alpha > 0) captures the click
   → Must toggle [NSWindow setIgnoresMouseEvents:] at 60Hz

3. ObjC runtime bridge for ignoresMouseEvents:
   → Use objc_msgSend directly in .cpp (no .mm rename needed)
   → Get NSView from peer->getNativeHandle()
   → Get NSWindow from [NSView window]

4. Guard: don't toggle ignoresMouseEvents during active drags
   → if (windowIgnoringMouse && mouseButtonDown) return;
```

### 4.3 JUCE Project Management

```
=== .jucer / Projucer / Xcode Rules ===

1. The .jucer file is THE source of truth
   → Edit it for: new files, binary resources, module config, permissions

2. ALWAYS re-save with Projucer after .jucer edits
   → CLI: Projucer --resave path/to/Project.jucer
   → This regenerates: project.pbxproj, Info-*.plist, BinaryData

3. Verify COMPILED artifacts, not source templates
   → plutil -p .app/Contents/Info.plist | grep <key>
   → The template plist and the compiled plist can differ

4. BinaryData: images added to .jucer with compile="0" resource="1"
   → Access via: BinaryData::imageName_png, BinaryData::imageName_pngSize
   → Load: juce::ImageCache::getFromMemory(data, size)

5. JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
   → Required in both .jucer JUCEOPTIONS and XCODE_MAC extraDefs
   → Tells JUCE to use our StandaloneApp.cpp instead of default
```

### 4.4 Component Architecture Patterns

```
=== Render Architecture ===

1. Parent paint() runs BEFORE children
   → Children's g.fillAll() overwrites parent painting
   → Rule: Content components must NOT call g.fillAll()

2. Chart overlays > canvas splitting
   → Don't use removeFromRight(38) for scale labels
   → Draw the chart full-width, overlay labels on top with alpha

3. Folded card height: use constants, not getHeight()
   → Float→int rounding causes 1-2px inconsistencies
   → const int foldedH = headerHeight + maxShadowOffset

4. Drag coordinates: always convert to common ancestor
   → event.getEventRelativeTo(contentComponent.get())
   → Never assume iteration order matches display order
```

---

## Part 5: File-by-File Change Summary (Uncommitted as of Mar 7)

| File | Lines Changed | What Changed |
|------|:---:|---|
| StandaloneNonoEditor.h | +2738 | Complete rewrite: 9-phase animation, snap engine, hover buttons, shatter VFX, settled recall fix |
| StandaloneApp.cpp | +254 | Custom app + DesktopPetWindow + "Recording" MenuBarModel |
| MeterCardComponent.h | +300 | Floating drag, undock callbacks, resize grip, detach button, customWidth |
| HoloNonoComponent.h | +182 | Local drag mode (useLocalDrag), onLocalDrag callback, hover callbacks |
| PluginProcessor.cpp | +17 | Audio recording tap, standalone output mute |
| PluginProcessor.h | +4 | AudioRecorder member, include |
| GoodMeterLookAndFeel.h | +6 | accentSoftPink, accentBlue color constants |
| PluginEditor.cpp | +4 | Minor standalone guard |
| GOODMETER.jucer | +10 | microphone permission, PNG assets, AudioRecorder.h |
| Info-Standalone_Plugin.plist | +4 | NSMicrophoneUsageDescription |
| **New: AudioRecorder.h** | ~200 | Lock-free WAV recorder (FIFO + background thread) |
| **New: Assets/*.png** | 3 files | Hover button icons (settings, record, stow) |

---

## Part 6: Known Issues & Future Work

### Current Limitations
1. **Standalone captures microphone only** — no system audio capture yet
2. **No mini mode toggle** — planned in settings menu but deferred
3. **Recording saves to Desktop only** — no configurable output directory
4. **No recording format options** — hardcoded WAV 16-bit (planned 24-bit)

### V2.0 Roadmap
1. **ScreenCaptureKit integration** — system audio capture without loopback
2. **Customizable card arrangement** — save/restore layout presets
3. **Settings persistence** — remember audio device, card positions
4. **Windows support** — transparent window + click-through on Win32

---

## Part 7: How to Pick Up This Project

### For a New AI Assistant

1. Read `ARCHITECTURE.md` first — understand the dual-mode architecture
2. Read this file — understand the development history and pitfalls
3. Read `LESSONS_LEARNED.md` — the 6-round drag-freeze saga is essential context
4. Read `Claude作战书.md` — the original 4-module blueprint with full code samples
5. **Before any code change**, check the AnimPhase state machine diagram
6. **Before any .jucer change**, remember to re-save with Projucer
7. **Before any paint() change**, verify no drawText/drawLine in the hot path
8. **Build command**:
   ```bash
   /Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
     -project Builds/MacOSX/GOODMETER.xcodeproj \
     -scheme "GOODMETER - Standalone Plugin" \
     -configuration Release build
   ```

### Critical Files to Read (in order)
1. `PluginProcessor.h` — public API (atomics, FIFOs, recorder)
2. `StandaloneNonoEditor.h` — the 2800-line mega editor (animation + UI)
3. `StandaloneApp.cpp` — app lifecycle + menu bar
4. `MeterCardComponent.h` — card mechanics (dock, float, snap, resize)
5. `HoloNonoComponent.h` — Nono character animation + interaction
