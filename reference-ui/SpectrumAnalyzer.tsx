import React, { useEffect, useRef } from 'react';
import { engine } from '../../lib/audio';

export function SpectrumAnalyzer() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();

      const width = canvas.width;
      const height = canvas.height;

      ctx.clearRect(0, 0, width, height);

      const bufferLength = engine.analyzerL!.frequencyBinCount;
      const dataL = engine.freqDataL;
      const dataR = engine.freqDataR;

      const minFreq = 20;
      const maxFreq = 20000;
      const sampleRate = engine.ctx!.sampleRate;
      
      const freqToX = (freq: number) => {
        const minLog = Math.log10(minFreq);
        const maxLog = Math.log10(maxFreq);
        const logFreq = Math.log10(Math.max(minFreq, freq));
        return ((logFreq - minLog) / (maxLog - minLog)) * width;
      };

      const dbToY = (db: number) => {
        const minDb = -90;
        const maxDb = 0;
        const clamped = Math.max(minDb, Math.min(maxDb, db));
        return height - ((clamped - minDb) / (maxDb - minDb)) * height;
      };

      // Draw grid
      ctx.strokeStyle = 'rgba(42, 42, 53, 0.1)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000].forEach(f => {
        const x = freqToX(f);
        ctx.moveTo(x, 0);
        ctx.lineTo(x, height);
      });
      [-20, -40, -60, -80].forEach(db => {
        const y = dbToY(db);
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
      });
      ctx.stroke();

      // Draw spectrum (Max of L and R)
      ctx.beginPath();
      ctx.strokeStyle = '#E6335F';
      ctx.lineWidth = 2; // User requested 2
      ctx.lineJoin = 'round';
      
      let startX = -1;
      let endX = -1;

      for (let i = 0; i < bufferLength; i++) {
        const freq = (i * sampleRate) / (bufferLength * 2);
        if (freq < minFreq) continue;
        if (freq > maxFreq) break;
        
        const x = freqToX(freq);
        const db = Math.max(dataL[i], dataR[i]);
        const y = dbToY(db);
        
        if (startX === -1) {
          ctx.moveTo(x, y);
          startX = x;
        } else {
          ctx.lineTo(x, y);
        }
        endX = x;
      }
      ctx.stroke();

      if (startX !== -1) {
        ctx.lineTo(endX, height);
        ctx.lineTo(startX, height);
        ctx.closePath();
        ctx.fillStyle = 'rgba(230, 51, 95, 0.15)';
        ctx.fill();
      }
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  return (
    <div className="w-full h-[240px] relative">
      <canvas
        ref={canvasRef}
        width={1600}
        height={480}
        className="w-full h-full"
      />
      {/* Labels */}
      <div className="absolute bottom-0 left-0 w-full flex justify-between text-[10px] font-[600] text-[var(--text-muted)] px-1 pointer-events-none">
        <span>20Hz</span>
        <span>50</span>
        <span>100</span>
        <span>200</span>
        <span>500</span>
        <span>1k</span>
        <span>2k</span>
        <span>5k</span>
        <span>10k</span>
        <span>20kHz</span>
      </div>
    </div>
  );
}
