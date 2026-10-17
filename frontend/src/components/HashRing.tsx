import { useRef, useEffect } from 'react';
import useStore from '../store/useStore';

const HashRing = () => {
  const canvasRef = useRef(null);
  
  // Animation frame reference for cleanup
  const animationRef = useRef(null);
  
  // Persistent tracking of live dot positions across renders
  const animatedDotsRef = useRef({}); 

  const { clusterState, dataRing } = useStore();

  // Determines owner node on the ring to color data dots
  const getOwnerNode = (keyAngle) => {
  const activeNodes = clusterState.filter(n => n.status === "alive");
  if (activeNodes.length === 0) return { color: "#64748b", id: "Offline", displayId: "Offline" }; 
  
  // Sort active nodes by angular position on ring
  let sortedNodes = [...activeNodes].sort((a, b) => a.angle - b.angle);
  
  // Sort by ID for deterministic display index
  const allSortedById = [...clusterState].sort((a, b) => a.id - b.id);
  
  let targetNode = sortedNodes[0]; // Wrap-around fallback
  
  for (let node of sortedNodes) {
    if (node.angle >= keyAngle) {
      targetNode = node;
      break;
    }
  }
  
  // 1-based display index (e.g., 8080 -> Node 1)
  const nodeIndex = allSortedById.findIndex(n => n.id === targetNode.id) + 1;
  
  return { ...targetNode, displayId: `Node ${nodeIndex}` }; 
};

  // Canvas LERP Rendering Engine
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    // Canvas Dimensions
    const width = canvas.width;
    const height = canvas.height;
    const centerX = width / 2;
    const centerY = height / 2;
    const radius = 170; // Ring radius

    const animate = () => {
      // 1. Clear the canvas
      ctx.clearRect(0, 0, width, height);

      // 2. Draw base ring border
      ctx.beginPath();
      ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
      ctx.strokeStyle = '#334155'; // slate-700
      ctx.lineWidth = 3;
      ctx.stroke();

      // 3. Draw data dots with LERP animation
      dataRing.forEach(item => {
        // Initialize position on first appearance
        if (!animatedDotsRef.current[item.key]) {
          animatedDotsRef.current[item.key] = { currentAngle: item.angle };
        }
        
        const animDot = animatedDotsRef.current[item.key];
        const targetAngle = item.angle;

        // LERP for smooth interpolation towards target angle
        if (Math.abs(targetAngle - animDot.currentAngle) > 0.1) {
          animDot.currentAngle += (targetAngle - animDot.currentAngle) * 0.08; 
        } else {
          animDot.currentAngle = targetAngle;
        }

        // Color dots according to target owner node
        const owner = getOwnerNode(targetAngle);
        
        // Convert animated degrees to radians
        const rad = animDot.currentAngle * (Math.PI / 180);
        
        // Compute Cartesian coordinates
        const x = centerX + radius * Math.cos(rad);
        const y = centerY + radius * Math.sin(rad);

        ctx.beginPath();
        ctx.arc(x, y, 3, 0, 2 * Math.PI);
        ctx.fillStyle = owner.color;
        
        // Glow effect
        ctx.shadowBlur = 8;
        ctx.shadowColor = owner.color;
        
        ctx.fill();
        
        // Reset shadow for subsequent items
        ctx.shadowBlur = 0; 
      });

      // 4. Draw physical server nodes
      clusterState.forEach(node => {
        const rad = node.angle * (Math.PI / 180);
        const x = centerX + radius * Math.cos(rad);
        const y = centerY + radius * Math.sin(rad);

        // Node Circle
        ctx.beginPath();
        ctx.arc(x, y, 18, 0, 2 * Math.PI);
        ctx.fillStyle = node.status === 'alive' ? node.color : '#334155';
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
        ctx.fillText(node.id || node.port, x, y);
      });

      // Request next animation frame (60 FPS)
      animationRef.current = requestAnimationFrame(animate);
    };

    // Cancel previous animation loop before starting new one
    if (animationRef.current) cancelAnimationFrame(animationRef.current);
    animate();

    // Cleanup when component unmounts
    return () => {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
    };

  }, [clusterState, dataRing]);

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