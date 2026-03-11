# GOODMETER - Professional Audio Metering Plugin & Desktop Pet

**Company**: Solaris
**Version**: 1.0.0
**Formats**: VST3, AU, Standalone (macOS)
**Framework**: JUCE 8 / C++17
**Platform**: macOS ARM64 (Apple Silicon native)
**Bundle ID**: com.solaris.GOODMETER

---

## What Is GOODMETER?

A professional audio metering suite that operates in two modes:

**Plugin Mode** (AU/VST3): Runs inside DAWs (REAPER, Logic, etc.). Meters audio flowing through the plugin chain. Scrollable multi-card UI with 8 meter modules.

**Standalone Mode** (Desktop Pet): A transparent, borderless, always-on-top macOS desktop pet. An animated character ("Nono") sits on the desktop. Click Nono to reveal 8 meter cards that orbit out and dock as a bookshelf. Cards can be dragged, snapped into groups, stowed with a "Thanos snap" VFX, and recalled.

---

## Metering Modules (8 Cards)

| Module | Description |
|--------|-------------|
| **LEVELS** | Momentary, Short-term, Integrated LUFS (BS.1770-4) + True Peak + LRA |
| **CLASSIC VU** | Analog-style needle meter with AffineTransform rotation, 300ms ballistics |
| **3-BAND** | Low (20-250Hz), Mid (250-2kHz), High (2k-20kHz) frequency analysis |
| **SPECTRUM** | Real-time FFT spectrum analyzer (4096-point, log frequency, smooth polygon) |
| **PHASE** | Stereo phase correlation condenser tube (-1.0 to +1.0) |
| **STEREO IMAGE** | Goniometer/Lissajous display + M/S level meters |
| **SPECTROGRAM** | Time-frequency waterfall display (ring buffer rendering) |
| **PSR** | Peak-to-Short-Term Ratio meter |

---

## Features

### Audio Capture
- **Microphone Input**: Hardware mic capture for real-time metering
- **System Audio Capture**: CoreAudio Process Tap (macOS 14.2+ Sonoma) captures all system audio, excluding own process. Toggled via menu: Audio Source > System Audio

### Recording
- **Live Recording**: Lock-free WAV recorder — FIFO bridge from audio thread to background writer thread. 24-bit WAV format
- **Retroactive Recording**: Always-on 65-second circular buffer. "Save Last 60s" exports the buffer to WAV on a background thread without interrupting audio
- **Configurable Output Directory**: Set recording save location via menu (Recording > Set Recording Location)

### Nono (Interactive Character)
- **Offline Audio Analysis**: Double-click Nono to flip to back face, drop any audio file (WAV/MP3/FLAC/AIFF/OGG) for EBU R128 analysis: Peak dBFS, Momentary Max, Short-Term Max, Integrated LUFS. Multichannel support (up to 64ch) with center channel dialogue loudness
- **Video Audio Extraction**: Extract audio from video files for analysis (via AVFoundation)
- **Expressions**: Idle figure-8 floating, mouse-tracking pupils, smile orbit, shy (><), dizzy spirals, wink, recording grid face, rewind holographic face, amber extraction face
- **Overload Explosion**: Peak >= 0 dBFS triggers glass shard explosion + liquid splatter animation

### Card Management (Standalone)
- **Orbit Animation**: Cards fly out from Nono in a circular orbit, then dock as a bookshelf
- **Snap Groups**: Drag cards near each other to auto-snap into groups with BFS connected components
- **Thanos Snap Stow**: Double-click Nono's test tube to stow all cards with particle disintegration VFX
- **Recall**: Cards fly back to shelf positions with spring animation

---

## Source File Map

```
Source/
  PluginProcessor.h/.cpp       DSP engine (shared by plugin + standalone)
  PluginEditor.h/.cpp          Plugin UI (DAW mode, scrollable card layout)
  StandaloneNonoEditor.h       Standalone UI (desktop pet, ~4100 lines)
  StandaloneApp.cpp            Custom JUCEApplication + MenuBarModel
  GoodMeterLookAndFeel.h       Shared colors, fonts, drawStatusDot
  HoloNonoComponent.h          Animated Nono character (~3000 lines)
  MeterCardComponent.h         Collapsible card (header + content + shadow)
  AudioRecorder.h              Lock-free WAV recorder (FIFO + background thread)
  AudioHistoryBuffer.h         Retroactive 65s circular buffer (lock-free)
  SystemAudioCapture.h/.mm     CoreAudio Process Tap system audio capture
  VideoAudioExtractor.h/.mm    AVFoundation video-to-audio extraction
  LevelsMeterComponent.h       LUFS/Peak/RMS/LRA meter
  VUMeterComponent.h           Classic analog VU needle meter
  Band3Component.h             3-band frequency meter
  SpectrumAnalyzerComponent.h  FFT spectrum analyzer
  PhaseCorrelationComponent.h  Stereo phase correlation meter
  StereoImageComponent.h       Goniometer/Lissajous + M/S meters
  SpectrogramComponent.h       Time-frequency waterfall display
  PsrMeterComponent.h          Peak-to-Short-Term Ratio meter

Assets/
  btn_settings.png             Gear icon for hover button
  btn_record.png               Tape/record icon for hover button
  btn_stow.png                 Shard/stow icon for hover button
```

---

## Build Instructions

### Prerequisites

- macOS 14.2+ (ARM64 / Apple Silicon)
- Xcode 16.3+
- JUCE 8 (with Projucer)

### Build Standalone

```bash
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - Standalone Plugin" \
  -configuration Release build
```

### Build VST3

```bash
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - VST3" \
  -configuration Release build
```

### Build AU

```bash
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
  -project Builds/MacOSX/GOODMETER.xcodeproj \
  -scheme "GOODMETER - AU" \
  -configuration Release build
```

### After editing .jucer file

**Always re-save with Projucer:**
```bash
"/path/to/JUCE/Projucer.app/Contents/MacOS/Projucer" --resave "GOODMETER.jucer"
```
This regenerates Xcode project files, Info.plist templates, and BinaryData.

---

## Documentation (For AI Assistants)

This project includes comprehensive documentation designed for AI code assistants:

| Document | Purpose |
|----------|---------|
| `ARCHITECTURE.md` | Complete technical map: file structure, DSP pipeline, standalone architecture, state machine, build system |
| `DEVELOPMENT_HISTORY.md` | Chronological record: 45 commits, bugs, fixes, battle plan vs reality, JUCE lessons |

**Reading order for a new AI assistant:**
1. `ARCHITECTURE.md` - understand the dual-mode architecture
2. `DEVELOPMENT_HISTORY.md` - understand history and pitfalls

---

## Key Technical Decisions

- **Standalone output mute**: `buffer.clear()` in processBlock (guarded by `#if JucePlugin_Build_Standalone`) prevents mic-speaker feedback loop
- **No OpenGL**: MessageManagerLock deadlocks with NSEventTrackingRunLoopMode on macOS
- **No drawText in paint()**: JUCE 8 HarfBuzz creates CTFont every call. All text pre-rendered to offline Images
- **Custom standalone app**: `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` enables our StandaloneApp.cpp
- **Click-through**: macOS `[NSWindow setIgnoresMouseEvents:]` toggled at 60Hz via ObjC runtime bridge
- **System audio via CoreAudio Process Tap**: `CATapDescription` + aggregate device + IOProc callback. Requires macOS 14.2+ and `NSAudioCaptureUsageDescription` entitlement
- **Lock-free audio pipeline**: All audio thread paths are allocation-free, mutex-free. FIFO bridges to background threads for I/O

---

## Known Limitations

1. macOS only (no Windows/Linux support)
2. System audio capture requires macOS 14.2+ (Sonoma)
3. Standalone mode only — plugin mode does not support system audio capture or recording

---

Copyright 2026 Solaris Audio. All rights reserved.
**Last Updated**: 2026-03-11
