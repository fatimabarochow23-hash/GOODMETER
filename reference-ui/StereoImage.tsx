import React, { useEffect, useRef } from 'react';
import { engine } from '../../lib/audio';

export function StereoImage() {
  const leftCanvasRef = useRef<HTMLCanvasElement>(null);
  const rightCanvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const leftCanvas = leftCanvasRef.current;
    const rightCanvas = rightCanvasRef.current;
    if (!leftCanvas || !rightCanvas) return;
    
    const ctxL = leftCanvas.getContext('2d');
    const ctxR = rightCanvas.getContext('2d');
    if (!ctxL || !ctxR) return;

    let animationId: number;
    
    // Smoothing states for cylinders
    let levelL = 0;
    let levelR = 0;
    let levelM = 0;
    let levelS = 0;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();
      
      // Calculate RMS for L, R, M, S
      let sumL = 0;
      let sumR = 0;
      let sumM = 0;
      let sumS = 0;
      
      const timeDataL = engine.timeDataL;
      const timeDataR = engine.timeDataR;
      const len = timeDataL.length;
      
      for(let i=0; i<len; i++) {
        const l = timeDataL[i];
        const r = timeDataR[i];
        const m = (l + r) * 0.7071;
        const s = (l - r) * 0.7071;
        
        sumL += l * l;
        sumR += r * r;
        sumM += m * m;
        sumS += s * s;
      }
      
      const rmsL = Math.sqrt(sumL / len);
      const rmsR = Math.sqrt(sumR / len);
      const rmsM = Math.sqrt(sumM / len);
      const rmsS = Math.sqrt(sumS / len);
      
      const dbL = 20 * Math.log10(rmsL || 1e-8);
      const dbR = 20 * Math.log10(rmsR || 1e-8);
      const dbM = 20 * Math.log10(rmsM || 1e-8);
      const dbS = 20 * Math.log10(rmsS || 1e-8);

      const minDb = -60;
      const maxDb = 0;
      
      const getTarget = (db: number) => Math.max(0, Math.min(1, (db - minDb) / (maxDb - minDb)));
      
      levelL += (getTarget(dbL) - levelL) * 0.2;
      levelR += (getTarget(dbR) - levelR) * 0.2;
      levelM += (getTarget(dbM) - levelM) * 0.2;
      levelS += (getTarget(dbS) - levelS) * 0.2;

      // --- Draw Left Canvas (Cylinders) ---
      const wL = leftCanvas.width;
      const hL = leftCanvas.height;
      ctxL.clearRect(0, 0, wL, hL);
      ctxL.fillStyle = '#FFFFFF';
      ctxL.fillRect(0, 0, wL, hL);

      const drawCylinder = (x: number, y: number, w: number, h: number, level: number, color: string, label: string) => {
        // Trace cylinder path
        ctxL.beginPath();
        ctxL.moveTo(x, y);
        ctxL.lineTo(x, y + h - w/2);
        ctxL.arc(x + w/2, y + h - w/2, w/2, Math.PI, 0, true);
        ctxL.lineTo(x + w, y);
        ctxL.closePath();

        // Clip and fill liquid
        ctxL.save();
        ctxL.clip();
        
        const liquidH = level * h;
        const liquidY = y + h - liquidH;
        
        ctxL.fillStyle = color;
        ctxL.fillRect(x, liquidY, w, liquidH);
        ctxL.restore();

        // Draw outline
        ctxL.beginPath();
        ctxL.moveTo(x, y);
        ctxL.lineTo(x, y + h - w/2);
        ctxL.arc(x + w/2, y + h - w/2, w/2, Math.PI, 0, true);
        ctxL.lineTo(x + w, y);
        ctxL.closePath();
        ctxL.strokeStyle = '#2A2A35';
        ctxL.lineWidth = 6; // ~3px in CSS
        ctxL.stroke();

        // Draw ticks
        ctxL.strokeStyle = '#2A2A35';
        ctxL.lineWidth = 4;
        for(let i=1; i<=4; i++) {
          const tickY = y + (h * i) / 5;
          ctxL.beginPath();
          ctxL.moveTo(x + w, tickY);
          ctxL.lineTo(x + w + 16, tickY);
          ctxL.stroke();
        }

        // Draw label
        ctxL.fillStyle = '#2A2A35';
        ctxL.font = 'bold 32px sans-serif';
        ctxL.textAlign = 'center';
        ctxL.fillText(label, x + w/2, y + h + 50);
      };

      const cylW = 60;
      const cylH = hL - 160;
      const cylY = 60;
      const gap = 80;
      const totalW = 4 * cylW + 3 * gap;
      const startX = (wL - totalW) / 2;

      drawCylinder(startX, cylY, cylW, cylH, levelL, '#E6335F', 'L');
      drawCylinder(startX + cylW + gap, cylY, cylW, cylH, levelR, '#E6335F', 'R');
      drawCylinder(startX + 2*(cylW + gap), cylY, cylW, cylH, levelM, '#FFD166', 'M');
      drawCylinder(startX + 3*(cylW + gap), cylY, cylW, cylH, levelS, '#06D6A0', 'S');


      // --- Draw Right Canvas (Goniometer) ---
      const wR = rightCanvas.width;
      const hR = rightCanvas.height;
      
      // Fade out previous frame for ghosting effect
      ctxR.fillStyle = 'rgba(255, 255, 255, 0.15)';
      ctxR.fillRect(0, 0, wR, hR);

      const cx = wR / 2;
      const cy = hR - 60;
      const radius = hR - 120;

      // Draw Grid
      ctxR.strokeStyle = '#2A2A35';
      ctxR.lineWidth = 4;
      
      // Semicircle
      ctxR.beginPath();
      ctxR.arc(cx, cy, radius, Math.PI, 0);
      ctxR.lineTo(cx - radius, cy);
      ctxR.stroke();

      // Radial lines
      ctxR.beginPath();
      // M (Center)
      ctxR.moveTo(cx, cy);
      ctxR.lineTo(cx, cy - radius);
      // L (45 deg left)
      ctxR.moveTo(cx, cy);
      ctxR.lineTo(cx - radius * Math.cos(Math.PI/4), cy - radius * Math.sin(Math.PI/4));
      // R (45 deg right)
      ctxR.moveTo(cx, cy);
      ctxR.lineTo(cx + radius * Math.cos(Math.PI/4), cy - radius * Math.sin(Math.PI/4));
      ctxR.stroke();

      // Labels
      ctxR.fillStyle = '#2A2A35';
      ctxR.font = 'bold 28px sans-serif';
      ctxR.textAlign = 'center';
      ctxR.textBaseline = 'middle';
      
      ctxR.fillText('M', cx, cy - radius - 30);
      ctxR.fillText('L', cx - radius * Math.cos(Math.PI/4) - 20, cy - radius * Math.sin(Math.PI/4) - 20);
      ctxR.fillText('R', cx + radius * Math.cos(Math.PI/4) + 20, cy - radius * Math.sin(Math.PI/4) - 20);
      ctxR.fillText('+S', cx - radius - 30, cy);
      ctxR.fillText('-S', cx + radius + 30, cy);

      // Draw Trace
      ctxR.beginPath();
      ctxR.strokeStyle = 'rgba(230, 51, 95, 0.3)'; // Semi-transparent pink
      ctxR.lineWidth = 1; // Extremely thin line
      ctxR.lineJoin = 'round';

      const scale = radius / 1.5; // Adjust scale to fit well within radius
      const widthFactor = 1.0;
      const heightFactor = 1.0;

      let started = false;
      for(let i=0; i<len; i++) {
        const l = timeDataL[i];
        const r = timeDataR[i];
        
        // X: Side (R - L) so R goes right, L goes left
        // Y: Mid (L + R)
        const x = cx + (r - l) * scale * widthFactor;
        const y = cy - Math.abs(l + r) * scale * heightFactor; // Use abs to keep it in the upper semicircle
        
        // Constrain to semicircle
        const dx = x - cx;
        const dy = y - cy;
        const dist = Math.sqrt(dx*dx + dy*dy);
        
        let finalX = x;
        let finalY = y;
        
        if (dist > radius) {
          finalX = cx + (dx / dist) * radius;
          finalY = cy + (dy / dist) * radius;
        }

        if (!started) {
          ctxR.moveTo(finalX, finalY);
          started = true;
        } else {
          ctxR.lineTo(finalX, finalY);
        }
      }
      ctxR.stroke();
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  return (
    <div className="flex w-full h-[320px] gap-[24px]">
      {/* Left: Cylinders (40%) */}
      <div className="w-[40%] h-full bg-white rounded-[8px] border-[3px] border-[var(--border-color)] overflow-hidden relative">
        <canvas
          ref={leftCanvasRef}
          width={800}
          height={640}
          className="w-full h-full"
        />
      </div>
      
      {/* Right: Goniometer (60%) */}
      <div className="w-[60%] h-full bg-white rounded-[8px] border-[3px] border-[var(--border-color)] overflow-hidden relative">
        <canvas
          ref={rightCanvasRef}
          width={1200}
          height={640}
          className="w-full h-full"
        />
      </div>
    </div>
  );
}
