# GOODMETER - Project Rules for Claude Code

## Project Overview
GOODMETER is a JUCE 8 / C++17 professional audio metering suite with three platforms:
- **Plugin mode** (AU/VST3): Runs inside DAWs
- **Standalone mode**: macOS desktop pet "Nono" — transparent, borderless, always-on-top
- **iOS mode**: Universal iPhone/iPad app — two-page swipe layout (Nono + Meters)

## Three-Track .jucer Architecture
| Track | File | Project Type | Build Dir |
|-------|------|-------------|-----------|
| Standalone | `GOODMETER.jucer` | audioplug (VST3+AU+Standalone) | `Builds/MacOSX/` |
| Plugin-only | `GOODMETER_Plugin.jucer` | audioplug (VST3+AU) | `Builds/MacOSX_Plugin/` |
| iOS | `GOODMETER_iOS.jucer` | guiapp | `Builds/iOS/` |

**CRITICAL**: All three share one `JuceLibraryCode/` directory. Use `./build.sh <target>` which auto-manages per-target snapshots via `.jlcode_cache/`. NEVER manually `Projucer --resave` without the build script.

## Architecture (Key Files)
- `Source/PluginProcessor.h/.cpp` — DSP engine, atomic meter values, AudioRecorder
- `Source/PluginEditor.h/.cpp` — Plugin UI (DAW mode only)
- `Source/StandaloneApp.cpp` — Custom JUCEApplication + DesktopPetWindow + macOS menu bar
- `Source/StandaloneNonoEditor.h` — 2800+ line standalone UI: 9-phase animation state machine, card system, shatter VFX, hover buttons, snap engine
- `Source/HoloNonoComponent.h` — Nono character: expressions, flip states, ear interaction, orbit animation, drag, analysis
- `Source/MeterCardComponent.h` — 8 meter cards: LUFS, Peak, RMS, Spectrogram, Correlation, PSR, Dynamics, Levels
- `Source/PsrMeterComponent.h` — PSR meter with recording-aware segmented waveform
- `Source/GoodMeterLookAndFeel.h` — Neo-Brutalism LookAndFeel (pure white + ink black)
- `Source/AudioRecorder.h` — Lock-free FIFO recording engine with background WAV writer
- `Source/iOS/` — iOS-specific files (see iOS section below)

## 8 Meter Cards
LUFS-I, LUFS-S, Peak, RMS, Spectrogram, Correlation, PSR (Peak-to-Short), Dynamics, Levels

## Visual Style: Neo-Brutalism
- Background: pure white (#FFFFFF) or off-white (#F4F4F6)
- All text, borders, lines: ink black (#2A2A35)
- Thick borders: 2.5-3.0f on controls
- Corners: max radius 2.0f (near-zero)
- Press interaction: color inversion (black bg, white text)
- No gradients, no soft shadows, no glow on UI chrome
- Nono itself retains holographic/neon aesthetic (magicPink, techCyan)

## Build
**默认始终使用 Release 构建**，只有用户明确要求 Debug 时才切换。

### Recommended: Use build.sh (handles JuceLibraryCode isolation)
```bash
./build.sh standalone   # macOS Standalone + AU/VST3
./build.sh plugin       # Plugin-only (AU/VST3)
./build.sh ios          # iOS device (arm64)
./build.sh ios-sim      # iOS Simulator (arm64)
./build.sh all          # All targets safely
./build.sh resave <t>   # Just resave a .jucer (standalone|plugin|ios)
```

### Direct xcodebuild (if needed)
```bash
xcodebuild -project Builds/MacOSX/GOODMETER.xcodeproj -scheme "GOODMETER - Standalone Plugin" -configuration Release build
```
App output: `Builds/MacOSX/build/Release/GOODMETER.app`

## Critical Rules
- DO NOT codesign, notarize, or run any signing/notarytool commands unless the user explicitly asks for it. Testing builds don't need signing.
- DO NOT modify PluginProcessor::processBlock DSP logic unless explicitly asked
- DO NOT remove the standalone output muting (anti-feedback protection)
- Always preserve the NSMicrophoneUsageDescription in Info plists
- Nono eye line thickness should stay thin (1.0-1.5f max) — user hates thick eyes
- Keep Font deprecated warnings — they are JUCE 8 upstream issues, not our bugs
- The standalone window is fully transparent — extra padding is invisible

## Recording System
- AudioRecorder uses lock-free FIFO (262144 samples) + background writer thread
- `audioProcessor.audioRecorder.getIsRecording()` is the atomic state check
- PSR meter has per-frame recording state in `recHistory[]` for segmented red coloring

## State Machine (StandaloneNonoEditor)
AnimPhase: compact -> orbFlyOut -> orbDwell -> orbWheelToShelf -> cardFadeIn -> settled -> floating -> recalling
- Shatter VFX expands window by 80px during particle flight
- `cancelOrbit()` exists as safety valve for failed animation launches

## GitHub
Repository: https://github.com/fatimabarochow23-hash/GOODMETER.git
Branch: main

## iOS App Architecture (Phase 1 Complete, 2026-03-26)

### Source Files
```
Source/iOS/
├── iOSPluginDefines.h      — Fallback macros (JucePlugin_Name etc.) for guiapp builds
├── iOSAudioEngine.h        — AudioDeviceManager + AudioTransportSource for file playback
├── iOSMainApp.cpp          — JUCEApplication entry point
├── iOSMainComponent.h      — Root two-page swipe container (ViewportPageIndicator)
├── NonoPageComponent.h     — Page 1: Nono/Guoba character + import + play/pause + progress
└── MetersPageComponent.h   — Page 2: 8 meter cards in responsive grid (2-col portrait, 3-col landscape)
```

### Key Design Decisions
- **guiapp project type** (not audioplug) — iOS has no plugin host concept
- **`#if JUCE_IOS` guards** on: PluginProcessor.cpp (iOSPluginDefines include), HoloNonoComponent.h (FileDragAndDrop, nausea tracking), PluginProcessor.cpp (output muting)
- **`analyzeFile()` public API** on HoloNonoComponent — replaces `filesDropped()` for iOS (no drag-drop on mobile)
- **iOSAudioEngine** wraps AudioDeviceManager → feeds audio through PluginProcessor for real-time metering during playback
- **No DeepFilter/ONNX** on iOS (stripped from iOS .jucer)
- **No SystemAudioCapture** on iOS (macOS-only CoreAudio Tap)

### Build Commands (iOS)
```bash
# MUST use DEVELOPER_DIR override (xcode-select points to CommandLineTools)
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer

# Device build
$DEVELOPER_DIR/usr/bin/xcodebuild -project Builds/iOS/GOODMETER.xcodeproj \
  -target "GOODMETER - App" -configuration Release -sdk iphoneos -arch arm64 \
  CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO

# Simulator build
$DEVELOPER_DIR/usr/bin/xcodebuild -project Builds/iOS/GOODMETER.xcodeproj \
  -target "GOODMETER - App" -configuration Release -sdk iphonesimulator -arch arm64 \
  CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
  CONFIGURATION_BUILD_DIR=Builds/iOS/build/Release-iphonesimulator

# Install on simulator
$DEVELOPER_DIR/usr/bin/simctl install booted Builds/iOS/build/Release-iphonesimulator/GOODMETER.app
$DEVELOPER_DIR/usr/bin/simctl launch booted com.solaris.GOODMETER
```

### iOS Current Status (2026-03-26)
- Phase 1 complete: compiles, installs, runs on iPhone 16 Pro simulator (iOS 18.6)
- Nono/Guoba character renders correctly with full sprite + test tube
- Two-page swipe navigation working (page indicators visible)
- Page 2 shows LEVELS + VU + 3-BAND meter cards
- Next: Phase 2 (audio engine wiring for real-time playback metering)

## Current Session Context (复活文档)

### What was being worked on (2026-03-26)
1. **iOS mobile app** — Phase 1 complete. App runs on simulator.
2. **JuceLibraryCode isolation** — Solved via `build.sh` with `.jlcode_cache/` snapshots.
3. **Plugin contamination fix** — Resize grip now only shows when `onResizeSnapQuery` is wired (Standalone only).
4. **Jiggle animation lag fix** — 30Hz drag throttle now skipped during jiggle mode.

### Three-platform build verified
```
./build.sh plugin   → BUILD SUCCEEDED
./build.sh ios-sim  → BUILD SUCCEEDED
./build.sh plugin   → BUILD SUCCEEDED (zero cross-contamination)
```

### Known issues
- Standalone build fails: `onnxruntime_cxx_api.h` not found (pre-existing, needs header path fix)
- iOS meters look slightly blurry (needs Retina resolution investigation)
- `userFilesDirectory="JuceLibraryCode_iOS"` in iOS .jucer is inert (Projucer ignores it)
