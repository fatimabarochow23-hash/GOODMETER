# Audio Doctor App

This folder contains the extracted Audio Doctor macOS app shell.

It is intentionally not a destructive split from GOODMETER. The app reuses the
existing Audio Doctor source files from `../Source/` and keeps GOODMETER,
the plug-in project, and iOS targets untouched.

## Current Shape

- App name: `Audio Doctor`
- JUCE project: `AudioDoctor.jucer`
- App entry point: `Source/AudioDoctorStandaloneApp.cpp`
- Shared core: `../Source/AudioDoctorComponent.h`,
  `../Source/AudioDoctorAnalysis.h`, `../Source/AudioDoctorFigureRenderer.h`,
  `../Source/AudioDoctorPluginHost.h`, and `../Source/AudioDoctorJobRunner.h`
- App icon source: `../AAADOCTOR.PNG`
- `.clz` project icon: `../Assets/audio_doctor_project_pigeon.icns`

## Build

From the GOODMETER repo root:

```bash
./build.sh audio-doctor
```

To regenerate the Xcode project only:

```bash
./build.sh resave audio-doctor
```

The build script uses the same Xcode autodetection as the GOODMETER targets, so
`/Applications/Xcode-26.4.app` is supported through `DEVELOPER_DIR` detection.

## GitHub Extraction Note

This folder is designed to be movable into a future standalone GitHub repository
after the first local build is verified. Until then, it is safer to keep it in
the same repo so the large Audio Doctor core can be reused without copying or
forking behavior.
