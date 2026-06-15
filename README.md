# GOODMETER

GOODMETER is a macOS audio metering, desktop companion, and analysis toolkit built with JUCE.
It began as a real-time metering suite and has since grown into a standalone desktop app,
AU/VST3 plugin target, offline audio processing tool, and thesis-oriented Audio Doctor
analysis workflow.

- **Current public intro updated:** 2026-06-15
- **Current app version:** 1.0.2
- **Primary platform:** macOS Apple Silicon
- **Formats:** Standalone app, AU, VST3, iOS prototype branch
- **Main research branch:** `codex/thesis-audio-doctor-20260505`

> Note: the default GitHub branch is `main`. The newest Audio Doctor work currently lives on
> `codex/thesis-audio-doctor-20260505` at commit
> `d1c269aa4caef17741afd89349e5e73ccd3a3496`.

## Author Thesis

The author's undergraduate thesis PDF is available in:

[作者毕业论文电子版](./作者毕业论文电子版/)

## What GOODMETER Does

GOODMETER has several connected surfaces:

| Surface | Purpose |
| --- | --- |
| Standalone macOS app | Desktop audio assistant with real-time meters, recording, retroactive capture, video audio extraction, and offline analysis. |
| AU / VST3 plugin | DAW insert version for metering audio inside Logic Pro, REAPER, Ableton Live, and other hosts. |
| Audio Lab | Offline AI-assisted noise reduction, preview, room tone extraction, and WAV export workflow. |
| Audio Doctor | A/B/C dry/wet analysis module for plugin-chain comparison, thesis figures, manifests, and structured exports. |
| iOS prototype | Mobile GOODMETER exploration kept separate from desktop Audio Doctor work. |

## Main Features

### Metering Suite

GOODMETER includes eight core metering cards:

- **Levels**: LUFS, RMS, true peak, LRA, and broadcast-style target overlays.
- **Classic VU**: analog-style VU meter with ballistics.
- **3-Band**: low, mid, and high band energy tracking.
- **Spectrum**: real-time FFT spectrum display.
- **Phase**: stereo phase correlation.
- **Stereo Image**: goniometer / Lissajous view and Mid/Side levels.
- **Spectrogram**: time-frequency waterfall.
- **PSR**: peak-to-short-term ratio for dynamic range inspection.

### Desktop Companion

The standalone app uses an interactive desktop character, Nono, as the control surface.
Nono can deploy and recall floating meter cards, open settings, trigger recording,
save the recent audio buffer, extract audio from video, and open offline processing tools.

Cards can be moved, snapped into groups, resized, stowed, and recalled. The desktop app is
designed for quick audio checks while editing, mixing, watching references, or preparing
sound-design materials.

### Audio Lab

Audio Lab provides offline processing for imported audio files:

- DeepFilterNet3-based noise reduction through ONNX Runtime.
- Wet/dry preview and channel solo controls.
- Waveform, spectrogram, and holographic preview modes.
- Room tone extraction and synthesis from suitable quiet segments.
- 24-bit WAV export for clean audio and room tone.

### Audio Doctor

Audio Doctor is the newer analysis module developed for GOODMETER's research and thesis
workflow. It focuses on comparing dry and processed audio in a structured way rather than
presenting subjective listening results as measurements.

Current Audio Doctor capabilities include:

- DRY / WET A/B/C source loading.
- Preview solo routing for individual dry/wet sources.
- Plugin-chain rendering and parameter summary panels.
- Spectrogram, spectrum, stereo image, masking risk, layer-fit proxy, and group-delay figures.
- Direct group-delay comparison between dry and wet material.
- PNG figure export, JSON manifests, and structured data for paper / presentation use.
- Job Runner support for repeatable batch export outside the UI.

Audio Doctor figures are intended as signal-analysis and visualization evidence. They do not
replace listening, creative judgment, or formal listening-test results.

## Repository Branches

| Branch | Role |
| --- | --- |
| `main` | Default GitHub branch and stable project landing page. |
| `codex/thesis-audio-doctor-20260505` | Latest Audio Doctor thesis branch. |
| `codex/integrate-audio-doctor-20260515` | Integration branch used during Audio Doctor merge work. |

If another machine has newer iOS work, do not blindly reset or pull the whole Audio Doctor
branch over it. Prefer checking diffs first and cherry-picking the specific desktop Audio
Doctor commit when needed.

## Build

The fastest local desktop check is:

```bash
./build.sh standalone
```

The generated standalone app is placed under:

```text
~/Library/Caches/GOODMETERBuild/<build-id>/standalone/Products/Release/GOODMETER.app
```

For Xcode builds, use the generated JUCE projects under:

```text
Builds/MacOSX/
Builds/MacOSX_Plugin/
Builds/iOS/
```

The macOS project expects a local JUCE installation and Apple Silicon build environment.
When moving between machines, verify local JUCE paths before treating project-file changes
as real source changes.

## Source Map

```text
Source/
  StandaloneApp.cpp            Standalone app entry point and Audio Doctor job routing
  StandaloneNonoEditor.h       Desktop companion UI and main standalone controls
  AudioDoctorComponent.h       Audio Doctor UI, source slots, preview, export flow
  AudioDoctorFigureRenderer.h  Audio Doctor figure rendering and metrics output
  AudioDoctorJobRunner.h       Batch job runner and manifest export
  AudioDoctorPluginHost.h      Plugin hosting and offline rendering support
  AudioLabComponent.h          Offline audio processing UI
  DeepFilterProcessor.h        DeepFilterNet3 / ONNX wrapper
  MeterCardComponent.h         Shared meter card shell
  *MeterComponent.h            Individual meter modules
  iOS/                         iOS prototype surface

Assets/                        App icons and UI assets
Builds/                        Generated Xcode projects
ThirdParty/                    ONNX Runtime and DeepFilterNet resources
memory_palace/                 Project handoff notes and long-running task memory
```

## Distribution Notes

GOODMETER has been packaged as notarized macOS DMG / PKG builds during thesis work, but
release artifacts are not the source of truth for development. The source tree, branch,
commit, build script, and export manifests should be checked before distributing a new build.

Before committing from an external drive, avoid staging macOS metadata and build noise:

```text
._*
dist/
tmp/
releases/*.dmg
releases/*.pkg
Signing/
```

## Development Notes

- GOODMETER is a local JUCE/C++ project, not a web app.
- The desktop app and plugin targets share some code, but Audio Doctor is primarily a
  standalone desktop analysis workflow.
- iOS work should be protected on its own branch when pulling desktop Audio Doctor changes.
- Audio Doctor exports should keep UI figures, Job Runner outputs, manifests, and paper
  figures conceptually separate.

## License

Private research / portfolio project unless a separate license file states otherwise.
