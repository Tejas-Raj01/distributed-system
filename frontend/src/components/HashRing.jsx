import { useRef, useEffect } from 'react';
import useStore from '../store/useStore';

const HashRing = () => {
  const canvasRef = useRef(null);
  
  // NAYA: Animation loop ko track karne aur cancel karne ke liye
  const animationRef = useRef(null);
  
  // NAYA: Har dot ki live position track karne ke liye (Taaki React re-render se data ude nahi)
  const animatedDotsRef = useRef({}); 

  const { clusterState, dataRing } = useStore();

  // Malik (Owner Node) dhoondhne ka logic (For coloring the dots)
  const getOwnerNode = (keyAngle) => {
  const activeNodes = clusterState.filter(n => n.status === "alive");
  if (activeNodes.length === 0) return { color: "#64748b", id: "Offline", displayId: "Offline" }; 
  
  // Ring ke hisaab se nodes ko angle par sort kiya
  let sortedNodes = [...activeNodes].sort((a, b) => a.angle - b.angle);
  
  // 🚀 THE FIX: Asli sequential number nikalne ke liye nodes ko unke asli ID se sort kiya
  const allSortedById = [...clusterState].sort((a, b) => a.id - b.id);
  
  let targetNode = sortedNodes[0]; // Default fallback agar circle cross ho jaye
  
  for (let node of sortedNodes) {
    if (node.angle >= keyAngle) {
      targetNode = node;
      break;
    }
  }
  
  // Target node ka asli number pata lagana (e.g., 8080 -> Index 0 -> Node 1)
  const nodeIndex = allSortedById.findIndex(n => n.id === targetNode.id) + 1;
  
  // Node return karte waqt usme ek safe 'displayId' daal diya
  return { ...targetNode, displayId: `Node ${nodeIndex}` }; 
};

  // 🖌️ THE CANVAS LERP RENDERING ENGINE (Pillar 4 Animation)
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

    const animate = () => {
      // 1. Clear the canvas (Pichla frame saaf karo)
      ctx.clearRect(0, 0, width, height);

      // 2. Draw the Base Ring (Border)
      ctx.beginPath();
      ctx.arc(centerX, centerY, radius, 0, 2 * Math.PI);
      ctx.strokeStyle = '#334155'; // slate-700
      ctx.lineWidth = 3;
      ctx.stroke();

      // 3. Draw 1000+ Data Dots (WITH FLYING ANIMATION LERP)
      dataRing.forEach(item => {
        // Agar dot pehli baar aaya hai, toh current position set karo
        if (!animatedDotsRef.current[item.key]) {
          animatedDotsRef.current[item.key] = { currentAngle: item.angle };
        }
        
        const animDot = animatedDotsRef.current[item.key];
        const targetAngle = item.angle; // C++ Backend se aaya asli Target Angle

        // 🧠 LERP Math: Purane angle se naye angle tak smooth sliding!
        if (Math.abs(targetAngle - animDot.currentAngle) > 0.1) {
          // Speed: 0.08. Isko badhayenge toh dots tez udenge
          animDot.currentAngle += (targetAngle - animDot.currentAngle) * 0.08; 
        } else {
          animDot.currentAngle = targetAngle; // Target par pahunch gaya
        }

        // Color hamesha target owner ka hoga (Taaki udte waqt color change dikhe)
        const owner = getOwnerNode(targetAngle);
        
        // Convert LIVE animated degrees to Radians
        const rad = animDot.currentAngle * (Math.PI / 180);
        
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
        ctx.fillText(node.id || node.port, x, y);
      });

      // Agla frame paint karne ke liye loop ko call karo (60 FPS)
      animationRef.current = requestAnimationFrame(animate);
    };

    // Purana frame loop cancel karo aur naya start karo
    if (animationRef.current) cancelAnimationFrame(animationRef.current);
    animate();

    // Cleanup when component unmounts
    return () => {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
    };

  }, [clusterState, dataRing]); // React state change hone par naya target set hoga

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