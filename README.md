# GOODMETER Professional Audio Metering Plugin

**Company**: Solaris
**Version**: 1.0.0
**Format**: VST3/AU
**Framework**: JUCE

---

## 📊 Project Overview

GOODMETER is a professional audio metering plugin inspired by Mini Meter and iZotope Insight 2. It provides comprehensive loudness and frequency analysis tools for audio production, post-production, and mastering.

### Key Features

- **Multiple Loudness Standards**:
  - EBU R128 (-23 LUFS) - European Broadcasting Union
  - ATSC A/85 (-24 LKFS) - US Television
  - ITU-R BS.1770-4 - International Telecommunications Union
  - AES Streaming (-16 LUFS) - Spotify/Apple Music
  - Custom user-defined targets

- **Metering Modules**:
  - **LEVELS**: Momentary, Short-term, Integrated LUFS + True Peak
  - **CLASSIC VU METER**: Analog-style VU meter with proper ballistics (300ms)
  - **3-BAND METER**: Low (20-250Hz), Mid (250-4kHz), High (4-20kHz)
  - **SPECTRUM ANALYZER**: Real-time FFT frequency analysis (20Hz - 20kHz)
  - **PHASE CORRELATION**: Stereo phase relationship (-1.0 to +1.0)
  - **STEREO IMAGE**: M/L/R/S channel meters + Polar field analyzer
  - **SPECTROGRAM**: Time-frequency waterfall display

---

## 🎨 Design Philosophy

Following professional audio industry aesthetics (Goodhertz, iZotope, FabFilter):
- Deep black backgrounds (#0B0B0D)
- Desaturated professional colors
- Minimal visual clutter
- Maximum data clarity

---

## 📁 Project Structure

```
GOODMETER/
├── Source/              # JUCE C++ source code
│   ├── PluginProcessor.h/.cpp
│   ├── PluginEditor.h/.cpp
│   ├── DSP/            # DSP algorithms
│   │   ├── LUFSMeter.h/.cpp
│   │   ├── TruePeak.h/.cpp
│   │   ├── VUMeter.h/.cpp
│   │   ├── SpectrumAnalyzer.h/.cpp
│   │   ├── PhaseCorrelation.h/.cpp
│   │   └── Spectrogram.h/.cpp
│   └── GUI/            # Custom GUI components
│       ├── MeterCard.h/.cpp
│       ├── LevelsModule.h/.cpp
│       ├── VUMeterModule.h/.cpp
│       └── ...
├── Builds/             # Platform-specific build files
├── docs/               # Documentation
└── reference-ui/       # Gemini's React/TypeScript UI reference
    ├── src/
    │   ├── components/
    │   ├── lib/
    │   └── App.tsx
    └── package.json
```

---

## 🚀 Development Status

### Phase 1: UI Design ✅ COMPLETE
- [x] Gemini designed professional UI in React/TypeScript
- [x] All 7 metering modules designed
- [x] Professional color scheme established

### Phase 2: JUCE Migration 🔄 IN PROGRESS
- [ ] Set up JUCE project
- [ ] Implement DSP algorithms
- [ ] Migrate UI to JUCE C++
- [ ] Connect parameters (APVTS)

### Phase 3: Audio Integration 📅 PLANNED
- [ ] Core Audio system monitoring (macOS)
- [ ] Plugin audio input processing
- [ ] Real-time metering updates

### Phase 4: Polish & Release 📅 PLANNED
- [ ] Code signing & notarization
- [ ] DMG installer creation
- [ ] User manual
- [ ] Beta testing

---

## 🛠 Technical Requirements

- **JUCE Framework**: 7.0.12+
- **C++ Standard**: C++17
- **macOS**: 10.13+
- **Xcode**: 15.0+
- **Audio Standards**: ITU-R BS.1770-4 for LUFS measurement

---

## 📖 Metering Standards Reference

### LUFS (Loudness Units relative to Full Scale)
- **Momentary**: 400ms window, updated every 100ms
- **Short-term**: 3 second window
- **Integrated**: Entire program duration (gated at -70 LUFS)

### True Peak
- 4x oversampling ITU-R BS.1770-4 algorithm
- Detects inter-sample peaks

### VU Meter
- 300ms attack/release time (classic analog ballistics)
- 0 VU = -18 dBFS (broadcast standard)

---

## 📝 License

Copyright © 2026 Solaris Audio
All rights reserved.

---

## 🔗 Related Projects

- **SPLENTA**: Low-frequency enhancement plugin
- **SOLARIS-8**: Organismic synthesizer (LYRA-8 inspired)

---

**Last Updated**: 2026-01-27
**Bundle ID**: com.solaris.GOODMETER
