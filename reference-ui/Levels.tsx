import React, { useEffect, useRef, useState } from 'react';
import { engine } from '../../lib/audio';

export function Levels({ standard }: { standard: string }) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [metrics, setMetrics] = useState({
    momentary: -70,
    shortTerm: -70,
    integrated: -70,
    truePeakL: -70,
    truePeakR: -70,
    luRange: 0
  });

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let lastUpdate = performance.now();
    
    // Peak hold state
    let peakHoldL = -60;
    let peakHoldR = -60;
    let peakHoldTimeL = 0;
    let peakHoldTimeR = 0;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();
      const peak = engine.getPeak();
      const lufs = engine.getLUFS(); // Momentary LUFS
      
      const now = performance.now();
      
      // Update peak holds
      if (peak.l > peakHoldL || now - peakHoldTimeL > 1000) {
        if (peak.l > peakHoldL) {
          peakHoldL = peak.l;
          peakHoldTimeL = now;
        } else {
          peakHoldL -= 0.5; // decay
        }
      }
      if (peak.r > peakHoldR || now - peakHoldTimeR > 1000) {
        if (peak.r > peakHoldR) {
          peakHoldR = peak.r;
          peakHoldTimeR = now;
        } else {
          peakHoldR -= 0.5;
        }
      }

      if (now - lastUpdate > 100) {
        setMetrics(prev => ({
          ...prev,
          momentary: lufs,
          shortTerm: lufs, // Simplified for now
          integrated: lufs, // Simplified for now
          truePeakL: peakHoldL,
          truePeakR: peakHoldR,
          luRange: 5.2 // Mock value
        }));
        lastUpdate = now;
      }

      const width = canvas.width;
      const height = canvas.height; // 28px * 2 + 12px gap = 68px

      ctx.clearRect(0, 0, width, height);

      const drawBar = (y: number, currentPeak: number, holdPeak: number) => {
        const barHeight = 28 * 2; // High res
        
        // Background
        ctx.fillStyle = '#EAEAEA';
        ctx.fillRect(0, y, width, barHeight);
        
        // Border
        ctx.strokeStyle = '#2A2A35';
        ctx.lineWidth = 2;
        ctx.strokeRect(0, y, width, barHeight);

        const minDb = -60;
        const maxDb = 0;
        
        const dbToX = (db: number) => {
          const clamped = Math.max(minDb, Math.min(maxDb, db));
          return ((clamped - minDb) / (maxDb - minDb)) * width;
        };

        const currentX = dbToX(currentPeak);
        
        // Gradient
        const gradient = ctx.createLinearGradient(0, 0, width, 0);
        gradient.addColorStop(0, '#00D084');
        gradient.addColorStop(dbToX(-18) / width, '#00D084');
        gradient.addColorStop(dbToX(-18) / width, '#FFD166');
        gradient.addColorStop(dbToX(-6) / width, '#FFD166');
        gradient.addColorStop(dbToX(-6) / width, '#E6335F');
        gradient.addColorStop(1, '#E6335F');

        ctx.fillStyle = gradient;
        ctx.fillRect(0, y, currentX, barHeight);

        // Peak hold
        const holdX = dbToX(holdPeak);
        ctx.fillStyle = '#2A2A35';
        ctx.fillRect(holdX, y, 4, barHeight);

        // Target Loudness Reference Line
        let targetLoudness = -23;
        if (standard === 'ATSC A/85') targetLoudness = -24;
        else if (standard === 'AES Streaming') targetLoudness = -16;
        
        const targetX = dbToX(targetLoudness);
        ctx.strokeStyle = '#06D6A0';
        ctx.lineWidth = 4;
        ctx.setLineDash([8, 8]);
        ctx.beginPath();
        ctx.moveTo(targetX, y);
        ctx.lineTo(targetX, y + barHeight);
        ctx.stroke();
        ctx.setLineDash([]);
      };

      drawBar(0, peak.l, peakHoldL);
      drawBar(40 * 2, peak.r, peakHoldR); // 28 + 12 gap
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, [standard]);

  const formatDb = (val: number) => val <= -60 ? '-∞' : val.toFixed(1);

  return (
    <div className="flex flex-col w-full h-[200px]">
      {/* Canvas for Peak Bars */}
      <div className="relative w-full h-[68px]">
        <canvas
          ref={canvasRef}
          width={1600} // High res
          height={136}
          className="w-full h-[68px]"
        />
        {/* Ticks */}
        <div className="absolute top-0 left-0 w-full h-full pointer-events-none">
          {[-60, -40, -20, -10, -6, -3, 0].map(db => {
            const left = `${((db + 60) / 60) * 100}%`;
            return (
              <div key={db} className="absolute top-0 bottom-0 border-l-2 border-[rgba(42,42,53,0.1)]" style={{ left }}>
                <span className="absolute top-full mt-1 -translate-x-1/2 text-[10px] text-[var(--text-muted)] font-[600]">{db}</span>
              </div>
            );
          })}
        </div>
      </div>

      {/* Loudness Info */}
      <div className="mt-[36px] min-h-[80px] py-[12px] px-[16px] bg-[#EAEAEA] border-2 border-[var(--border-color)] rounded-[4px] grid grid-cols-3 gap-4">
        <div className="flex flex-col justify-between gap-2">
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">momentary</span>
            <span className={`text-[1.2rem] font-[800] ${metrics.momentary > -10 ? 'text-[var(--accent-pink)]' : 'text-[var(--text-main)]'}`}>
              {formatDb(metrics.momentary)} LUFS
            </span>
          </div>
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">true peak l</span>
            <span className={`text-[1.2rem] font-[800] ${metrics.truePeakL > -1 ? 'text-[var(--accent-pink)]' : 'text-[var(--text-main)]'}`}>
              {formatDb(metrics.truePeakL)} dBTP
            </span>
          </div>
        </div>
        <div className="flex flex-col justify-between gap-2">
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">short-term</span>
            <span className="text-[1.2rem] font-[800] text-[var(--text-main)]">
              {formatDb(metrics.shortTerm)} LUFS
            </span>
          </div>
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">true peak r</span>
            <span className={`text-[1.2rem] font-[800] ${metrics.truePeakR > -1 ? 'text-[var(--accent-pink)]' : 'text-[var(--text-main)]'}`}>
              {formatDb(metrics.truePeakR)} dBTP
            </span>
          </div>
        </div>
        <div className="flex flex-col justify-between gap-2">
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">integrated</span>
            <span className="text-[1.2rem] font-[800] text-[var(--text-main)]">
              {formatDb(metrics.integrated)} LUFS
            </span>
          </div>
          <div className="flex justify-between items-center">
            <span className="text-[0.8rem] font-[600] text-[var(--text-muted)] lowercase">lu range</span>
            <span className="text-[1.2rem] font-[800] text-[var(--text-main)]">
              {metrics.luRange.toFixed(1)} LU
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}
