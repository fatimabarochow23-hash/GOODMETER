# JUCE Audio Plugin Development Skill

> Distilled from GOODMETER (metering suite) + SPLENTA (transient synthesizer) production development.
> Two shipped macOS plugins, 120+ commits, 22 bugs fixed, 6-round performance optimization, $300 Karplus-Strong failure postmortem.
> GitHub: [GOODMETER](https://github.com/fatimabarochow23-hash/GOODMETER) | [SPLENTA](https://github.com/fatimabarochow23-hash/SPLENTA)

---

## Appendix A: Full Commit Timeline

### GOODMETER (49 commits, Feb 27 - Mar 26, 2026)

```
fa40d0a  Initial project structure
5ad3116  V0.1.0: DSP Foundation (peak, RMS, LUFS BS.1770-4, K-weighting, lock-free FIFO)
7fe5362  V0.2.0: UI Foundation (LookAndFeel, MeterCardComponent)
b0bde02  V0.3.0: LevelsMeterComponent
c02c4fd  V0.4.0: Phase Correlation Condenser Tube
9caba45  V0.4.1: CRITICAL Crash Fix — Dangling Pointer (setContentComponent ownership)
822f05c  V0.4.2: FFT memory error fix
261b846  V0.5.0: MeterCardComponent rewrite (parent paint BEFORE children lesson)
78ce7f2  V0.6.0: Classic VU Meter (AffineTransform needle)
1489bfc  Fix: VU coordinate system (AffineTransform rotation)
1b1b1e6  Fix: VU dB double-conversion + Levels text 6Hz cache
e56305c  FFT Spectrum Analyzer (log freq + smooth polygon)
5527c9e  SpectrogramComponent (ring buffer rendering)
f97427d  V0.8.0: Responsive dual-column layout
858f245  Fix: VU clipping (dual-dimension radius protection)
cb07dee  Fix: Coordinate system chaos in LevelsMeter
66c90e8  Responsive layout rewrite + NONO bubble adaptive
d87100a  Flex-Grow elastic layout + 2-column 4v5 balance
fa7130e  NONO explosion + spectrogram overlay + fillAll cleanup
a1e30b0  PERF: NONO offline 40s→4s + handlePaint 387→43 samples + drawText 312→0
e93cc74  PERF: 6-round drag-freeze optimization complete + Mini grid
5c0014e  Standalone desktop pet Phase 1 (transparent Nono, click-through)
237ef20  Standalone Phase 2 (hover UI + recording + Thanos snap + mic permission + anti-feedback)
2219e88  Neo-Brutalism UI + PSR recording segments + Shatter overflow fix
78fb8f1  Snap physics hardening + Stereo responsive + dB contrast fix
eb57cbc  State machine 3-phase refactor + pupil tracking + recording grid + Nono clipping fix
568b5e3  Video extraction (WAV) + system audio capture + rewind recording + amber animation
26a8041  Audio Lab (DeepFilterNet3) + Skill Tree + mode-aware PopupMenu + waterfall sharpening
```

### SPLENTA (80+ commits, Nov 27, 2025 - Dec 26, 2025)

```
5d9f211  Initial: V18.3 professional Min/Max peak detection scope
b82f0d9  V18.5: Professional EnvelopeView with peak aggregation
601344b  V18.6: UI migration begins
01716f4  Batch 00: Initial layout + theme system
10052e1  Batch 01: THEME external change synchronization
fb5ff1c  Batch 02: Custom LookAndFeel (knob & fader interactive feedback)
2dfb77f  Batch 03: UI reorder + knob value display + FFT removal
01c885a  Batch 04: Energy Topology (Mobius Visualizer)
dce6911  Batch 05: Cartesian Trek & Panel Architecture
be86168  Batch 05.5: Visual Alignment & Theme Backgrounds
373860a  Batch 06: Custom Controls (Waveform & Split-Toggle)
168d840  Remove SPLENTA logo header
169632f  Batch 09: SubForge rebranding
3e4bd4a  V19.0: UI Migration Complete
f6b2243  V19.3: MIDI Mode Complete
824e232  V19.3: Retriggerable Envelope (Hard/Soft modes)
f04801d  V19.4: MIDI Display + Performance Optimization
c2cf349  V19.4: A/B Comparison with 3D Pyramid Animation
c6d5039  V19.5: Rainbow color cycling pyramid
dbaf10f  V1.0.0: Public release
0cba249  V19.0 release tag
eef63c9  EnergyTopology V19.5 update
--- christmas-experiment branch ---
f35638a  Backup V20.0-V20.6 failed Karplus-Strong ($300, 16 iterations)
df0c716  V19.23 PPAP Rollback: Restore 24-Harmonic Additive Piano
07e6596  V19.25 PPAP: Fix sustain logic (harmonicEnv only decays on release)
```

---

## Skill 1: JUCE 8 / C++17 Audio Plugin Architecture

### 1.1 Project Setup (.jucer)

```xml
<JUCERPROJECT
  name="MY_PLUGIN"
  companyName="Solaris"
  bundleIdentifier="com.solaris.MY_PLUGIN"
  pluginManufacturerCode="SLRS"
  pluginCode="MyPl"
  version="1.0.0"
  pluginFormats="buildVST3,buildAU,buildStandalone">

  <!-- Custom standalone (if desktop pet / special window needed) -->
  <JUCEOPTIONS JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP="1"/>

  <!-- Third-party linking (e.g. ONNX Runtime) -->
  <extraLinkerFlags>
    -L../../ThirdParty/lib -lmylib -rpath @executable_path/../Frameworks
  </extraLinkerFlags>
  <headerPath>../../ThirdParty/include</headerPath>

  <!-- macOS frameworks -->
  <extraFrameworks>CoreMedia,CoreAudio,AVFoundation,Accelerate</extraFrameworks>

  <!-- Permissions -->
  <customPList>
    <key>NSMicrophoneUsageDescription</key>
    <string>Needs microphone access for real-time audio analysis.</string>
  </customPList>

  <CONFIGURATION macOSDeploymentTarget="14.2"/>
</JUCERPROJECT>
```

**Iron Rule**: After ANY `.jucer` edit, MUST re-save with Projucer:
```bash
Projucer --resave MyPlugin.jucer
```
Then verify compiled artifact:
```bash
plutil -p Builds/MacOSX/Info-Standalone_Plugin.plist | grep NSMicrophoneUsageDescription
```

### 1.2 Real-Time Safe Audio Pipeline

**processBlock() must be lock-free**: zero allocations, zero mutex, zero blocking I/O.

```cpp
void processBlock(AudioBuffer<float>& buffer, MidiBuffer&) override
{
    juce::ScopedNoDenormals noDenormals;  // Prevent denormal slowdown
    const int numSamples = buffer.getNumSamples();

    // 1. Read input (before any modification)
    const float* dataL = buffer.getReadPointer(0);
    const float* dataR = buffer.getReadPointer(1);

    // 2. All DSP: atomics only, no allocation
    float maxL = 0.0f, maxR = 0.0f;
    for (int i = 0; i < numSamples; ++i) {
        maxL = std::max(maxL, std::abs(dataL[i]));
        maxR = std::max(maxR, std::abs(dataR[i]));
    }
    peakLevelL.store(maxL, std::memory_order_relaxed);
    peakLevelR.store(maxR, std::memory_order_relaxed);

    // 3. Lock-free FIFO for GUI data (FFT, waveform)
    fifo.push(dataL, dataR, numSamples);

    // 4. Recording tap (lock-free FIFO to background writer thread)
    if (audioRecorder.getIsRecording())
        audioRecorder.pushSamples(dataL, dataR, numSamples);

    // 5. Standalone output mute (anti-feedback)
    #if JucePlugin_Build_Standalone
        buffer.clear();
    #endif
}
```

**Data flow to GUI**: Audio thread writes atomics/FIFO -> GUI timer reads at 60Hz.
```cpp
// GUI component:
startTimerHz(60);
void timerCallback() override {
    float peak = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
    updateMeter(peak);
    repaint();
}
```

### 1.3 Lock-Free FIFO (SPSC Ring Buffer)

```cpp
template <typename T, size_t Size>
class LockFreeFIFO {
    std::array<std::array<T, 2048>, Size> buffer;
    std::atomic<size_t> writeIndex{0}, readIndex{0};
public:
    bool push(const T* data, int numSamples) {
        size_t wi = writeIndex.load(std::memory_order_relaxed);
        size_t next = (wi + 1) % Size;
        if (next == readIndex.load(std::memory_order_acquire)) return false; // Full
        std::copy(data, data + numSamples, buffer[wi].begin());
        writeIndex.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T* dest, int numSamples) {
        size_t ri = readIndex.load(std::memory_order_relaxed);
        if (ri == writeIndex.load(std::memory_order_acquire)) return false; // Empty
        std::copy(buffer[ri].begin(), buffer[ri].begin() + numSamples, dest);
        readIndex.store((ri + 1) % Size, std::memory_order_release);
        return true;
    }
};
```

### 1.4 DSP Building Blocks (Proven Patterns)

**K-Weighting (ITU-R BS.1770-4 LUFS)**:
- High-shelf @ 1500Hz +4dB (hearing curve compensation)
- High-pass @ 38Hz (rumble rejection)
- 400ms window -> Momentary LUFS
- 3s window -> Short-Term LUFS
- Gated block accumulation -> Integrated LUFS (absolute gate -70dB, relative gate -20dB)

**3-Band Frequency Split**:
- LOW: 4th-order Butterworth LP @ 250Hz
- MID: 4th-order Butterworth BP 250Hz-2kHz (cascaded HP+LP)
- HIGH: 4th-order Butterworth HP @ 2kHz

**FFT Pipeline**:
- 4096-point FFT, Hann window, 75% overlap (1024 hop)
- Lock-free FIFO from audio thread to GUI
- Log-frequency axis for spectrum display

**Phase Correlation**:
```cpp
correlation = sum(L[n] * R[n]) / sqrt(sum(L^2) * sum(R^2));
// Range: -1.0 (out of phase) to +1.0 (mono)
```

**Dual-Oscillator Synthesis (SPLENTA pattern)**:
```cpp
// Clean: sin/tri/saw/square
// Dirty: waveshaping (soft clip + asymmetry)
float drive = 1.0f + 4.0f * colorAmount;
float dirty = clean * drive;
dirty = dirty / (1.0f + std::abs(dirty));      // Soft clip
dirty += 0.15f * colorAmount * dirty * dirty;   // Asymmetry
float mix = clean * (1-color) + dirty * color;  // Crossfade
```

**Additive Synthesis (24-Harmonic PPAP Piano)**:
```cpp
for (int n = 1; n <= 24; ++n) {
    float inharm = 1.0f + 0.0001f * n * n;  // Piano inharmonicity
    sample += sin(phase * n * inharm) * harmonicAmps[n-1] * harmonicEnv[n-1];
    if (isReleasing) harmonicEnv[n-1] *= harmonicDecay[n-1];
}
// Per-harmonic decay: higher harmonics fade faster
// harmonicDecay[n] = 0.9995 - n * 0.00002
```

### 1.5 Recording System (Lock-Free WAV Writer)

```
Audio Thread: pushSamples() -> Lock-free FIFO (262K samples, ~5.5s @ 48kHz)
Background Thread: drains FIFO -> writes 24-bit WAV
Stop: isRecording.store(false) -> flush -> writer.reset() -> fsync(dir)
```

Key details:
- FIFO overflow detection (log warning, don't block audio thread)
- fsync directory after close (forces Finder to show file immediately)
- Naming: `PLUGIN_YYYYMMDD_HHMMSS.wav`

### 1.6 Retroactive Recording (Circular Buffer)

```
65-second circular buffer @ 48kHz = ~3M samples
Always running in processBlock (lock-free memcpy push)
"Rewind" button: snapshot-copies last N seconds -> async WAV writer
Zero allocation on audio thread
```

---

## Skill 2: Plugin to Standalone Software

### 2.1 Custom Standalone App (Desktop Pet Pattern)

When `JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1`, provide `StandaloneApp.cpp`:

```cpp
class MyStandaloneApp : public juce::JUCEApplication, public juce::MenuBarModel
{
    std::unique_ptr<DesktopPetWindow> mainWindow;

    void initialise(const String&) override {
        // Critical: un-mute microphone input
        if (auto* holder = juce::StandalonePluginHolder::getInstance()) {
            holder->deviceManager.setCurrentAudioDeviceType("CoreAudio", true);
            // Override default input muting
        }
        mainWindow = std::make_unique<DesktopPetWindow>(...);
        juce::MenuBarModel::setMacMainMenu(this);
    }

    // Native macOS menu bar
    StringArray getMenuBarNames() override { return {"Recording", "Settings"}; }
};
```

### 2.2 Transparent Borderless Window (macOS)

```cpp
class DesktopPetWindow : public juce::Component, public juce::Timer
{
    DesktopPetWindow() {
        setOpaque(false);
        addToDesktop(
            juce::ComponentPeer::windowIsTemporary |
            juce::ComponentPeer::windowIgnoresKeyPresses
        );
        // Make always-on-top
        setAlwaysOnTop(true);

        startTimerHz(60);  // Click-through polling
    }

    void paint(Graphics& g) override {
        g.fillAll(juce::Colours::transparentBlack);
        // Only draw visible elements (character, cards, etc.)
    }
};
```

### 2.3 Click-Through System (macOS Native Bridge)

JUCE `hitTest()` alone is INSUFFICIENT. macOS routes clicks by pixel alpha.

```cpp
// 60Hz polling in timerCallback:
void timerCallback() override {
    auto mousePos = juce::Desktop::getMousePosition();
    auto localPos = getLocalPoint(nullptr, mousePos);
    bool shouldIgnore = !hitTest(localPos.x, localPos.y);

    // Guard: don't toggle during active drag
    if (isDragging) return;

    setNativeIgnoreMouse(shouldIgnore);
}

void setNativeIgnoreMouse(bool ignore) {
    if (auto* peer = getPeer()) {
        auto nsView = reinterpret_cast<id>(peer->getNativeHandle());
        auto nsWindow = objc_msgSend(nsView, sel_registerName("window"));
        objc_msgSend(nsWindow, sel_registerName("setIgnoresMouseEvents:"),
                     static_cast<BOOL>(ignore));
    }
}
```

### 2.4 Platform-Specific Code (PIMPL Pattern)

Hide Objective-C++ behind pure C++ interface:

```cpp
// SystemAudioCapture.h (pure C++, include anywhere)
class SystemAudioCapture {
public:
    void startAsync(double sampleRate);
    void stop();
    bool isActive() const;
    int readSamples(float* destL, float* destR, int maxSamples);
private:
    std::unique_ptr<Impl> pImpl;  // Forward-declared
};

// SystemAudioCapture.mm (Obj-C++, CoreAudio hidden)
struct SystemAudioCapture::Impl {
    AudioObjectID tapID, aggregateDeviceID;
    AudioDeviceIOProcID ioProcID;
    juce::AbstractFifo ringBuffer{131072};
    // All CoreAudio code here
};
```

### 2.5 System Audio Capture (CoreAudio Process Tap, macOS 14.2+)

7-step pipeline:
1. Translate PID -> AudioObjectID
2. Create CATapDescription (stereo global tap, exclude own process)
3. AudioHardwareCreateProcessTap() -> tap in HAL
4. Read tap format (AudioStreamBasicDescription)
5. AudioHardwareCreateAggregateDevice() -> wrap tap as input
6. AudioDeviceCreateIOProcID() -> register callback
7. AudioDeviceStart() -> streaming

**Why not ScreenCaptureKit**: Wrong permission category (orange dot, scary), zombie permission state, mandatory video overhead, forced display selection.

**Permissions**: `NSAudioCaptureUsageDescription` in Info.plist (purple dot, friendly).

### 2.6 Standalone Output Mute (Anti-Feedback)

```cpp
// At END of processBlock, after ALL metering:
#if JucePlugin_Build_Standalone
    buffer.clear();  // Prevent mic -> speaker -> mic feedback loop
#endif
// MUST be at end, not beginning (metering reads input buffer first)
```

---

## Skill 3: Apple Codesign + Notarization Flow

### 3.1 Prerequisites

```bash
# Certificate: "Developer ID Application: Your Name (TEAM_ID)"
security find-identity -v -p codesigning

# Store notarization credentials
xcrun notarytool store-credentials "notary-profile" \
  --apple-id "your@email.com" \
  --team-id "TEAM_ID" \
  --password "app-specific-password"
```

### 3.2 Build (Always Release)

```bash
xcodebuild -project Builds/MacOSX/PLUGIN.xcodeproj \
  -scheme "PLUGIN - Standalone Plugin" \
  -configuration Release clean build
```

### 3.3 Post-Build: Bundle Dependencies

```bash
APP="Builds/MacOSX/build/Release/PLUGIN.app"

# Create directories
mkdir -p "$APP/Contents/Frameworks"
mkdir -p "$APP/Contents/Resources/Models"

# Copy dylib
cp ThirdParty/lib/libmylib.dylib "$APP/Contents/Frameworks/"

# Fix dylib ID
install_name_tool -id @rpath/libmylib.dylib \
  "$APP/Contents/Frameworks/libmylib.dylib"

# Copy resources (ONNX models, config, etc.)
cp ThirdParty/Models/*.onnx "$APP/Contents/Resources/Models/"
```

### 3.4 Code Signing

```bash
SIGN_ID="Developer ID Application: Your Name (TEAM_ID)"

# 1. Sign dylibs FIRST (inside-out signing)
codesign --force --sign "$SIGN_ID" --timestamp \
  "$APP/Contents/Frameworks/libmylib.dylib"

# 2. Sign the app bundle (with hardened runtime)
codesign --force --sign "$SIGN_ID" --timestamp --options runtime --deep \
  "$APP"

# 3. Verify
codesign --verify --deep --strict "$APP"
```

### 3.5 Notarization

```bash
# Create zip for submission
cd Builds/MacOSX/build/Release
ditto -c -k --keepParent PLUGIN.app /tmp/PLUGIN.zip

# Submit
xcrun notarytool submit /tmp/PLUGIN.zip \
  --keychain-profile "notary-profile" --wait

# Staple (embeds ticket in app)
xcrun stapler staple "$APP"
```

### 3.6 DMG Packaging (Professional Installer)

```bash
# 1. Create background image (Python/PIL, 1280x800 @2x Retina)
# - Dark gradient + grid
# - Gold arrow from app position to Applications
# - Dashed circles at icon positions
# - "Drag to Applications to install" text

# 2. Create read-write DMG
hdiutil create -size 100m -fs HFS+ -volname "PLUGIN v1.0.0" /tmp/rw.dmg

# 3. Mount and populate
hdiutil attach /tmp/rw.dmg
VOLUME="/Volumes/PLUGIN v1.0.0"
cp -R "$APP" "$VOLUME/"
ln -s /Applications "$VOLUME/Applications"

# 4. Set background + icon positions via AppleScript
mkdir "$VOLUME/.background"
cp /tmp/dmg_background.png "$VOLUME/.background/background.png"

osascript <<'APPLESCRIPT'
tell application "Finder"
  tell disk "PLUGIN v1.0.0"
    open
    set current view of container window to icon view
    set toolbar visible of container window to false
    set the bounds of container window to {100, 100, 740, 500}
    set theViewOptions to icon view options of container window
    set icon size of theViewOptions to 96
    set arrangement of theViewOptions to not arranged
    set position of item "PLUGIN.app" to {180, 200}
    set position of item "Applications" to {460, 200}
    close
  end tell
end tell
APPLESCRIPT

# 5. Detach and convert to compressed read-only
hdiutil detach "$VOLUME"
hdiutil convert /tmp/rw.dmg -format UDZO \
  -o "/Users/you/Desktop/PLUGIN_v1.0.0.dmg"
```

### 3.7 VST3/AU Plugin Signing

```bash
# VST3
VST3="Builds/MacOSX/build/Release/PLUGIN.vst3"
codesign --force --sign "$SIGN_ID" --timestamp --options runtime --deep "$VST3"

# AU Component
AU="Builds/MacOSX/build/Release/PLUGIN.component"
codesign --force --sign "$SIGN_ID" --timestamp --options runtime --deep "$AU"

# Create zip containing all formats for notarization
ditto -c -k --keepParent "$VST3" /tmp/plugin_vst3.zip
xcrun notarytool submit /tmp/plugin_vst3.zip --keychain-profile "notary-profile" --wait
xcrun stapler staple "$VST3"
```

---

## Skill 4: Bug Patterns & Avoidance

### 4.1 CRITICAL: macOS paint() Path Iron Rules

From 6-round drag-freeze debugging (handlePaint 65.6% -> 1.8%):

| Rule | Why | Fix |
|------|-----|-----|
| **No OpenGL** | MML deadlocks with NSEventTrackingRunLoopMode | Software rendering only |
| **No drawText() in paint()** | JUCE 8 HarfBuzz creates CTFont every call (uncacheable) | Pre-render to SoftwareImageType() Image |
| **No drawLine() loops in paint()** | CoreGraphics storm (512 CG calls per frame) | BitmapData + Bresenham in timerCallback |
| **Use SoftwareImageType()** | NativeImage may use Metal -> MML from background | Always specify SoftwareImageType() |
| **60Hz + half-rate on drag** | Reduce CA compositing pressure during window ops | `if (mouseDown) skip odd frames` |
| **setOpaque(true) for plugins** | Reduces CA alpha compositing overhead | Standalone pets use false |

**Text Caching Pattern**:
```cpp
// Static text: cache on resize
if (textCache.isNull() || lastCacheW != getWidth()) {
    textCache = Image(ARGB, w, h, true, SoftwareImageType());
    Graphics tg(textCache);
    tg.drawText("TITLE", ...);  // Only here, never in paint()
    lastCacheW = getWidth();
}
// paint(): just blit
g.drawImageAt(textCache, 0, 0);

// Dynamic text: cache in timerCallback
void timerCallback() override {
    if (valueChanged) {
        numberCache = Image(ARGB, w, h, true, SoftwareImageType());
        Graphics tg(numberCache);
        tg.drawText(formatValue(currentValue), ...);
    }
}
void paint(Graphics& g) override {
    g.drawImageAt(numberCache, x, y);  // Zero drawText in paint
}
```

### 4.2 CRITICAL: Standalone Feedback Loop

**Bug**: Mic -> processBlock -> speakers -> mic = ear-piercing howl.
**Fix**: `buffer.clear()` at END of processBlock (after metering reads input).
**Guard**: `#if JucePlugin_Build_Standalone` to preserve plugin pass-through.

### 4.3 CRITICAL: macOS Permission Silent Denial

**Bug**: No `NSMicrophoneUsageDescription` = macOS silently denies mic access. No dialog, no error, just zero audio.
**Fix**: Set `microphonePermissionNeeded="1"` in .jucer, then Projucer --resave.
**Verify**: `plutil -p` the COMPILED Info.plist, not the template.
**Reset**: `tccutil reset Microphone com.solaris.PLUGIN` to force re-prompt.

### 4.4 CRITICAL: State Machine Dead Phases

**Bug**: AnimPhase callback only handles compact + floating, ignores settled -> clicking Nono does nothing.
**Pattern**: Every callback must enumerate ALL phases. Dead phase = silent failure.
**Fix**: Draw transition diagram. Add explicit handler for every phase.
```cpp
// WRONG:
if (phase == compact) doA();
else if (phase == floating) doB();
// settled, recalling, etc. -> NOTHING HAPPENS

// CORRECT:
if (phase == compact)       doA();
else if (phase == floating) doB();
else if (phase == settled)  promoteSettledToFloating(); doB();
else if (phase == recalling) {}  // Intentionally ignored
else jassertfalse;  // Unknown phase = crash in debug
```

### 4.5 HIGH: VU Meter Double-dB Conversion

**Bug**: Processor stores dB values, meter applies log10 again = double logarithm.
**Fix**: Name parameters explicitly (`updateVU_dB`) to signal units.
**Rule**: Document unit conventions (linear 0-1, dB, LUFS) at API boundary.

### 4.6 HIGH: Dangling Pointer on Component Ownership

**Bug**: `setContentComponent()` transfers ownership, then code overwrites same slot with placeholder = deletes original component. timerCallback accesses deleted pointer -> EXC_BAD_ACCESS.
**Fix**: Never overwrite after ownership transfer.
**Rule**: `jassert(card->getContentComponent() == nullptr)` before assigning.

### 4.7 HIGH: JUCE Parent/Child Paint Order

**Bug**: Parent draws border, child's `g.fillAll(white)` overwrites it.
**Rule**: Content components must NEVER call `g.fillAll()`. Parent manages all backgrounds.
**Exception**: Offline Images need `clear()` on initialization.

### 4.8 MEDIUM: Coordinate System Chaos

**Bug**: Using `0.0f` for positions, hardcoded arithmetic sums, symmetric `reduced()` stealing from wrong side.
**Rules**:
- Never use `0.0f` -> always `bounds.getY()`
- Never hardcode `barHeight * 2 + gap` -> use actual component `.getBottom()`
- Asymmetric trimming: `withTrimmedTop(16)` instead of `reduced(0, 16)`
- Anchor lines/labels to real component edges

### 4.9 MEDIUM: Viewport Scroll Reset

**Bug**: `resized()` recalculates contentComponent bounds -> Viewport resets scroll to (0,0).
**Fix**:
```cpp
auto savedPos = viewport->getViewPosition();
// ... all layout calculations ...
viewport->setViewPosition(savedPos);
```

### 4.10 MEDIUM: VU Meter Clipping (Radius Overflow)

**Bug**: Radius calculated from width only. In wide windows, arc extends above component top.
**Fix**: Constrain on BOTH dimensions:
```cpp
float r = jmin(bounds.getWidth() * 0.4f, cy - bounds.getY() - 10.0f);
```

### 4.11 MEDIUM: Irregular Shape Liquid Fill

**Bug**: Manual triangle-similarity width calculation for flask liquid = wrong for curves.
**Fix**: Use clip region:
```cpp
g.reduceClipRegion(vesselPath);
g.fillRect(fullWidthRect);  // Automatically clipped to vessel shape
```

### 4.12 MEDIUM: Folded Card Height Inconsistency (1-2px)

**Bug**: Float-to-int rounding in `card->getHeight()` varies per card.
**Fix**: Use absolute constant: `const int foldedH = headerHeight + maxShadowOffset;`

### 4.13 SPLENTA-SPECIFIC: Karplus-Strong Failure ($300 Postmortem)

16 iterations, 8 bugs found, zero functional improvement. Lessons:
1. **Start from working baseline** (V19.23 additive synthesis worked)
2. **Small incremental changes** (not complete algorithm swap)
3. **Test after each change** (don't stack 16 untested changes)
4. **Keep experiments on branches** (christmas-experiment, not main)
5. **Know when to stop** ($300 spent, rollback to working version)
6. **JUCE Synthesiser quirks**: `clearVoices()` without `clearSounds()` + re-add = voices can't play
7. **AudioParameterChoice**: use `getIndex()` not `(int)load()` (load returns normalized 0-1)
8. **DBG() disabled in Release**: use file logging for Release diagnostics

### 4.14 SPLENTA-SPECIFIC: Emoji in JUCE

**Bug**: JUCE font system can't render emoji Unicode (bell/piano icons show garbled).
**Fix**: Replace with geometric drawing (rectangles + circles to form icon shapes).

### 4.15 Build System Gotchas

| Trap | Prevention |
|------|-----------|
| Edit .jucer but forget Projucer --resave | Permissions/resources silently missing |
| Projucer --resave breaks header search paths | Re-add custom paths after resave |
| Sign app before signing dylibs | Inside-out signing: dylibs first, then bundle |
| Notarize without hardened runtime (--options runtime) | Notarization rejects |
| DMG without Applications symlink | Users can't drag-to-install |
| Missing @rpath for bundled dylibs | App crashes on launch (dylib not found) |

---

## Quick Reference: Build Commands

```bash
# Build (always Release unless user says Debug)
xcodebuild -project Builds/MacOSX/PLUGIN.xcodeproj \
  -scheme "PLUGIN - Standalone Plugin" -configuration Release build

# Resave after .jucer changes
Projucer --resave PLUGIN.jucer

# Sign + Notarize (full flow)
SIGN_ID="Developer ID Application: Name (TEAM_ID)"
APP="Builds/MacOSX/build/Release/PLUGIN.app"
codesign --force --sign "$SIGN_ID" --timestamp "$APP/Contents/Frameworks/*.dylib"
codesign --force --sign "$SIGN_ID" --timestamp --options runtime --deep "$APP"
ditto -c -k --keepParent "$APP" /tmp/plugin.zip
xcrun notarytool submit /tmp/plugin.zip --keychain-profile "notary-profile" --wait
xcrun stapler staple "$APP"

# Reset macOS permission cache
tccutil reset Microphone com.solaris.PLUGIN

# macOS CPU sampling (drag-freeze diagnosis)
sample <PID> 2000 1
# Look at: CA::Transaction::commit > handlePaint > drawText/CoreGraphics
```

---

## Architecture Decision Checklist

Before starting a new JUCE plugin, answer these:

1. **Plugin only or also standalone?** -> If standalone, need custom JUCEApplication
2. **Transparent window?** -> Need click-through system (60Hz NSWindow toggle)
3. **Third-party dylibs?** -> Need @rpath, install_name_tool, inside-out signing
4. **System audio capture?** -> CoreAudio Process Tap (macOS 14.2+), PIMPL pattern
5. **AI/ML inference?** -> ONNX Runtime, bundle models in Resources, background thread
6. **Offline processing?** -> Background thread + AsyncUpdater for GUI updates
7. **Recording?** -> Lock-free FIFO + background writer thread
8. **Complex animations?** -> Hand-rolled interpolation in timerCallback (NOT ComponentAnimator)
9. **Multiple meter views?** -> Pre-render text to Image, overlay labels on charts
10. **macOS menu bar?** -> MenuBarModel integrated into JUCEApplication
11. **iOS mobile app?** -> Separate guiapp .jucer, `#if JUCE_IOS` guards, build.sh for JuceLibraryCode isolation

---

## Skill 5: iOS / Cross-Platform Mobile Porting

### 5.1 iOS Project Setup (guiapp, not audioplug)

iOS has no plugin host concept. Use `projectType="guiapp"` with `defines="JUCE_IOS=1"`:

```xml
<JUCERPROJECT id="GdMtiOS" name="MYAPP" projectType="guiapp"
              defines="JUCE_IOS=1"
              bundleIdentifier="com.company.MYAPP">
  <EXPORTFORMATS>
    <XCODE_IPHONE targetFolder="Builds/iOS"
                  developmentTeamID="TEAM_ID"
                  iosDeploymentTarget="16.0"
                  iosBackgroundAudio="1"
                  extraFrameworks="AVFoundation,AudioToolbox,CoreAudio,Accelerate"/>
  </EXPORTFORMATS>
</JUCERPROJECT>
```

**Key**: Strip macOS-only modules (juce_opengl, juce_audio_plugin_client) from iOS .jucer.

### 5.2 Conditional Compilation Guards

Shared source files need `#if JUCE_IOS` / `#if ! JUCE_IOS` guards:

```cpp
// PluginProcessor.cpp — include fallback macros for guiapp
#if JUCE_IOS
#include "iOS/iOSPluginDefines.h"  // Provides JucePlugin_Name etc.
#endif
#include "PluginProcessor.h"

// HoloNonoComponent.h — no FileDragAndDrop on iOS
class HoloNonoComponent : public juce::Component,
#if ! JUCE_IOS
                           public juce::FileDragAndDropTarget,
#endif
                           public juce::Timer { ... };

// processBlock — no output muting on iOS (no mic feedback scenario)
#if JucePlugin_Build_Standalone && ! JUCE_IOS
    buffer.clear();
#endif
```

### 5.3 iOSPluginDefines.h (Fallback Macros)

guiapp projects don't define `JucePlugin_Name`, `JucePlugin_Desc` etc. Create a fallback:

```cpp
#pragma once
#ifndef JucePlugin_Name
 #define JucePlugin_Name          "MYAPP"
 #define JucePlugin_Desc          "My App Description"
 #define JucePlugin_Manufacturer  "Company"
 #define JucePlugin_VersionString "1.0.0"
#endif
```

### 5.4 Multi-.jucer JuceLibraryCode Isolation

**Problem**: Multiple .jucer files share one `JuceLibraryCode/` directory. Each `Projucer --resave` regenerates it, deleting files from other targets.

**Solution**: `build.sh` with per-target rsync snapshots:
```bash
# Cache after resave
rsync -a --delete JuceLibraryCode/ .jlcode_cache/<target>/

# Restore before build
rsync -a --delete .jlcode_cache/<target>/ JuceLibraryCode/
```

### 5.5 iOS Build Commands

```bash
# xcode-select may point to CommandLineTools — always use DEVELOPER_DIR override
DEVELOPER_DIR=/Applications/Xcode.app/Contents/Developer

# Use -target (not -scheme) + explicit -sdk for iOS
$DEVELOPER_DIR/usr/bin/xcodebuild \
  -project Builds/iOS/MYAPP.xcodeproj \
  -target "MYAPP - App" \
  -configuration Release \
  -sdk iphoneos -arch arm64 \
  CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO

# Simulator: separate output dir to avoid device/sim collision
$DEVELOPER_DIR/usr/bin/xcodebuild \
  -project Builds/iOS/MYAPP.xcodeproj \
  -target "MYAPP - App" \
  -configuration Release \
  -sdk iphonesimulator -arch arm64 \
  CONFIGURATION_BUILD_DIR=Builds/iOS/build/Release-iphonesimulator

# Install + launch on simulator
$DEVELOPER_DIR/usr/bin/simctl install booted Builds/iOS/build/Release-iphonesimulator/MYAPP.app
$DEVELOPER_DIR/usr/bin/simctl launch booted com.company.MYAPP
```

### 5.6 iOS-Specific Gotchas

| Trap | Fix |
|------|-----|
| `filesDropped()` unavailable on iOS | Add public `analyzeFile(const File&)` method |
| `JucePlugin_Name` undefined in guiapp | Include iOSPluginDefines.h before PluginProcessor.h |
| iOS platform "not installed" error | Run `xcodebuild -downloadPlatform iOS` (~8.86 GB) |
| Storyboard compilation fails | Need iOS Simulator runtime installed |
| Device + Simulator builds collide | Use `CONFIGURATION_BUILD_DIR` for separate output |
| `xcrun` not found via CommandLineTools | Use full path: `$DEVELOPER_DIR/usr/bin/simctl` |
| Shared component uses macOS-only API | Guard with `#if ! JUCE_IOS` or runtime callback check |
