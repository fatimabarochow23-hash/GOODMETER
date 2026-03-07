# GOODMETER Architecture Reference

> **Purpose**: Complete technical map for any AI assistant (Claude Code, Cursor, Codex)
> picking up this project. Read this FIRST before touching any code.
>
> **Last updated**: 2026-03-07 (v1.0.0 — Standalone Desktop Pet Mode complete)

---

## 1. What Is GOODMETER?

A professional audio metering plugin (VST3/AU) AND a standalone macOS desktop pet app.
Built with JUCE 8 / C++17 on macOS ARM64 (M4 Max, macOS 15.x).

- **Plugin mode** (AU/VST3): Runs inside a DAW (REAPER, Logic, etc.). Meters the audio
  flowing through the plugin chain. Uses `PluginEditor` with a scrollable multi-card UI.
- **Standalone mode**: A transparent, borderless, always-on-top desktop pet. An animated
  character ("Nono") sits on the desktop. Click Nono to reveal 8 meter cards that orbit
  out, then dock as a bookshelf. Cards can be dragged, snapped into groups, stowed with
  a "Thanos snap" VFX, and recalled. Uses `StandaloneNonoEditor`.

**Bundle ID**: `com.solaris.GOODMETER`
**Company**: Solaris
**Build targets**: VST3, AU, Standalone (macOS only currently)

---

## 2. Source File Map

```
Source/
├── PluginProcessor.h/.cpp      — DSP engine (shared by plugin + standalone)
├── PluginEditor.h/.cpp         — Plugin UI (DAW mode, scrollable card layout)
├── StandaloneNonoEditor.h      — Standalone UI (desktop pet, 2800 lines, ALL-IN-ONE)
├── StandaloneApp.cpp           — Custom JUCEApplication (replaces default standalone)
├── GoodMeterLookAndFeel.h      — Shared colors, drawStatusDot, fonts
├── HoloNonoComponent.h         — Animated Nono character (holographic pet)
├── MeterCardComponent.h        — Collapsible card wrapper (header + content + shadow)
├── AudioRecorder.h             — Lock-free WAV recorder (FIFO + background writer)
├── LevelsMeterComponent.h      — LUFS/Peak/RMS/LRA meter
├── VUMeterComponent.h          — Classic analog VU needle meter
├── Band3Component.h            — 3-band frequency meter (LOW/MID/HIGH)
├── SpectrumAnalyzerComponent.h — FFT spectrum analyzer (log frequency)
├── PhaseCorrelationComponent.h — Stereo phase correlation meter
├── StereoImageComponent.h      — Goniometer/Lissajous + M/S meters
├── SpectrogramComponent.h      — Time-frequency waterfall display
└── PsrMeterComponent.h         — Peak-to-Short-Term Ratio meter

Assets/
├── btn_settings.png            — Gear icon for hover button
├── btn_record.png              — Tape/record icon for hover button
└── btn_stow.png                — Shard/stow icon for hover button

nono_icon.png                   — App icon (Nono character)
GOODMETER.jucer                 — Projucer project file (THE source of truth for builds)
```

---

## 3. Build System

### Critical Rule: ALWAYS re-save with Projucer after editing `.jucer`

The `.jucer` file is an XML descriptor. Projucer reads it and generates:
- `Builds/MacOSX/GOODMETER.xcodeproj/project.pbxproj`
- `Builds/MacOSX/Info-*.plist` templates
- BinaryData wrapper for assets

**If you edit the .jucer XML directly, you MUST run:**
```bash
"/path/to/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "GOODMETER.jucer"
```
Otherwise Xcode project files are stale and your changes won't take effect.

### Build commands

```bash
# Full path to xcodebuild (if CommandLineTools is active instead of Xcode):
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - Standalone Plugin" \
  -configuration Release build

# Other schemes:
# "GOODMETER - VST3"
# "GOODMETER - AU"
```

### Key .jucer settings

```xml
JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP="1"  <!-- Uses our StandaloneApp.cpp -->
microphonePermissionNeeded="1"              <!-- NSMicrophoneUsageDescription -->
pluginFormats="buildVST3,buildAU,buildStandalone"
```

---

## 4. DSP Architecture (PluginProcessor)

### Audio Thread (processBlock) — Real-Time Safe

```
Input Buffer (stereo float)
  │
  ├─ Recording Tap → AudioRecorder (lock-free FIFO → background WAV writer)
  │
  ├─ Peak Detection → atomic<float> peakLevelL/R
  ├─ RMS Accumulation → atomic<float> rmsLevelL/R
  ├─ Phase Correlation → atomic<float> phaseCorrelation
  │
  ├─ K-Weighting Filter (ITU-R BS.1770-4)
  │   └─ 400ms circular buffer → Momentary LUFS
  │   └─ 3s circular buffer → Short-Term LUFS
  │   └─ Gated block accumulation → Integrated LUFS
  │
  ├─ 3-Band Filters (IIR: LP@250Hz, BP@250-2k, HP@2kHz)
  │   └─ → atomic<float> rmsLevelLow/Mid/High
  │
  ├─ M/S Encoding → atomic<float> rmsLevelMid/Side
  │
  ├─ FFT Ring Buffer (4096-point, Hann window, 75% overlap)
  │   └─ → LockFreeFIFO for Spectrum, Spectrogram, Stereo Image
  │
  └─ [STANDALONE ONLY] buffer.clear() — mute output to prevent feedback
```

### GUI Thread (timerCallback at 60Hz)

Reads atomic values and feeds to meter components.
**Critical**: Meters only update when `phase != AnimPhase::compact`.

### LRA (Loudness Range)

Calculated on GUI timer thread (every 6 frames = ~100ms):
```
pushShortTermLUFSForLRA() → lraHistory vector (mutex-protected)
calculateLRARealtime() → absolute gate (-70) → relative gate (-20) → percentile 10/95
```

---

## 5. Standalone Architecture (Desktop Pet Mode)

### Window Stack

```
GoodMeterStandaloneApp (JUCEApplication + MenuBarModel)
  └─ DesktopPetWindow (Component, NOT DocumentWindow)
       ├─ Semi-transparent, borderless, always-on-top
       ├─ 60Hz click-through engine (NSWindow setIgnoresMouseEvents)
       └─ StandaloneNonoEditor (AudioProcessorEditor)
            ├─ HoloNonoComponent (animated character)
            ├─ 8 × MeterCardComponent (collapsible cards)
            │   ├─ LevelsMeterComponent
            │   ├─ VUMeterComponent
            │   ├─ Band3Component
            │   ├─ SpectrumAnalyzerComponent
            │   ├─ PhaseCorrelationComponent
            │   ├─ StereoImageComponent
            │   ├─ SpectrogramComponent
            │   └─ PsrMeterComponent
            ├─ Hover Button System (3 PNG icon buttons)
            ├─ Snap Group Engine (edge detection + group management)
            └─ Shatter VFX System (Thanos snap particle physics)
```

### Animation State Machine (9 phases)

```
compact ──[click Nono]──→ orbFlyOut → orbDwell → orbWheelToShelf → cardFadeIn → canvasShrink → settled
                                                                                                  │
settled ──[drag card]──→ floating ←───────────────────────────────────────────────────────────────┘
settled ──[click Nono]──→ floating → recalling → canvasShrink → settled  (if no stowed cards)
settled ──[click Nono]──→ floating → recalling → canvasShrink → settled  (if stowed → selective recall)
floating ─[click Nono]──→ recalling → canvasShrink → settled → compact transition
```

### Click-Through System (macOS)

JUCE's hitTest alone is NOT sufficient for transparent windows. macOS routes clicks
based on pixel alpha — any glow/shadow pixel makes the OS assign the click to our
window. Our fix:

1. `hitTest()` — Pure Euclidean geometry: circle for Nono body, rect for test tube,
   rect for visible cards (minus 8px shadow). Everything else → false.
2. `DesktopPetWindow::timerCallback()` at 60Hz polls global mouse position, runs
   hitTest, toggles `[NSWindow setIgnoresMouseEvents:]` via ObjC runtime.
3. Guard: don't toggle during active drags (prevents drag interruption).

### Snap Group Engine

Cards can be edge-snapped to form groups (like window tiling):
- Detection: 4 edge pairs (L↔R, R↔L, T↔B, B↔T) with 14px threshold
- Secondary axis alignment: auto-aligns to nearest edge
- L-shape snaps: after first snap commits, checks for second perpendicular snap
- Group management: merge, split (BFS connected components), detach
- Width contagion: vertical snaps auto-match expanded card widths

### Hover Button System

3 PNG buttons appear when hovering Nono body:
- Gear (cyan glow) → Audio Settings popup menu
- Tape (pink glow) → Start/Stop WAV recording
- Shard (purple glow) → Thanos snap stow/recall

Animation: staggered fly-out from Nono center, retract when cursor leaves safe zone.

---

## 6. macOS Permissions (CRITICAL)

### Microphone Permission

Required for standalone mode to capture audio input.

```xml
<!-- In GOODMETER.jucer <XCODE_MAC> tag: -->
microphonePermissionNeeded="1"
microphonePermissionsText="GOODMETER needs microphone access..."
```

This generates `NSMicrophoneUsageDescription` in Info.plist. Without it, macOS
**silently denies** microphone access — no dialog, no error, just zero-filled buffers.

**LESSON**: Editing .jucer alone does NOT propagate to Xcode project. You MUST
re-save with Projucer. Or manually edit `Builds/MacOSX/Info-Standalone_Plugin.plist`.

### System Audio Capture (FUTURE - V2.0)

Currently GOODMETER captures microphone input only. To capture system audio output
(like MiniMeters does), would need:
- `ScreenCaptureKit` API (macOS 13+)
- `NSScreenCaptureUsageDescription` permission
- Objective-C++ bridge code
- This is a major feature, not yet implemented

### Feedback Loop Prevention

Standalone mode clears the output buffer after processing:
```cpp
#if JucePlugin_Build_Standalone
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
#endif
```
Without this, mic input → speakers → mic creates howling feedback.

---

## 7. Menu Bar (macOS Native)

`GoodMeterStandaloneApp` implements `juce::MenuBarModel`:
- Single "Recording" menu
- Start/Stop Recording toggle
- Recent Recordings submenu (scans ~/Desktop/GOODMETER_*.wav)
- Reveal in Finder

Audio settings are accessed via Nono's gear hover button (popup menu with
device list + full AudioDeviceSelectorComponent dialog).

---

## 8. Recording System

`AudioRecorder` (in AudioRecorder.h):
- Lock-free FIFO: audio thread pushes samples, never blocks
- Background writer thread: pops from FIFO, writes WAV via juce::WavAudioFormat
- Files saved to ~/Desktop/GOODMETER_YYYYMMDD_HHMMSS.wav
- Started/stopped from GUI thread (hover button or menu bar)
- Sample rate obtained from AudioDeviceManager at start time

---

## 9. Key Constants

```cpp
// StandaloneNonoEditor layout
compactW = 280, compactH = 360     // Nono-only size
expandedW = 1100, expandedH = 1100 // Animation canvas
foldedCardW = 220, foldedCardH = 56
arcRadius = 320.0f                 // Orb orbit radius
numCards = 8

// Hover buttons
hoverBtnSize = 52.0f
hoverBtnGap = 10.0f

// Animation timing (at 60Hz)
flySpeed = 1/18  (~0.3s per card fly-out)
wheelSpeed = 1/65 (~1.08s wheel-to-shelf)
recallSpeed = 1/30 (~0.5s recall)
```

---

## 10. Dependencies

- **JUCE 8** (modules: core, audio_basics, audio_devices, audio_formats,
  audio_plugin_client, audio_processors, audio_utils, data_structures,
  dsp, events, graphics, gui_basics, gui_extra, opengl, analytics, animation)
- **No external libraries** — everything is JUCE + standard C++17
- **Projucer** — for project file management and Xcode project generation
- **Xcode 16.3+** — macOS build toolchain
- **macOS 15.x SDK** (ARM64 native)
