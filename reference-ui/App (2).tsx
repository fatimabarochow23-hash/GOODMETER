import React, { useState } from 'react';
import { engine } from './lib/audio';
import { MeterCard } from './components/MeterCard';
import { Levels } from './components/meters/Levels';
import { SpectrumAnalyzer } from './components/meters/SpectrumAnalyzer';
import { ClassicVUMeter } from './components/meters/ClassicVUMeter';
import { StereoFieldAnalyzer } from './components/meters/StereoFieldAnalyzer';
import { StereoImage } from './components/meters/StereoImage';
import { PhaseCorrelation } from './components/meters/PhaseCorrelation';
import { Spectrogram } from './components/meters/Spectrogram';
import { Mic, MonitorUp } from 'lucide-react';

export default function App() {
  const [isConnected, setIsConnected] = useState(false);
  const [source, setSource] = useState<'microphone' | 'system' | null>(null);
  const [standard, setStandard] = useState('EBU R128');

  const handleConnect = async (type: 'microphone' | 'system') => {
    try {
      await engine.start(type);
      setIsConnected(true);
      setSource(type);
    } catch (err: any) {
      console.error(err);
      setIsConnected(false);
      setSource(null);
    }
  };

  const handleDisconnect = () => {
    engine.stop();
    setIsConnected(false);
    setSource(null);
  };

  return (
    <div className="w-[900px] h-[1100px] bg-[var(--bg-main)] font-sans overflow-hidden mx-auto flex flex-col p-[24px]">
      {/* Header */}
      <header className="flex justify-between items-start pb-[20px] border-b-[3px] border-[var(--border-color)] shrink-0 z-10">
        <div>
          <h1 className="text-[42px] font-[900] text-[var(--text-main)] leading-none tracking-tight">GOODMETER</h1>
          <p className="text-[12px] text-[var(--text-muted)] mt-[8px] font-[600] uppercase tracking-widest">PRECISION AUDIO ANALYSIS SUITE</p>
        </div>

        <div className="flex flex-col items-end gap-3">
          {/* Audio Source Buttons */}
          <div className="flex gap-3">
            <button
              onClick={() => isConnected && source === 'microphone' ? handleDisconnect() : handleConnect('microphone')}
              className={`flex items-center gap-2 px-5 py-2 rounded-full border-2 border-[var(--border-color)] font-[800] text-[14px] transition-all shadow-[2px_2px_0px_0px_var(--border-color)] active:translate-y-[2px] active:shadow-none ${
                isConnected && source === 'microphone'
                  ? 'bg-[var(--accent-green)] text-[var(--text-main)]'
                  : 'bg-white text-[var(--text-main)] hover:bg-gray-50'
              }`}
            >
              <Mic size={18} strokeWidth={2.5} />
              MIC
            </button>
            <button
              onClick={() => isConnected && source === 'system' ? handleDisconnect() : handleConnect('system')}
              className={`flex items-center gap-2 px-5 py-2 rounded-full border-2 border-[var(--border-color)] font-[800] text-[14px] transition-all shadow-[2px_2px_0px_0px_var(--border-color)] active:translate-y-[2px] active:shadow-none ${
                isConnected && source === 'system'
                  ? 'bg-[var(--accent-yellow)] text-[var(--text-main)]'
                  : 'bg-[var(--text-main)] text-white hover:bg-gray-800'
              }`}
            >
              <MonitorUp size={18} strokeWidth={2.5} />
              SYSTEM AUDIO
            </button>
          </div>

          {/* Loudness Standard Selector */}
          <div className="w-[200px] h-[36px] relative">
            <select
              value={standard}
              onChange={(e) => setStandard(e.target.value)}
              className="w-full h-full bg-[var(--bg-panel)] border-2 border-[var(--border-color)] rounded-[4px] text-[13px] font-[700] text-[var(--text-main)] px-[12px] appearance-none outline-none cursor-pointer"
            >
              <option value="EBU R128">EBU R128</option>
              <option value="ATSC A/85">ATSC A/85</option>
              <option value="ITU-R BS.1770-4">ITU-R BS.1770-4</option>
              <option value="AES Streaming">AES Streaming</option>
              <option value="Custom">Custom</option>
            </select>
            <div className="absolute right-[12px] top-1/2 -translate-y-1/2 pointer-events-none text-[var(--text-main)] text-[10px] font-[700]">
              ▼
            </div>
          </div>
        </div>
      </header>

      {/* Modules Container */}
      <div className="flex-1 overflow-y-auto flex flex-col gap-[20px] mt-[20px] pr-2 custom-scrollbar pb-[20px]">
        <MeterCard title="LEVELS" indicatorColor="var(--accent-pink)" defaultExpanded>
          <Levels standard={standard} />
        </MeterCard>

        <MeterCard title="SPECTRUM ANALYZER" indicatorColor="var(--accent-green)" defaultExpanded>
          <SpectrumAnalyzer />
        </MeterCard>

        <MeterCard title="CLASSIC VU METER" indicatorColor="var(--accent-purple)">
          <ClassicVUMeter />
        </MeterCard>

        <MeterCard title="STEREO FIELD ANALYZER" indicatorColor="var(--accent-cyan)">
          <StereoFieldAnalyzer />
        </MeterCard>

        <MeterCard title="PHASE CORRELATION" indicatorColor="var(--accent-cyan)">
          <PhaseCorrelation />
        </MeterCard>

        <MeterCard title="STEREO IMAGE" indicatorColor="var(--accent-yellow)">
          <StereoImage />
        </MeterCard>

        <MeterCard title="SPECTROGRAM" indicatorColor="var(--accent-pink)">
          <Spectrogram />
        </MeterCard>
      </div>
    </div>
  );
}
