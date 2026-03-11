# GOODMETER - Project Rules for Claude Code

## Project Overview
GOODMETER is a JUCE 8 / C++17 professional audio metering suite with two modes:
- **Plugin mode** (AU/VST3): Runs inside DAWs
- **Standalone mode**: macOS desktop pet "Nono" — transparent, borderless, always-on-top

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
```bash
xcodebuild -project Builds/MacOSX/GOODMETER.xcodeproj -scheme "GOODMETER - Standalone Plugin" -configuration Release build
```
App output: `Builds/MacOSX/build/Release/GOODMETER.app`

## Critical Rules
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
