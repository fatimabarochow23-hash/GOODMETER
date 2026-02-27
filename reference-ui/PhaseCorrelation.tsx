import React, { useEffect, useRef, useState } from 'react';
import { engine } from '../../lib/audio';

export function PhaseCorrelation() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [corrValue, setCorrValue] = useState<string>('0.00');

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let lastUpdate = performance.now();
    let smoothedPhase = 0;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();
      const corr = engine.getCorrelation();
      
      smoothedPhase += (corr - smoothedPhase) * 0.1;

      const now = performance.now();
      if (now - lastUpdate > 100) {
        setCorrValue(smoothedPhase.toFixed(2));
        lastUpdate = now;
      }

      const width = canvas.width;
      const height = canvas.height;
      const cx = width / 2;
      const cy = height / 2 - 20;

      ctx.clearRect(0, 0, width, height);

      const condenserWidth = width * 0.7;
      const condenserHeight = 120;
      const startX = cx - condenserWidth / 2;
      const endX = cx + condenserWidth / 2;
      
      // Draw Outer Tube
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 6;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      
      ctx.beginPath();
      // Top edge
      ctx.moveTo(startX, cy - condenserHeight/2);
      ctx.lineTo(endX, cy - condenserHeight/2);
      // Bottom edge
      ctx.moveTo(startX, cy + condenserHeight/2);
      ctx.lineTo(endX, cy + condenserHeight/2);
      
      // Left end caps
      ctx.moveTo(startX, cy - condenserHeight/2);
      ctx.lineTo(startX, cy - 24);
      ctx.moveTo(startX, cy + 24);
      ctx.lineTo(startX, cy + condenserHeight/2);
      
      // Right end caps
      ctx.moveTo(endX, cy - condenserHeight/2);
      ctx.lineTo(endX, cy - 24);
      ctx.moveTo(endX, cy + 24);
      ctx.lineTo(endX, cy + condenserHeight/2);
      
      // Inlet (top left)
      ctx.moveTo(startX + 60, cy - condenserHeight/2);
      ctx.lineTo(startX + 60, cy - condenserHeight/2 - 30);
      ctx.moveTo(startX + 100, cy - condenserHeight/2);
      ctx.lineTo(startX + 100, cy - condenserHeight/2 - 30);
      
      // Outlet (bottom right)
      ctx.moveTo(endX - 100, cy + condenserHeight/2);
      ctx.lineTo(endX - 100, cy + condenserHeight/2 + 30);
      ctx.moveTo(endX - 60, cy + condenserHeight/2);
      ctx.lineTo(endX - 60, cy + condenserHeight/2 + 30);
      
      ctx.stroke();

      // Inner wavy tube path
      const loops = 8;
      const drawInnerTubePath = () => {
        ctx.beginPath();
        ctx.moveTo(startX - 80, cy);
        ctx.lineTo(startX, cy);
        for (let i = 0; i <= loops * 40; i++) {
          const t = i / (loops * 40);
          const x = startX + t * condenserWidth;
          const y = cy + Math.sin(t * Math.PI * 2 * loops) * (condenserHeight/2 - 28);
          ctx.lineTo(x, y);
        }
        ctx.lineTo(endX + 80, cy);
      };

      // 1. Inner tube outline (thick black)
      drawInnerTubePath();
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 24;
      ctx.stroke();

      // 2. Inner tube inside (white)
      drawInnerTubePath();
      ctx.strokeStyle = '#FFFFFF';
      ctx.lineWidth = 16;
      ctx.stroke();

      // 3. Draw liquid (colored)
      ctx.save();
      const blobWidth = 160;
      const mappedX = startX + ((smoothedPhase + 1) / 2) * condenserWidth;
      ctx.beginPath();
      ctx.rect(mappedX - blobWidth/2, 0, blobWidth, height);
      ctx.clip();
      
      drawInnerTubePath();
      ctx.strokeStyle = smoothedPhase > 0 ? '#06D6A0' : '#E6335F';
      ctx.lineWidth = 16;
      ctx.stroke();
      ctx.restore();
      
      // Draw center zero line
      ctx.beginPath();
      ctx.moveTo(cx, cy - condenserHeight/2 - 20);
      ctx.lineTo(cx, cy + condenserHeight/2 + 20);
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 4;
      ctx.setLineDash([8, 8]);
      ctx.stroke();
      ctx.setLineDash([]);
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  return (
    <div className="flex flex-col w-full h-[180px] items-center">
      <canvas
        ref={canvasRef}
        width={1600}
        height={320}
        className="w-full h-[140px]"
      />
      <div className="flex justify-between w-full px-[40px] mt-[8px] text-[14px] font-[800] text-[var(--text-main)]">
        <span className="text-[var(--accent-pink)]">-1.0</span>
        <span className="text-[1.2rem]">{corrValue}</span>
        <span className="text-[var(--accent-cyan)]">+1.0</span>
      </div>
    </div>
  );
}
