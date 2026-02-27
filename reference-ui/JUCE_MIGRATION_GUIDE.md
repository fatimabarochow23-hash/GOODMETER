# Web Canvas to JUCE VST3 Migration Guide

## ⚠️ SYSTEM DIRECTIVES (CRITICAL)
You are an expert C++ audio developer specializing in the JUCE framework. Your task is to migrate a React + HTML5 Canvas web prototype into a production-ready JUCE VST3/AU plugin. 

**Strict Architectural Rules:**
1. **Thread Safety is Paramount:** You MUST strictly separate the audio thread (`processBlock`) from the GUI thread (`paint`). **NEVER** allocate memory, use locks (`std::mutex`), or call UI functions inside `processBlock`.
2. **Data Communication:** Use Lock-free structures (e.g., `juce::AbstractFifo`, `LockFreeQueue`, or `std::atomic<float>`) to pass RMS, Peak, Phase, and FFT data from the `PluginProcessor` to the `PluginEditor`.
3. **Timer-Driven UI:** Discard web `requestAnimationFrame`. Use `juce::Timer` (e.g., `startTimerHz(60)`) in the Editor/Components to fetch the latest atomic data and trigger `repaint()`.
4. **No Web Audio API:** Do not attempt to emulate web contexts. Write native JUCE DSP (or use `juce::dsp` module) for all analysis.

---

## 📂 FILE-BY-FILE TRANSLATION MAP

### 1. The DSP Backend (Reference Only)
**File:** `src/lib/audio.ts`
* **Web Role:** Uses Web Audio API (`AnalyserNode`, `AudioContext`) to calculate RMS, True Peak, FFT, and Phase Correlation.
* **JUCE Translation:** Treat this purely as a **requirements document**. 
  * Recreate these calculations inside `PluginProcessor.cpp`.
  * Use `juce::dsp::FFT` for the Spectrogram/Spectrum.
  * Implement custom envelope followers (Attack/Release) and mathematical mappings (e.g., Log10 conversions for dBFS) as seen in this file.

### 2. Global Aesthetics & Layout
**File:** `src/index.css`
* **Web Role:** Defines the "Goodhertz-inspired" flat, bold, vector aesthetic via CSS variables (Hex codes, thick borders).
* **JUCE Translation:** * Extract all Hex colors (`#E6335F`, `#FFD166`, `#2A2A35`, etc.).
  * Hardcode these as `juce::Colour::fromString()` in a custom `juce::LookAndFeel_V4` class or as global constants.

**File:** `src/App.tsx`
* **Web Role:** The main vertical Flexbox container.
* **JUCE Translation:** Translate to `PluginEditor.cpp`. Use `juce::FlexBox` or calculate bounds in `resized()` to stack the meter modules vertically.

**File:** `src/components/MeterCard.tsx`
* **Web Role:** A collapsible container with a thick border, white background, a colored status dot, and a bold title.
* **JUCE Translation:** Create a custom `juce::Component` (e.g., `MeterCardComponent`). 
  * Implement `paint()` to draw the thick `juce::Path` border and title text.
  * Implement mouse listeners for the expand/collapse logic, triggering an animated or instant height change and calling `resized()` on the parent.

### 3. Core Canvas Components (The "Meat")
**Files:** `src/components/meters/*.tsx`
* **Web Role:** Contains the math, physics (spring/damping), and HTML5 Canvas drawing commands (`ctx`).
* **JUCE Translation:** Translate each file into a separate `juce::Component` class. Keep the math EXACTLY as it is, but translate the rendering API.

**Specific Component Instructions:**
* **`ClassicVUMeter.tsx`**: Retain the ballistics/smoothing algorithm. Translate the flat arc drawing and thick needle to `juce::Path` and `g.strokePath()`.
* **`PhaseCorrelation.tsx`**: This is a horizontal "Graham Condenser" (chemistry tube). Translate the sine-wave/bezier curve inner tube drawing to `juce::Path::quadraticTo()` or `cubicTo()`.
* **`StereoFieldAnalyzer.tsx` (Chemistry Flasks)**: 
  * **CRITICAL:** In the Web version, `ctx.clip()` is used to prevent the colored liquid from spilling outside the flask paths. 
  * In JUCE, you MUST use `g.reduceClipRegion(flaskPath)` before filling the liquid, then restore the state, and finally draw the thick outer stroke.
* **`StereoImage.tsx` (Goniometer/Lissajous)**: Retain the M/S mapping math. For the phosphor trail effect, consider using `juce::Image` fading or `setBufferedToImage(true)` to avoid massive CPU spikes from drawing thousands of lines per frame.
* **`Spectrogram.tsx`**: Translate the waterfall drawing. You will need to write a sliding `juce::Image` buffer to performantly render the waterfall history.

---

## 📖 Canvas to JUCE API "Rosetta Stone"
When translating `*.tsx` files, apply these API conversions:

| Web Canvas (`ctx`) | JUCE Graphics (`juce::Graphics &g`) |
| :--- | :--- |
| `ctx.fillStyle = '#...'; ctx.fill();` | `g.setColour(juce::Colour::fromString("...")); g.fillPath(path);` |
| `ctx.lineWidth = 4; ctx.stroke();` | `g.strokePath(path, juce::PathStrokeType(4.0f));` |
| `ctx.beginPath(); ctx.moveTo(x,y);` | `juce::Path p; p.startNewSubPath(x, y);` |
| `ctx.lineTo(x,y);` | `p.lineTo(x, y);` |
| `ctx.bezierCurveTo(...)` | `p.cubicTo(...)` |
| `ctx.clip();` | `g.reduceClipRegion(path);` (Remember to save/restore state) |
| `ctx.save(); / ctx.restore();` | `juce::Graphics::ScopedSaveState state(g);` |

---

## 🚀 EXECUTION PLAN FOR CLAUDE
1. **Phase 1: DSP Setup.** Create the atomic variables and FIFOs in `PluginProcessor.h`. Implement RMS, Peak, and basic filtering in `processBlock`.
2. **Phase 2: UI Foundation.** Create the custom `LookAndFeel` and the `MeterCardComponent`. Set up the main Editor layout and the 60Hz Timer.
3. **Phase 3: Component Translation.** Translate the meters one by one, starting with the easiest (`Levels.tsx` / `ClassicVUMeter.tsx`) and ending with the most complex (`StereoFieldAnalyzer.tsx` / `PhaseCorrelation.tsx`).
