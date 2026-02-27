import React, { useEffect, useRef } from 'react';
import { engine } from '../../lib/audio';

export function Spectrogram() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const tempCanvasRef = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    if (!tempCanvasRef.current) {
      tempCanvasRef.current = document.createElement('canvas');
      tempCanvasRef.current.width = canvas.width;
      tempCanvasRef.current.height = canvas.height;
      
      // Initialize temp canvas with background color
      const tCtx = tempCanvasRef.current.getContext('2d');
      if (tCtx) {
        tCtx.fillStyle = '#F4F4F6';
        tCtx.fillRect(0, 0, canvas.width, canvas.height);
      }
    }
    const tempCtx = tempCanvasRef.current.getContext('2d');
    if (!tempCtx) return;

    let animationId: number;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();

      const width = canvas.width;
      const height = canvas.height;

      // 1. Shift left by 2 pixels (horizontal scrolling from right to left)
      tempCtx.drawImage(canvas, 2, 0, width - 2, height, 0, 0, width - 2, height);
      
      // 2. Draw the shifted image back to the main canvas FIRST
      ctx.drawImage(tempCanvasRef.current, 0, 0);

      const bufferLength = engine.analyzerL!.frequencyBinCount;
      const dataL = engine.freqDataL;
      const dataR = engine.freqDataR;

      const minFreq = 20;
      const maxFreq = 20000;
      const sampleRate = engine.ctx!.sampleRate;

      const freqToY = (freq: number) => {
        const minLog = Math.log10(minFreq);
        const maxLog = Math.log10(maxFreq);
        const logFreq = Math.log10(Math.max(minFreq, freq));
        return height - ((logFreq - minLog) / (maxLog - minLog)) * height;
      };

      const getColor = (db: number) => {
        const minDb = -90; // Lowered slightly to catch more tail details
        const maxDb = 0;
        const normalized = Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb)));
        
        if (normalized === 0) {
          return 'rgb(244, 244, 246)';
        } else if (normalized <= 0.5) {
          // Background to Pink
          const t = normalized * 2;
          const r = Math.round(244 + t * (230 - 244));
          const g = Math.round(244 + t * (51 - 244));
          const b = Math.round(246 + t * (95 - 246));
          return `rgb(${r}, ${g}, ${b})`;
        } else {
          // Pink to Dark Line
          const t = (normalized - 0.5) * 2;
          const r = Math.round(230 + t * (42 - 230));
          const g = Math.round(51 + t * (42 - 51));
          const b = Math.round(95 + t * (53 - 95));
          return `rgb(${r}, ${g}, ${b})`;
        }
      };

      // 3. Draw the new column on the right edge
      ctx.fillStyle = '#F4F4F6';
      ctx.fillRect(width - 2, 0, 2, height);

      for (let i = 0; i < bufferLength; i++) {
        const freq = (i * sampleRate) / (bufferLength * 2);
        if (freq < minFreq) continue;
        if (freq > maxFreq) break;
        
        const y = freqToY(freq);
        const nextY = freqToY(((i + 1) * sampleRate) / (bufferLength * 2));
        const h = Math.max(1, y - nextY);
        
        const db = Math.max(dataL[i], dataR[i]);
        
        ctx.fillStyle = getColor(db);
        ctx.fillRect(width - 2, nextY, 2, h);
      }
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  return (
    <div className="w-full h-[200px] relative">
      <canvas
        ref={canvasRef}
        width={1600}
        height={400}
        className="w-full h-full"
      />
      <div className="absolute top-0 left-2 text-[10px] font-[600] text-[var(--text-muted)]">20kHz</div>
      <div className="absolute bottom-0 left-2 text-[10px] font-[600] text-[var(--text-muted)]">20Hz</div>
    </div>
  );
}
