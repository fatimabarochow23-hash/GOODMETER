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

## Source File Map

```
Source/
  PluginProcessor.h/.cpp       DSP engine (shared by plugin + standalone)
  PluginEditor.h/.cpp          Plugin UI (DAW mode, scrollable card layout)
  StandaloneNonoEditor.h       Standalone UI (desktop pet, 2800 lines)
  StandaloneApp.cpp            Custom JUCEApplication + MenuBarModel
  GoodMeterLookAndFeel.h       Shared colors, fonts, drawStatusDot
  HoloNonoComponent.h          Animated Nono character (holographic pet)
  MeterCardComponent.h         Collapsible card (header + content + shadow)
  AudioRecorder.h              Lock-free WAV recorder (FIFO + background thread)
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

- macOS 15.x (ARM64 / Apple Silicon)
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
| `LESSONS_LEARNED.md` | Deep dive: 6-round drag-freeze debugging saga, paint() path rules, performance emergency |

**Reading order for a new AI assistant:**
1. `ARCHITECTURE.md` - understand the dual-mode architecture
2. `DEVELOPMENT_HISTORY.md` - understand history and pitfalls
3. `LESSONS_LEARNED.md` - the performance debugging saga

---

## Key Technical Decisions

- **Standalone output mute**: `buffer.clear()` in processBlock (guarded by `#if JucePlugin_Build_Standalone`) prevents mic-speaker feedback loop
- **No OpenGL**: MessageManagerLock deadlocks with NSEventTrackingRunLoopMode on macOS
- **No drawText in paint()**: JUCE 8 HarfBuzz creates CTFont every call. All text pre-rendered to offline Images
- **Custom standalone app**: `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1` enables our StandaloneApp.cpp
- **Click-through**: macOS `[NSWindow setIgnoresMouseEvents:]` toggled at 60Hz via ObjC runtime bridge
- **Microphone only**: Currently captures hardware mic input. System audio capture (ScreenCaptureKit) is a V2.0 feature

---

## Known Limitations

1. Standalone captures microphone only (no system audio capture yet)
2. Recording saves to ~/Desktop only (no configurable output directory)
3. Recording format is WAV 16-bit only
4. macOS only (no Windows support yet)

---

## Related Projects

- **SPLENTA**: Low-frequency enhancement plugin
- **SOLARIS-8**: Organismic synthesizer (LYRA-8 inspired)

---

Copyright 2026 Solaris Audio. All rights reserved.
**Last Updated**: 2026-03-07
