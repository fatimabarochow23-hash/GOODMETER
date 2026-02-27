export class AudioEngine {
  ctx: AudioContext | null = null;
  stream: MediaStream | null = null;
  source: MediaStreamAudioSourceNode | null = null;

  splitter: ChannelSplitterNode | null = null;

  analyzerL: AnalyserNode | null = null;
  analyzerR: AnalyserNode | null = null;

  analyzerLufsL: AnalyserNode | null = null;
  analyzerLufsR: AnalyserNode | null = null;

  analyzerLow: AnalyserNode | null = null;
  analyzerMid: AnalyserNode | null = null;
  analyzerHigh: AnalyserNode | null = null;
  analyzerSide: AnalyserNode | null = null;

  timeDataL: Float32Array;
  timeDataR: Float32Array;
  freqDataL: Float32Array;
  freqDataR: Float32Array;

  lufsTimeDataL: Float32Array;
  lufsTimeDataR: Float32Array;

  timeDataMid: Float32Array;
  timeDataSide: Float32Array;

  isInitialized = false;

  constructor() {
    this.timeDataL = new Float32Array(2048);
    this.timeDataR = new Float32Array(2048);
    this.freqDataL = new Float32Array(2048);
    this.freqDataR = new Float32Array(2048);

    this.lufsTimeDataL = new Float32Array(32768);
    this.lufsTimeDataR = new Float32Array(32768);

    this.timeDataMid = new Float32Array(2048);
    this.timeDataSide = new Float32Array(2048);
  }

  async start(sourceType: 'microphone' | 'system') {
    this.stop();

    this.ctx = new AudioContext();

    try {
      if (sourceType === 'system') {
        // @ts-ignore - getDisplayMedia is available in modern browsers
        this.stream = await navigator.mediaDevices.getDisplayMedia({
          audio: {
            echoCancellation: false,
            noiseSuppression: false,
            autoGainControl: false,
          },
          video: true, // Required by some browsers to capture system audio
        });
      } else {
        this.stream = await navigator.mediaDevices.getUserMedia({
          audio: {
            echoCancellation: false,
            noiseSuppression: false,
            autoGainControl: false,
          },
        });
      }

      if (this.stream.getAudioTracks().length === 0) {
        throw new Error('No audio track found. Please make sure to share audio.');
      }

      this.source = this.ctx.createMediaStreamSource(this.stream);
      this.splitter = this.ctx.createChannelSplitter(2);
      this.source.connect(this.splitter);

      // Standard Analyzers (Peak, RMS, Spectrum, Goniometer)
      this.analyzerL = this.ctx.createAnalyser();
      this.analyzerL.fftSize = 4096;
      this.analyzerL.smoothingTimeConstant = 0.85;
      this.analyzerL.minDecibels = -90;
      this.analyzerL.maxDecibels = 0;
      
      this.analyzerR = this.ctx.createAnalyser();
      this.analyzerR.fftSize = 4096;
      this.analyzerR.smoothingTimeConstant = 0.85;
      this.analyzerR.minDecibels = -90;
      this.analyzerR.maxDecibels = 0;

      this.splitter.connect(this.analyzerL, 0);
      // Connect right channel if it exists, otherwise fallback to left
      try {
        this.splitter.connect(this.analyzerR, 1);
      } catch (e) {
        this.splitter.connect(this.analyzerR, 0);
      }

      // LUFS Analyzers (K-Weighted)
      const highShelfL = this.ctx.createBiquadFilter();
      highShelfL.type = 'highshelf';
      highShelfL.frequency.value = 1500;
      highShelfL.gain.value = 4;

      const highPassL = this.ctx.createBiquadFilter();
      highPassL.type = 'highpass';
      highPassL.frequency.value = 38;

      const highShelfR = this.ctx.createBiquadFilter();
      highShelfR.type = 'highshelf';
      highShelfR.frequency.value = 1500;
      highShelfR.gain.value = 4;

      const highPassR = this.ctx.createBiquadFilter();
      highPassR.type = 'highpass';
      highPassR.frequency.value = 38;

      this.analyzerLufsL = this.ctx.createAnalyser();
      this.analyzerLufsL.fftSize = 32768; // Max size for ~682ms window at 48kHz
      
      this.analyzerLufsR = this.ctx.createAnalyser();
      this.analyzerLufsR.fftSize = 32768;

      this.splitter.connect(highShelfL, 0);
      highShelfL.connect(highPassL);
      highPassL.connect(this.analyzerLufsL);

      try {
        this.splitter.connect(highShelfR, 1);
      } catch (e) {
        this.splitter.connect(highShelfR, 0);
      }
      highShelfR.connect(highPassR);
      highPassR.connect(this.analyzerLufsR);

      // Mid/Side Analyzers
      const midGain = this.ctx.createGain();
      midGain.gain.value = 0.5;
      
      const sideGainL = this.ctx.createGain();
      sideGainL.gain.value = 0.5;
      
      const sideGainR = this.ctx.createGain();
      sideGainR.gain.value = -0.5;
      
      const sideMerger = this.ctx.createGain();
      sideMerger.gain.value = 1.0;

      this.splitter.connect(midGain, 0);
      this.splitter.connect(sideGainL, 0);
      
      try {
        this.splitter.connect(midGain, 1);
        this.splitter.connect(sideGainR, 1);
      } catch (e) {
        // Only 1 channel available
      }
      
      sideGainL.connect(sideMerger);
      sideGainR.connect(sideMerger);

      this.analyzerMid = this.ctx.createAnalyser();
      this.analyzerMid.fftSize = 2048;
      this.analyzerSide = this.ctx.createAnalyser();
      this.analyzerSide.fftSize = 2048;

      midGain.connect(this.analyzerMid);
      sideMerger.connect(this.analyzerSide);

      this.timeDataL = new Float32Array(this.analyzerL.fftSize);
      this.timeDataR = new Float32Array(this.analyzerR.fftSize);
      this.freqDataL = new Float32Array(this.analyzerL.frequencyBinCount);
      this.freqDataR = new Float32Array(this.analyzerR.frequencyBinCount);

      this.timeDataMid = new Float32Array(this.analyzerMid.fftSize);
      this.timeDataSide = new Float32Array(this.analyzerSide.fftSize);

      this.isInitialized = true;
    } catch (err) {
      console.error("Failed to start audio engine:", err);
      this.stop();
      throw err;
    }
  }

  stop() {
    if (this.stream) {
      this.stream.getTracks().forEach(track => track.stop());
      this.stream = null;
    }
    if (this.ctx) {
      this.ctx.close();
      this.ctx = null;
    }
    this.isInitialized = false;
  }

  updateData() {
    if (!this.isInitialized) return;
    this.analyzerL?.getFloatTimeDomainData(this.timeDataL);
    this.analyzerR?.getFloatTimeDomainData(this.timeDataR);
    this.analyzerL?.getFloatFrequencyData(this.freqDataL);
    this.analyzerR?.getFloatFrequencyData(this.freqDataR);
    
    this.analyzerLufsL?.getFloatTimeDomainData(this.lufsTimeDataL);
    this.analyzerLufsR?.getFloatTimeDomainData(this.lufsTimeDataR);

    this.analyzerMid?.getFloatTimeDomainData(this.timeDataMid);
    this.analyzerSide?.getFloatTimeDomainData(this.timeDataSide);
  }

  getPeak() {
    let peakL = 0;
    let peakR = 0;
    for (let i = 0; i < this.timeDataL.length; i++) {
      const absL = Math.abs(this.timeDataL[i]);
      const absR = Math.abs(this.timeDataR[i]);
      if (absL > peakL) peakL = absL;
      if (absR > peakR) peakR = absR;
    }
    return {
      l: 20 * Math.log10(peakL || 1e-8),
      r: 20 * Math.log10(peakR || 1e-8)
    };
  }

  getRMS() {
    let sumL = 0;
    let sumR = 0;
    for (let i = 0; i < this.timeDataL.length; i++) {
      sumL += this.timeDataL[i] * this.timeDataL[i];
      sumR += this.timeDataR[i] * this.timeDataR[i];
    }
    const rmsL = Math.sqrt(sumL / this.timeDataL.length);
    const rmsR = Math.sqrt(sumR / this.timeDataR.length);
    return {
      l: 20 * Math.log10(rmsL || 1e-8),
      r: 20 * Math.log10(rmsR || 1e-8)
    };
  }

  getLUFS() {
    if (!this.ctx) return -70;
    // Momentary LUFS uses a 400ms window.
    const sampleRate = this.ctx.sampleRate;
    const windowSamples = Math.floor(sampleRate * 0.4);
    const startIdx = Math.max(0, this.lufsTimeDataL.length - windowSamples);
    
    let sumL = 0;
    let sumR = 0;
    let count = 0;
    
    for (let i = startIdx; i < this.lufsTimeDataL.length; i++) {
      sumL += this.lufsTimeDataL[i] * this.lufsTimeDataL[i];
      sumR += this.lufsTimeDataR[i] * this.lufsTimeDataR[i];
      count++;
    }
    
    const meanSquareL = sumL / count;
    const meanSquareR = sumR / count;
    
    // LUFS = -0.691 + 10 * log10(meanSquareL + meanSquareR)
    // Assuming stereo, no surround channels.
    const sumMeanSquare = meanSquareL + meanSquareR;
    if (sumMeanSquare < 1e-10) return -70;
    
    return -0.691 + 10 * Math.log10(sumMeanSquare);
  }

  getCorrelation() {
    let sumXY = 0;
    let sumX2 = 0;
    let sumY2 = 0;
    for (let i = 0; i < this.timeDataL.length; i++) {
      const x = this.timeDataL[i];
      const y = this.timeDataR[i];
      sumXY += x * y;
      sumX2 += x * x;
      sumY2 += y * y;
    }
    const denominator = Math.sqrt(sumX2 * sumY2);
    if (denominator === 0) return 0;
    return sumXY / denominator;
  }

  getStereoFieldRMS() {
    let sumL = 0;
    let sumR = 0;
    let sumMid = 0;
    let sumSide = 0;
    for (let i = 0; i < this.timeDataL.length; i++) {
      sumL += this.timeDataL[i] * this.timeDataL[i];
      sumR += this.timeDataR[i] * this.timeDataR[i];
      sumMid += this.timeDataMid[i] * this.timeDataMid[i];
      sumSide += this.timeDataSide[i] * this.timeDataSide[i];
    }
    const rmsL = Math.sqrt(sumL / this.timeDataL.length);
    const rmsR = Math.sqrt(sumR / this.timeDataR.length);
    const rmsMid = Math.sqrt(sumMid / this.timeDataMid.length);
    const rmsSide = Math.sqrt(sumSide / this.timeDataSide.length);
    return {
      l: 20 * Math.log10(rmsL || 1e-8),
      r: 20 * Math.log10(rmsR || 1e-8),
      mid: 20 * Math.log10(rmsMid || 1e-8),
      side: 20 * Math.log10(rmsSide || 1e-8)
    };
  }
}

export const engine = new AudioEngine();
