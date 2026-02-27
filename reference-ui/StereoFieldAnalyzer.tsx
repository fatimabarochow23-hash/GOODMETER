import React, { useEffect, useRef, useState } from 'react';
import { engine } from '../../lib/audio';

class Particle {
  x: number;
  y: number;
  vx: number;
  vy: number;
  life: number;
  maxLife: number;
  color: string;
  size: number;

  constructor(x: number, y: number, color: string) {
    this.x = x;
    this.y = y;
    this.color = color;
    this.vx = (Math.random() - 0.5) * 10;
    this.vy = -(Math.random() * 12 + 4);
    this.life = 0;
    this.maxLife = Math.random() * 30 + 30;
    this.size = Math.random() * 6 + 4;
  }

  update() {
    this.x += this.vx;
    this.y += this.vy;
    this.vy += 0.8; // gravity
    this.life++;
  }

  draw(ctx: CanvasRenderingContext2D) {
    ctx.beginPath();
    ctx.arc(this.x, this.y, this.size, 0, Math.PI * 2);
    ctx.fillStyle = this.color;
    ctx.globalAlpha = Math.max(0, 1 - this.life / this.maxLife);
    ctx.fill();
    ctx.globalAlpha = 1;
  }
}

class FlaskPhysics {
  currentLevel: number = 0;
  waveAmp: number = 0;
  wavePhase: number = 0;

  update(db: number) {
    const minDb = -60;
    const maxDb = 0;
    let target = (db - minDb) / (maxDb - minDb);
    target = Math.max(0, Math.min(1, target));
    
    // Level smoothing
    const lastLevel = this.currentLevel;
    this.currentLevel += (target - this.currentLevel) * 0.15;
    
    // Wave physics
    const delta = Math.abs(this.currentLevel - lastLevel);
    this.waveAmp += delta * 150; // Add energy
    this.waveAmp *= 0.85; // Damping
    this.wavePhase += 0.25;
  }
}

export function StereoFieldAnalyzer() {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [peaks, setPeaks] = useState({ l: -70, c: -70, r: -70, mid: -70, side: -70 });

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let lastUpdate = performance.now();

    const width = canvas.width;
    const height = canvas.height;
    
    const physL = new FlaskPhysics();
    const physC = new FlaskPhysics();
    const physR = new FlaskPhysics();
    const physMid = new FlaskPhysics();
    const physSide = new FlaskPhysics();

    let particles: Particle[] = [];

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();
      
      const rms = engine.getStereoFieldRMS();
      // For C, we use Mid signal
      const dbL = rms.l;
      const dbC = rms.mid;
      const dbR = rms.r;
      const dbMid = rms.mid;
      const dbSide = rms.side;

      const now = performance.now();
      
      if (now - lastUpdate > 100) {
        setPeaks({ l: dbL, c: dbC, r: dbR, mid: dbMid, side: dbSide });
        lastUpdate = now;
      }

      physL.update(dbL);
      physC.update(dbC);
      physR.update(dbR);
      physMid.update(dbMid);
      physSide.update(dbSide);

      ctx.clearRect(0, 0, width, height);
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(0, 0, width, height);

      // Layout
      const cy = height / 2 + 40;
      const mainCx = width / 2;
      const midCx = mainCx - 360;
      const sideCx = mainCx + 360;

      // Colors
      const colorPink = '#E6335F';
      const colorYellow = '#FFD166';
      const colorCyan = '#06D6A0';

      // Overflow logic
      const spawnParticles = (x: number, y: number, color: string) => {
        for(let i=0; i<3; i++) {
          particles.push(new Particle(x, y, color));
        }
      };

      if (dbL >= 0) spawnParticles(mainCx - 100, cy - 200, colorPink);
      if (dbC >= 0) spawnParticles(mainCx, cy - 200, colorPink);
      if (dbR >= 0) spawnParticles(mainCx + 100, cy - 200, colorPink);
      if (dbMid >= 0) spawnParticles(midCx, cy - 200, colorYellow);
      if (dbSide >= 0) spawnParticles(sideCx, cy - 200, colorCyan);

      particles.forEach(p => p.update());
      particles = particles.filter(p => p.life < p.maxLife && p.y < height);

      // Helper to draw liquid with wave
      const drawLiquid = (cx: number, cy: number, phys: FlaskPhysics, color: string, w: number, h: number, yOffset: number = 0) => {
        const liquidY = (cy + yOffset) - phys.currentLevel * h;
        const leftX = cx - w/2;
        const rightX = cx + w/2;
        
        ctx.beginPath();
        ctx.moveTo(leftX, cy + yOffset);
        ctx.lineTo(leftX, liquidY);
        
        const cpX = cx;
        const cpY = liquidY + Math.sin(phys.wavePhase) * phys.waveAmp;
        
        ctx.quadraticCurveTo(cpX, cpY, rightX, liquidY);
        ctx.lineTo(rightX, cy + yOffset);
        ctx.closePath();
        
        ctx.fillStyle = color;
        ctx.fill();
      };

      // Draw Mid Flask (Flat-Bottom)
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(midCx - 30, cy - 200);
      ctx.lineTo(midCx - 30, cy - 50);
      ctx.lineTo(midCx - 100, cy + 100);
      ctx.quadraticCurveTo(midCx - 100, cy + 150, midCx - 50, cy + 150);
      ctx.lineTo(midCx + 50, cy + 150);
      ctx.quadraticCurveTo(midCx + 100, cy + 150, midCx + 100, cy + 100);
      ctx.lineTo(midCx + 30, cy - 50);
      ctx.lineTo(midCx + 30, cy - 200);
      ctx.clip();
      drawLiquid(midCx, cy, physMid, colorYellow, 240, 350, 150);
      ctx.restore();

      ctx.beginPath();
      ctx.moveTo(midCx - 30, cy - 200);
      ctx.lineTo(midCx - 30, cy - 50);
      ctx.lineTo(midCx - 100, cy + 100);
      ctx.quadraticCurveTo(midCx - 100, cy + 150, midCx - 50, cy + 150);
      ctx.lineTo(midCx + 50, cy + 150);
      ctx.quadraticCurveTo(midCx + 100, cy + 150, midCx + 100, cy + 100);
      ctx.lineTo(midCx + 30, cy - 50);
      ctx.lineTo(midCx + 30, cy - 200);
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 6;
      ctx.stroke();

      // Draw Side Flask (Flat-Bottom)
      ctx.save();
      ctx.beginPath();
      ctx.moveTo(sideCx - 30, cy - 200);
      ctx.lineTo(sideCx - 30, cy - 50);
      ctx.lineTo(sideCx - 100, cy + 100);
      ctx.quadraticCurveTo(sideCx - 100, cy + 150, sideCx - 50, cy + 150);
      ctx.lineTo(sideCx + 50, cy + 150);
      ctx.quadraticCurveTo(sideCx + 100, cy + 150, sideCx + 100, cy + 100);
      ctx.lineTo(sideCx + 30, cy - 50);
      ctx.lineTo(sideCx + 30, cy - 200);
      ctx.clip();
      drawLiquid(sideCx, cy, physSide, colorCyan, 240, 350, 150);
      ctx.restore();

      ctx.beginPath();
      ctx.moveTo(sideCx - 30, cy - 200);
      ctx.lineTo(sideCx - 30, cy - 50);
      ctx.lineTo(sideCx - 100, cy + 100);
      ctx.quadraticCurveTo(sideCx - 100, cy + 150, sideCx - 50, cy + 150);
      ctx.lineTo(sideCx + 50, cy + 150);
      ctx.quadraticCurveTo(sideCx + 100, cy + 150, sideCx + 100, cy + 100);
      ctx.lineTo(sideCx + 30, cy - 50);
      ctx.lineTo(sideCx + 30, cy - 200);
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 6;
      ctx.stroke();

      // Draw LCR Flask (Three-Neck Round-Bottom)
      ctx.save();
      ctx.beginPath();
      // Left neck
      ctx.moveTo(mainCx - 130, cy - 200);
      ctx.lineTo(mainCx - 130, cy - 50);
      ctx.lineTo(mainCx - 70, cy - 50);
      ctx.lineTo(mainCx - 70, cy - 200);
      // Center neck
      ctx.moveTo(mainCx - 30, cy - 200);
      ctx.lineTo(mainCx - 30, cy - 80);
      ctx.lineTo(mainCx + 30, cy - 80);
      ctx.lineTo(mainCx + 30, cy - 200);
      // Right neck
      ctx.moveTo(mainCx + 70, cy - 200);
      ctx.lineTo(mainCx + 70, cy - 50);
      ctx.lineTo(mainCx + 130, cy - 50);
      ctx.lineTo(mainCx + 130, cy - 200);
      // Main body (large circle)
      ctx.arc(mainCx, cy + 50, 150, 0, Math.PI * 2);
      ctx.clip();

      // We need to draw 3 liquid columns that merge at the bottom.
      // Easiest way: draw 3 separate liquids, they will overlap in the circle.
      // To make it look connected, we draw them all with the same color.
      ctx.globalAlpha = 0.8;
      drawLiquid(mainCx - 100, cy, physL, colorPink, 150, 350, 200);
      drawLiquid(mainCx, cy, physC, colorPink, 150, 350, 200);
      drawLiquid(mainCx + 100, cy, physR, colorPink, 150, 350, 200);
      ctx.globalAlpha = 1.0;
      ctx.restore();

      // Draw LCR Flask Outline
      ctx.beginPath();
      // Left neck
      ctx.moveTo(mainCx - 130, cy - 200);
      ctx.lineTo(mainCx - 130, cy - 50);
      ctx.arc(mainCx, cy + 50, 150, Math.PI + 0.5, Math.PI * 1.5 - 0.2);
      ctx.lineTo(mainCx - 30, cy - 200);
      
      ctx.moveTo(mainCx + 30, cy - 200);
      ctx.lineTo(mainCx + 30, cy - 80);
      ctx.arc(mainCx, cy + 50, 150, Math.PI * 1.5 + 0.2, Math.PI * 2 - 0.5);
      ctx.lineTo(mainCx + 130, cy - 200);
      
      ctx.moveTo(mainCx + 70, cy - 200);
      ctx.lineTo(mainCx + 70, cy - 50);
      
      ctx.moveTo(mainCx - 70, cy - 200);
      ctx.lineTo(mainCx - 70, cy - 50);

      ctx.arc(mainCx, cy + 50, 150, Math.PI * 2 - 0.5, Math.PI + 0.5);

      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 6;
      ctx.stroke();

      // Draw particles
      particles.forEach(p => p.draw(ctx));
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  const formatDb = (val: number) => val <= -60 ? '-∞' : val.toFixed(1);

  return (
    <div className="flex flex-col w-full h-[320px] items-center">
      <canvas
        ref={canvasRef}
        width={1600}
        height={500}
        className="w-full h-[250px]"
      />
      <div className="flex justify-center gap-[60px] w-full mt-[12px]">
        <div className="flex flex-col items-center w-[60px]">
          <span className="text-[12px] font-[800] text-[var(--text-main)]">MID</span>
          <span className="text-[10px] font-[600] text-[var(--text-muted)]">{formatDb(peaks.mid)}</span>
        </div>
        <div className="flex flex-col items-center w-[60px]">
          <span className="text-[12px] font-[800] text-[var(--text-main)]">L</span>
          <span className="text-[10px] font-[600] text-[var(--text-muted)]">{formatDb(peaks.l)}</span>
        </div>
        <div className="flex flex-col items-center w-[60px]">
          <span className="text-[12px] font-[800] text-[var(--text-main)]">C</span>
          <span className="text-[10px] font-[600] text-[var(--text-muted)]">{formatDb(peaks.c)}</span>
        </div>
        <div className="flex flex-col items-center w-[60px]">
          <span className="text-[12px] font-[800] text-[var(--text-main)]">R</span>
          <span className="text-[10px] font-[600] text-[var(--text-muted)]">{formatDb(peaks.r)}</span>
        </div>
        <div className="flex flex-col items-center w-[60px]">
          <span className="text-[12px] font-[800] text-[var(--text-main)]">SIDE</span>
          <span className="text-[10px] font-[600] text-[var(--text-muted)]">{formatDb(peaks.side)}</span>
        </div>
      </div>
    </div>
  );
}
