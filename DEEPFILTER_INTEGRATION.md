# DeepFilterNet3 Noise Reduction - Technical Integration Document

## Overview

GOODMETER integrates DeepFilterNet3 (DFN3) as an offline noise reduction engine within the Audio Lab module. The system processes imported audio files through a full DFN3 inference pipeline, producing broadcast-quality denoised output while preserving a synthesized room tone track for post-production use.

**Key files:**
- `Source/DeepFilterProcessor.h` — ONNX Runtime inference wrapper
- `Source/RoomToneExtractor.h` — VAD + spectral envelope + room tone synthesis
- `Source/AudioLabComponent.h` — UI integration (import/process/export workflow)
- `ThirdParty/DeepFilterNet3_onnx/` — Pre-exported ONNX models (enc, erb_dec, df_dec)
- `ThirdParty/onnxruntime-osx-arm64-1.20.1/` — ONNX Runtime 1.20.1 (ARM64 macOS)

---

## Architecture

```
Audio File (WAV/AIFF/FLAC/MP3/M4A)
     |
     v
[Import] ── audioData buffer (multi-channel, original sample rate)
     |
     v
[Process] ── background thread
     |
     ├── RoomToneExtractor
     |   ├── 1. VAD: energy + spectral flatness → silent segments
     |   ├── 2. Spectral envelope: FFT on silent segments → avg magnitude
     |   ├── 3. Noise floor RMS measurement (calibration)
     |   └── 4. Synthesis: overlap-add (random phase × spectral envelope)
     |         → roomToneData (multi-channel, matched RMS)
     |
     └── DeepFilterProcessor
         ├── Per-channel independent processing
         ├── Resample to 48kHz (if needed)
         ├── STFT (960-pt Vorbis window, hop 480)
         ├── Feature extraction (ERB 32 bands + complex spec 96 bins)
         ├── Chunked ONNX inference (enc → erb_dec + df_dec)
         ├── ERB mask application (481 bins)
         ├── Deep filtering (5-tap complex FIR, first 96 bins)
         ├── ISTFT (Princen-Bradley overlap-add)
         └── Wet/dry blend → denoisedData
     |
     v
[Export] ── controlled by macOS menu bar (Audio Lab menu)
     ├── _clean.wav   (denoised, 24-bit WAV)
     └── _roomtone.wav (synthesized room tone, 24-bit WAV)
```

---

## DeepFilterNet3 Model Details

### Model Origin
- **Paper:** Schroeter et al., "DeepFilterNet: Perceptually Motivated Real-Time Speech Enhancement" (INTERSPEECH 2022)
- **Repository:** https://github.com/Rikorose/DeepFilterNet
- **Version:** DeepFilterNet3 (third generation)
- **License:** MIT (model weights), Apache-2.0 / MIT (runtime)

### ONNX Model Files

| File | Size | Function |
|------|------|----------|
| `enc.onnx` | 1.9 MB | Encoder: audio features → latent embeddings + skip connections |
| `erb_dec.onnx` | 3.2 MB | ERB decoder: embeddings → 32-band gain mask |
| `df_dec.onnx` | 3.1 MB | DF decoder: embeddings → 5-tap complex filter coefficients + alpha |
| `config.ini` | 4 KB | Training hyperparameters and model architecture config |

### Signal Processing Parameters (from config.ini)

| Parameter | Value | Description |
|-----------|-------|-------------|
| Sample Rate | 48000 Hz | Fixed internal rate; input auto-resampled |
| FFT Size | 960 | 960 = 15 x 2^6, handled by Accelerate vDSP_DFT |
| Hop Size | 480 | 50% overlap (Princen-Bradley condition) |
| FFT Bins | 481 | kFFTSize/2 + 1 |
| ERB Bands | 32 | Equivalent Rectangular Bandwidth partition |
| DF Bins | 96 | Deep filtering applied to first 96 bins only |
| DF Order | 5 | 5-tap complex FIR filter |
| DF Lookahead | 2 | 2 frames lookahead for causal filtering |
| Norm Tau | 1.0 s | EMA time constant for feature normalization |
| Norm Alpha | 0.99005 | exp(-hop / (tau * sr)) |

---

## DeepFilterProcessor Implementation

### STFT/ISTFT
- Uses Apple Accelerate `vDSP_DFT_zop` for forward/inverse DFT (not power-of-2 FFT)
- Vorbis window: `sin(pi/2 * sin^2(pi * (n+0.5) / N))`
- ISTFT uses Princen-Bradley property: no window-sum normalization needed (analysis x synthesis overlap-add = unity)

### Feature Extraction (matching Rust reference libDF)
Two feature streams extracted per frame:

1. **ERB features** `[1, 1, S, 32]`:
   - Compute mean power per ERB band: `sum(|X|^2) / bandwidth`
   - EMA normalization: `state = state * alpha + power * (1-alpha)`
   - Output: `(mean_power - state) / 40.0` (subtractive normalization)

2. **Complex spec features** `[1, 2, S, 96]`:
   - For each of first 96 bins: compute magnitude
   - EMA normalization on magnitude: `state = state * alpha + mag * (1-alpha)`
   - Output: `X / sqrt(state)` (unit-norm by sqrt of running mean)
   - Layout: channel 0 = real, channel 1 = imaginary

EMA states initialized to 0 (not first-frame value), matching the Rust reference.

### Chunked Inference
Large files processed in chunks to bound memory:
- **Chunk stride:** 2000 frames (~20s at 48kHz)
- **Warmup:** 200 frames prepended from previous chunk (GRU state warm-up)
- Only non-warmup frames written to output

### Post-processing Pipeline
1. **ERB mask application:** 32-band gain mask interpolated to 481 bins, applied to original spectrum
2. **Deep filtering:** 5-tap complex FIR on first 96 bins
   - Tap order: `source_frame = f + lookahead - tap` (matching Rust reference)
   - Complex multiply-accumulate: `coef * masked_spectrum`
3. **Alpha blending:** `alpha * DF_result + (1-alpha) * ERB_result`
   - Alpha from df_dec sigmoid output (per-frame, per-bin)

### Multi-channel Handling
Each channel processed independently through the full pipeline:
- Independent EMA feature normalization states per channel
- Independent ONNX inference per channel
- Progress reporting: `channel_base + channel_span * sub_progress`

---

## RoomToneExtractor Implementation

### VAD (Voice Activity Detection)
Dual-criterion frame classification:
- **Energy threshold:** RMS < -35 dBFS
- **Spectral flatness:** > 0.2 (white noise ≈ 1.0, tonal ≈ 0.0)
- **Minimum run:** 4 consecutive frames (~80ms)
- **Fallback:** If no silent segments found, selects quietest 10% of frames

### Spectral Envelope
- 4096-point FFT with Hann window, 1024 hop
- Average magnitude across all silent-segment frames
- Transient rejection: skip frames with energy > 3x running average
- High-pass filter at 80 Hz (quadratic roll-off) to prevent low-freq buildup

### Room Tone Synthesis
- **Method:** Overlap-add IFFT synthesis
- Per-frame: spectral envelope magnitude x random phase → IFFT → window → overlap-add
- Each output channel synthesized independently (unique random seed per channel to avoid comb filtering)
- **RMS calibration:** output normalized to match measured noise floor x 0.75 (safety margin)

---

## Build System Integration

### Xcode / Projucer Configuration

**Header search paths:**
```
../../ThirdParty/onnxruntime-osx-arm64-1.20.1/include
```

**Library search paths:**
```
../../ThirdParty/onnxruntime-osx-arm64-1.20.1/lib
```

**Linker flags:**
```
-lonnxruntime
```

**Runtime library deployment:**
`libonnxruntime.1.20.1.dylib` must be present at:
- Development: linked via `-L` path, `@rpath` resolves to `../Frameworks`
- Release app bundle: copy to `GOODMETER.app/Contents/Frameworks/`

**ONNX model deployment:**
Models loaded at runtime from (searched in order):
1. `<app_bundle>/Contents/Resources/DeepFilterNet3_onnx/`
2. `<app_bundle>/Contents/Frameworks/DeepFilterNet3_onnx/`
3. Hardcoded dev path (debug only)

### Dependencies

| Dependency | Version | License | Size |
|------------|---------|---------|------|
| ONNX Runtime | 1.20.1 | MIT | 24 MB (dylib) |
| DeepFilterNet3 ONNX models | v0.5.6 | MIT | 8.2 MB (3 models) |
| Apple Accelerate | System | - | 0 (system framework) |

---

## Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| Processing speed | ~4s for 40s mono audio | Apple M-series, 4 threads |
| Memory (peak) | ~200 MB for 5-min stereo | Chunked inference bounds memory |
| Latency | Offline only | Not designed for real-time |
| Quality | MOS improvement ~0.5-0.8 | Typical speech enhancement gain |

### ONNX Runtime Configuration
```cpp
opts.SetIntraOpNumThreads(4);
opts.SetGraphOptimizationLevel(ORT_ENABLE_ALL);
```

---

## Export Modes

Controlled via macOS menu bar (Audio Lab > Export mode):

| Mode | Files Generated | Use Case |
|------|----------------|----------|
| Export Both | `_clean.wav` + `_roomtone.wav` | Full post-production workflow |
| Export Clean Only | `_clean.wav` | Quick denoise-and-deliver |
| Export RoomTone Only | `_roomtone.wav` | Fill silence gaps in existing edit |

All exports: 24-bit WAV, original sample rate, original channel count.
Solo channel export supported (mono WAV with `_chN` suffix).

---

## Known Limitations

1. **Offline only:** Full-file processing required; not suitable for real-time streaming
2. **48kHz internal:** Non-48kHz input is resampled (linear interpolation) which may introduce minor artifacts on high-frequency content
3. **Speech-optimized:** DFN3 is trained on speech + noise; may produce artifacts on pure music content
4. **ARM64 macOS only:** Current ONNX Runtime binary is Apple Silicon specific
5. **No GPU acceleration:** CPU-only inference (CoreML provider not yet integrated)

---

## Bug Fixes Log (v2 - matching Rust reference)

These fixes were critical for correct audio output:

1. **ERB features:** Changed from divisive to subtractive normalization (`(power - state) / 40`)
2. **Spec features:** Changed to divide by `sqrt(running_mean_of_magnitude)` (not `running_mean_of_power`)
3. **DF tap order:** Fixed to `f + lookahead - tap` (was `f + tap - lookahead`)
4. **ISTFT:** Removed winSum normalization (Vorbis + 50% overlap = Princen-Bradley, unity gain)
5. **EMA initialization:** States start at 0 (was incorrectly initialized to first-frame value)
6. **Zero-padding:** Added `hop_size` padding on both sides before STFT to prevent edge artifacts
7. **ERB mask scope:** Applied to local cache for ALL chunk frames (DF taps need warmup frames)
