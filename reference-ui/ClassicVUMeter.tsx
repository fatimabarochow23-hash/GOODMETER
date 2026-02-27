import React, { useEffect, useRef } from 'react';
import { engine } from '../../lib/audio';

export function ClassicVUMeter() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animationId: number;
    let currentVuDisplay = 0;
    const vuSmoothing = 0.08;

    const draw = () => {
      animationId = requestAnimationFrame(draw);
      
      if (!engine.isInitialized) return;
      engine.updateData();
      
      // 1. Calculate linear RMS
      let sumL = 0;
      let sumR = 0;
      for (let i = 0; i < engine.timeDataL.length; i++) {
        sumL += engine.timeDataL[i] * engine.timeDataL[i];
        sumR += engine.timeDataR[i] * engine.timeDataR[i];
      }
      const rmsL = Math.sqrt(sumL / engine.timeDataL.length);
      const rmsR = Math.sqrt(sumR / engine.timeDataR.length);
      const rms = Math.max(rmsL, rmsR);
      
      // 2. Strict VU math
      let dbfs = 20 * Math.log10(rms + 0.00001);
      let vu = dbfs; // Direct 1:1 mapping to dBFS
      
      // Range: -30 VU to +3 VU
      const minVu = -30;
      const maxVu = 3;
      
      let targetLevel = (vu - minVu) / (maxVu - minVu);
      targetLevel = Math.max(0, Math.min(1, targetLevel));

      // 3. Apply ballistics (smoothing)
      currentVuDisplay += (targetLevel - currentVuDisplay) * vuSmoothing;

      const width = canvas.width;
      const height = canvas.height;
      
      // 4. Wide, flat arc geometry
      const centerX = width / 2;
      const centerY = height * 2.5; // Pivot point far below
      const radius = height * 2.2;  // Large radius for flat arc

      ctx.clearRect(0, 0, width, height);

      // Background
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(0, 0, width, height);

      // Angles
      const spread = 0.65; // Radians
      const startAngle = -Math.PI / 2 - spread;
      const endAngle = -Math.PI / 2 + spread;
      const zeroVuAngle = startAngle + ((0 - minVu) / (maxVu - minVu)) * (endAngle - startAngle);

      // Draw normal arc (-30 to 0)
      ctx.beginPath();
      ctx.arc(centerX, centerY, radius, startAngle, zeroVuAngle);
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 6; // Thick line (scales to ~3px in CSS)
      ctx.stroke();

      // Draw danger arc (0 to +3)
      ctx.beginPath();
      ctx.arc(centerX, centerY, radius, zeroVuAngle, endAngle);
      ctx.strokeStyle = '#E6335F';
      ctx.lineWidth = 6;
      ctx.stroke();

      // Draw ticks and labels
      const ticks = [-30, -20, -10, -5, -3, -1, 0, 1, 2, 3];

      ctx.textAlign = 'center';
      ctx.textBaseline = 'top';
      ctx.font = 'bold 36px sans-serif';

      ticks.forEach(tickVu => {
        const t = (tickVu - minVu) / (maxVu - minVu);
        const angle = startAngle + t * (endAngle - startAngle);
        
        const isDanger = tickVu > 0;
        const isZero = tickVu === 0;
        
        const tickLength = isZero ? 36 : 20;
        const innerRadius = radius - tickLength;
        
        const x1 = centerX + Math.cos(angle) * radius;
        const y1 = centerY + Math.sin(angle) * radius;
        const x2 = centerX + Math.cos(angle) * innerRadius;
        const y2 = centerY + Math.sin(angle) * innerRadius;

        ctx.beginPath();
        ctx.moveTo(x1, y1);
        ctx.lineTo(x2, y2);
        ctx.strokeStyle = isDanger ? '#E6335F' : '#2A2A35';
        ctx.lineWidth = isZero ? 8 : 6;
        ctx.stroke();

        // Labels
        const labelRadius = radius - tickLength - 16;
        const lx = centerX + Math.cos(angle) * labelRadius;
        const ly = centerY + Math.sin(angle) * labelRadius;
        ctx.fillStyle = isDanger ? '#E6335F' : '#2A2A35';
        
        const labelText = tickVu > 0 ? `+${tickVu}` : `${tickVu}`;
        ctx.fillText(labelText, lx, ly);
      });

      // Draw VU text
      ctx.fillStyle = '#2A2A35';
      ctx.font = '900 48px sans-serif';
      ctx.fillText('VU', centerX, height - 80);

      // Draw needle
      const needleAngle = startAngle + currentVuDisplay * (endAngle - startAngle);
      const needleLength = radius + 32; // Extend slightly past the arc
      
      const nx = centerX + Math.cos(needleAngle) * needleLength;
      const ny = centerY + Math.sin(needleAngle) * needleLength;

      ctx.beginPath();
      ctx.moveTo(centerX, centerY); // From virtual center
      ctx.lineTo(nx, ny);
      ctx.strokeStyle = '#2A2A35';
      ctx.lineWidth = 8; // Extremely thick needle (scales to ~4px in CSS)
      ctx.lineCap = 'butt';
      ctx.stroke();
    };

    draw();

    return () => cancelAnimationFrame(animationId);
  }, []);

  return (
    <div className="w-full h-[180px] relative">
      <canvas
        ref={canvasRef}
        width={1600}
        height={480}
        className="w-full h-full"
      />
    </div>
  );
}
