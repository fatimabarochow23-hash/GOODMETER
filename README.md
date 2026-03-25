# GOODMETER - Professional Audio Metering Suite & Desktop Pet

**Developer**: Solaris Audio
**Version**: 1.0.1
**Formats**: Standalone (macOS), VST3, AU
**Platform**: macOS ARM64 (Apple Silicon)
**Requirements**: macOS 14.2+ (Sonoma) for System Audio Capture; macOS 13+ for all other features

---

## What Is GOODMETER?

GOODMETER is a professional audio metering application with two modes of operation:

**Standalone Mode (Desktop Pet)** — A transparent, borderless, always-on-top desktop companion. An animated character named **Nono** lives on your desktop. Click Nono to deploy 8 professional meter cards that orbit out and dock as a bookshelf. Cards can be freely dragged, snapped into groups, resized, stowed, and recalled. Includes AI-powered noise reduction, live recording, retroactive capture, and video audio extraction.

**Plugin Mode (AU / VST3)** — Runs inside your DAW (Logic Pro, REAPER, Ableton, etc.). Meters the audio signal flowing through the plugin chain. Scrollable multi-card layout with 8 meter modules.

---

## Metering Modules

GOODMETER provides 8 professional meter cards:

| Module | What It Measures |
|--------|-----------------|
| **LEVELS** | Momentary, Short-term, Integrated LUFS (ITU-R BS.1770-4), True Peak, LRA. Includes configurable target overlay with broadcast presets (-14, -16, -23, -24 LUFS) |
| **CLASSIC VU** | Analog-style needle meter with real 300ms ballistics and AffineTransform rotation |
| **3-BAND** | Low (20-250 Hz), Mid (250-2 kHz), High (2k-20 kHz) frequency band energy |
| **SPECTRUM** | Real-time FFT spectrum analyzer (4096-point, log-frequency axis, smooth polygon fill) |
| **PHASE** | Stereo phase correlation (-1.0 to +1.0) with condenser-tube visual indicator |
| **STEREO IMAGE** | Goniometer/Lissajous display + Mid/Side level meters |
| **SPECTROGRAM** | Time-frequency waterfall heatmap (ring-buffer rendering, hover frequency readout) |
| **PSR** | Peak-to-Short-Term Ratio — measures dynamic range and crest factor |

---

## Standalone Features

### Nono (Interactive Character)

Nono is your desktop audio assistant:

- **Deploy Meters**: Click Nono to launch all 8 meter cards with a color-orb animation sequence
- **Recall Meters**: Click Nono again to recall all cards back to the bookshelf
- **Offline Audio Analysis**: Double-click Nono to flip to the back face. Drop any audio file (WAV, MP3, FLAC, AIFF, OGG, M4A) for EBU R128 analysis — Peak dBFS, Momentary Max, Short-Term Max, Integrated LUFS. Supports multichannel files (up to 64 channels) with per-channel breakdown and center channel dialogue loudness
- **Expressions**: Nono responds to audio with facial expressions — idle floating, pupil tracking, smile orbit, shy face (><), dizzy spirals, wink, and special animation states for recording, rewind, and video extraction
- **Overload Explosion**: When True Peak reaches 0 dBFS, Nono triggers a glass shard explosion with liquid splatter VFX

### Card Management

- **Snap Groups**: Drag cards near each other to auto-snap into rigid groups. Cards snap edge-to-edge with fluorescent green guide lines. Groups move, resize, and expand together
- **Resize Snap**: When resizing a card within a group, edges produce Apex-style deceleration damping near alignment targets (24px zone, 0.3x speed). Orange guide lines show the snap target. Release inside the zone to commit to exact alignment
- **Thanos Snap Stow**: Use the Stow button to disintegrate cards with particle VFX. Three stages: (1) stow harbor + folded cards, (2) dock expanded floaters, (3) stow remaining harbor cards
- **Recall**: Nono's exclusive power — click Nono to resurrect stowed cards with fly-back animation

### Hover Buttons (Skill System)

Hover over Nono to reveal 5 action buttons arranged in an Olympic-rings layout. The button loadout is configurable via the Skill Tree (Settings > More Settings > Skills tab):

| Skill | Function |
|-------|----------|
| **Gear** (locked) | Opens settings menu — audio input device, audio source (Mic/System Audio), More Settings dialog |
| **Record** | Start/stop live WAV recording (24-bit). Pulses red when recording. File revealed in Finder on stop |
| **Stow** | Thanos snap — disintegrate visible cards with particle VFX |
| **Rewind** | Save last 60 seconds from the always-on circular buffer to WAV. Nono shows holographic rewind face |
| **Video Extract** | Extract audio track from video files (MP4, MOV, M4V) via AVFoundation. Nono shows amber extraction face |
| **Audio Lab** | Open the AI noise reduction processing window |

Gear is permanently assigned to slot 0. Slots 1-4 can be customized by dragging skills in the Skill Tree dialog.

### Audio Lab (AI Noise Reduction)

Audio Lab provides offline AI-powered noise reduction using **DeepFilterNet3**:

1. **Import**: Drag or browse any audio file (WAV, AIFF, FLAC, MP3, M4A)
2. **Process**: One-click DeepFilterNet3 noise reduction with real-time progress display
3. **Preview**: Three display modes — Waveform, Holo-PSR holographic grid, Spectrogram heatmap. Solo/mute individual channels. Wet/dry blend slider (default 85%)
4. **Room Tone Synthesis**: Automatically detects silent segments via VAD, extracts spectral envelope, and synthesizes matching room tone (adjustable 10-120 second duration)
5. **Export**: Three export modes via menu bar:
   - Export Both: `_clean.wav` + `_roomtone.wav`
   - Export Clean Only: denoised audio
   - Export Room Tone Only: synthesized ambient noise track

All exports are 24-bit WAV at original sample rate and channel count. Solo channel export supported.

**DeepFilterNet3 Technical Details:**
- ONNX Runtime inference (MIT license)
- 48 kHz internal processing (auto-resamples other rates)
- 960-point STFT with Vorbis window, 480-sample hop
- 32-band ERB gain mask + 5-tap deep filter complex FIR
- Per-channel independent processing with chunked inference
- ~4 seconds processing time for 40 seconds of mono audio (Apple Silicon)

### Audio Capture

- **Microphone Input**: Standard hardware audio input for real-time metering
- **System Audio Capture**: CoreAudio Process Tap (macOS 14.2+ Sonoma) captures all system audio output, excluding GOODMETER's own process. Toggle via Settings > Audio Source

### Recording

- **Live Recording**: Lock-free 24-bit WAV recorder with FIFO bridge from audio thread to background writer
- **Retroactive Recording**: Always-on 65-second circular buffer. The Rewind button exports the last 60 seconds to WAV without interrupting audio capture
- **Configurable Save Location**: Set via Settings > More Settings

---

## Installation

### Standalone App

1. Open the DMG file
2. Drag `GOODMETER.app` to your Applications folder
3. Launch from Applications or Spotlight

### VST3 Plugin

Copy `GOODMETER.vst3` to:
```
~/Library/Audio/Plug-Ins/VST3/
```

### AU Plugin

Copy `GOODMETER.component` to:
```
~/Library/Audio/Plug-Ins/Components/
```

After installing plugins, restart your DAW and rescan plugins if necessary.

---

## System Requirements

- **OS**: macOS 13 (Ventura) or later
- **Architecture**: Apple Silicon (ARM64) native
- **System Audio**: Requires macOS 14.2 (Sonoma) or later
- **Disk Space**: ~50 MB (including AI models)
- **RAM**: 200 MB peak during AI noise reduction processing

---

## Source File Map

```
Source/
  PluginProcessor.h/.cpp       DSP engine (shared by plugin + standalone)
  PluginEditor.h/.cpp          Plugin UI (DAW mode, scrollable card layout)
  StandaloneNonoEditor.h       Standalone UI (desktop pet, animation, snap groups)
  StandaloneApp.cpp            Custom JUCEApplication + MenuBarModel
  GoodMeterLookAndFeel.h       Shared colors, fonts, drawStatusDot
  HoloNonoComponent.h          Animated Nono character (expressions, interactions)
  MeterCardComponent.h         Collapsible card (header + content + shadow + resize)
  AudioRecorder.h              Lock-free WAV recorder (FIFO + background thread)
  AudioHistoryBuffer.h         Retroactive 65s circular buffer (lock-free)
  SystemAudioCapture.h/.mm     CoreAudio Process Tap system audio capture
  VideoAudioExtractor.h/.mm    AVFoundation video-to-audio extraction
  SkillTreeComponent.h         N-choose-5 skill loadout UI + persistence
  AudioLabComponent.h          Audio Lab offline processing UI
  DeepFilterProcessor.h        DeepFilterNet3 ONNX inference wrapper
  RoomToneExtractor.h          VAD + spectral envelope + room tone synthesis
  LevelsMeterComponent.h       LUFS/Peak/RMS/LRA meter
  VUMeterComponent.h           Classic analog VU needle meter
  Band3Component.h             3-band frequency meter
  SpectrumAnalyzerComponent.h  FFT spectrum analyzer
  PhaseCorrelationComponent.h  Stereo phase correlation meter
  StereoImageComponent.h       Goniometer/Lissajous + M/S meters
  SpectrogramComponent.h       Time-frequency waterfall display
  PsrMeterComponent.h          Peak-to-Short-Term Ratio meter

ThirdParty/
  DeepFilterNet3_onnx/         Pre-exported ONNX models (enc, erb_dec, df_dec)
  onnxruntime-osx-arm64-1.20.1/ ONNX Runtime 1.20.1 (ARM64 macOS, MIT license)
```

---

## Build Instructions

### Prerequisites

- macOS 14.2+ (ARM64 / Apple Silicon)
- Xcode 16+
- JUCE 8 (with Projucer)

### Build All Targets

```bash
xcodebuild \
  -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - All" \
  -configuration Release \
  -arch arm64 build
```

### Individual Targets

```bash
# Standalone
xcodebuild -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - Standalone Plugin" -configuration Release -arch arm64 build

# VST3
xcodebuild -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - VST3" -configuration Release -arch arm64 build

# AU
xcodebuild -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - AU" -configuration Release -arch arm64 build
```

### Post-build: ONNX Runtime Deployment

The standalone app requires ONNX Runtime and DeepFilterNet3 models in the bundle:

```bash
APP=Builds/MacOSX/build/Release/GOODMETER.app
mkdir -p "$APP/Contents/Frameworks" "$APP/Contents/Resources/DeepFilterNet3_onnx"
cp ThirdParty/onnxruntime-osx-arm64-1.20.1/lib/libonnxruntime.1.20.1.dylib "$APP/Contents/Frameworks/"
cp ThirdParty/DeepFilterNet3_onnx/*.onnx ThirdParty/DeepFilterNet3_onnx/config.ini "$APP/Contents/Resources/DeepFilterNet3_onnx/"
install_name_tool -id @rpath/libonnxruntime.1.20.1.dylib "$APP/Contents/Frameworks/libonnxruntime.1.20.1.dylib"
```

### After Editing .jucer File

Always re-save with Projucer:
```bash
"/path/to/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "GOODMETER.jucer"
```

---

## Key Technical Decisions

- **Standalone output mute**: `buffer.clear()` in processBlock prevents mic-speaker feedback loop
- **No OpenGL**: Avoids MessageManagerLock deadlocks with NSEventTrackingRunLoopMode on macOS
- **No drawText in paint()**: JUCE 8 HarfBuzz creates CTFont every call. All text pre-rendered to offline Images
- **Click-through**: macOS `[NSWindow setIgnoresMouseEvents:]` toggled at 60Hz via ObjC runtime bridge
- **System audio via CoreAudio Process Tap**: `CATapDescription` + aggregate device + IOProc callback
- **Lock-free audio pipeline**: All audio thread paths are allocation-free, mutex-free
- **DeepFilterNet3 inference**: ONNX Runtime with Apple Accelerate vDSP for STFT/ISTFT, chunked processing with GRU state warm-up

---

## Known Limitations

1. macOS only (no Windows/Linux)
2. System audio capture requires macOS 14.2+ (Sonoma)
3. Plugin mode does not include Audio Lab, recording, or system audio capture
4. DeepFilterNet3 is speech-optimized; may produce artifacts on pure music content
5. ONNX Runtime is CPU-only (CoreML provider not yet integrated)

---

## Documentation (For AI Assistants)

| Document | Purpose |
|----------|---------|
| `ARCHITECTURE.md` | Complete technical map: file structure, DSP pipeline, standalone architecture, state machine |
| `DEVELOPMENT_HISTORY.md` | Chronological record: commits, bugs, fixes, JUCE lessons learned |
| `DEEPFILTER_INTEGRATION.md` | DeepFilterNet3 technical integration details |

---

Copyright 2026 Solaris Audio. All rights reserved.
**Last Updated**: 2026-03-11
