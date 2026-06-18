import { useRef, useEffect } from 'react';
import useStore from '../store/useStore';

const HashRing = () => {
  const canvasRef = useRef(null);
  const { clusterState, dataRing } = useStore();

  // Malik (Owner Node) dhoondhne ka logic
  const getOwnerNode = (keyAngle) => {
    const activeNodes = clusterState.filter(n => n.status === "alive");
    if (activeNodes.length === 0) return { color: "#64748b", id: "Offline" }; 
    let sortedNodes = [...activeNodes].sort((a, b) => a.angle - b.angle);
    for (let node of sortedNodes) {
      if (node.angle >= keyAngle) return node;
    }
    return sortedNodes[0]; 
  };

  // 🖌️ THE CANVAS RENDERING ENGINE
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // Canvas Dimensions
    const width = canvas.width;
    const height = canvas.height;
    const centerX = width / 2;
    const centerY = height / 2;
    const radius = 170; // Circle ki radius

    // 1. Clear the canvas (Pichla frame saaf karo)
    ctx.clearRect(0, 0, width, height);

    // 2. Draw the Base Ring (Border)
    ctx.beginPath();
    ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
    ctx.strokeStyle = '#334155'; // slate-700
    ctx.lineWidth = 3;
    ctx.stroke();

    // 3. Draw 1000+ Data Dots (The Math Heavy Part)
    dataRing.forEach(item => {
      const owner = getOwnerNode(item.angle);
      // Convert degrees to Radians
      const rad = item.angle * (Math.PI / 180);
      
      // Trigonometry se (X, Y) coordinates nikalna
      const x = centerX + radius * Math.cos(rad);
      const y = centerY + radius * Math.sin(rad);

      ctx.beginPath();
      ctx.arc(x, y, 3, 0, 2 * Math.PI); // 3px ki dot
      ctx.fillStyle = owner.color;
      
      // Glowing Effect
      ctx.shadowBlur = 8;
      ctx.shadowColor = owner.color;
      
      ctx.fill();
      
      // Reset shadow for next items
      ctx.shadowBlur = 0; 
    });

    // 4. Draw Physical Server Nodes
    clusterState.forEach(node => {
      const rad = node.angle * (Math.PI / 180);
      const x = centerX + radius * Math.cos(rad);
      const y = centerY + radius * Math.sin(rad);

      // Node Circle
      ctx.beginPath();
      ctx.arc(x, y, 18, 0, 2 * Math.PI); // 18px bada circle
      ctx.fillStyle = node.status === 'alive' ? node.color : '#334155'; // Dead hai toh gray
      ctx.fill();
      
      // Node Border
      ctx.lineWidth = 4;
      ctx.strokeStyle = '#0f172a'; // slate-900
      ctx.stroke();

      // Node ID Text
      ctx.fillStyle = '#ffffff';
      ctx.font = 'bold 11px monospace';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(node.id, x, y);
    });

  }, [clusterState, dataRing]); // Jab bhi state change ho, canvas re-render hoga!

  return (
    <div className="w-1/2 flex items-center justify-center border-r border-slate-800 relative bg-slate-950 overflow-hidden">
      {/* Background Grid */}
      <div className="absolute inset-0 bg-[linear-gradient(to_right,#1e293b_1px,transparent_1px),linear-gradient(to_bottom,#1e293b_1px,transparent_1px)] bg-[size:2rem_2rem] opacity-20"></div>
      
      <div className="relative flex items-center justify-center z-10">
        {/* HTML5 Canvas */}
        <canvas 
          ref={canvasRef} 
          width={400} 
          height={400} 
          className="rounded-full shadow-[inset_0_0_50px_rgba(0,0,0,0.5)] bg-slate-900/50 backdrop-blur-sm"
        />

        {/* Center Text Layout overlayed on top of Canvas */}
        <div className="absolute flex flex-col items-center pointer-events-none">
          <span className="text-slate-400 text-sm tracking-[0.3em] font-bold drop-shadow-md">HASH RING</span>
          <span className="text-cyan-500 text-[10px] mt-1 font-bold">KEYS: {dataRing.length}</span>
          <span className="text-emerald-500 text-[9px] mt-1 tracking-widest uppercase">Nodes: {clusterState.filter(n => n.status === "alive").length}</span>
        </div>
      </div>
    </div>
  );
};

export default HashRing;