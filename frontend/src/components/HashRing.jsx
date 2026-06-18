import useStore from '../store/useStore';

const HashRing = () => {
  const { clusterState, dataRing } = useStore();

  const getOwnerNode = (keyAngle) => {
    const activeNodes = clusterState.filter(n => n.status === "alive");
    if (activeNodes.length === 0) return { color: "#64748b", id: "Offline" }; 
    let sortedNodes = [...activeNodes].sort((a, b) => a.angle - b.angle);
    for (let node of sortedNodes) {
      if (node.angle >= keyAngle) return node;
    }
    return sortedNodes[0]; 
  };

  return (
    <div className="w-1/2 flex items-center justify-center border-r border-slate-800 relative bg-slate-950 overflow-hidden">
      <div className="absolute inset-0 bg-[linear-gradient(to_right,#1e293b_1px,transparent_1px),linear-gradient(to_bottom,#1e293b_1px,transparent_1px)] bg-[size:2rem_2rem] opacity-20"></div>
      <div className="w-96 h-96 border-[3px] border-slate-700 rounded-full relative flex items-center justify-center z-10 shadow-[inset_0_0_50px_rgba(0,0,0,0.5)] bg-slate-900/50 backdrop-blur-sm">
        
        {/* Nodes Anchor */}
        {clusterState.map((node, i) => {
          const x = 192 * Math.cos((node.angle * Math.PI) / 180);
          const y = 192 * Math.sin((node.angle * Math.PI) / 180);
          return (
            <div key={`node-${i}`} style={{ transform: `translate(${x}px, ${y}px)`, backgroundColor: node.status === "alive" ? node.color : "#334155" }} className={`w-10 h-10 rounded-full absolute flex items-center justify-center text-[11px] font-bold border-4 border-slate-900 z-20 text-white transition-colors duration-500 ${node.status === "alive" ? "shadow-[0_0_20px_rgba(255,255,255,0.2)]" : "opacity-50"}`}>
              {node.id}
            </div>
          );
        })}

        {/* Data Keys */}
        {dataRing.map((item) => {
          const x = 192 * Math.cos((item.angle * Math.PI) / 180);
          const y = 192 * Math.sin((item.angle * Math.PI) / 180);
          const owner = getOwnerNode(item.angle);
          return (
            <div key={`key-${item.key}`} style={{ transform: `translate(${x}px, ${y}px)`, backgroundColor: owner.color, boxShadow: `0 0 12px ${owner.color}` }} className="w-2.5 h-2.5 rounded-full absolute z-15 border border-white/50 group cursor-pointer transition-all duration-500">
              <span className="absolute hidden group-hover:block -top-6 -left-4 bg-slate-800 text-white text-[10px] px-2 py-1 rounded shadow-lg whitespace-nowrap z-50">
                {item.key} ({owner.id})
              </span>
            </div>
          );
        })}

        <div className="flex flex-col items-center">
          <span className="text-slate-400 text-sm tracking-[0.3em] font-bold">HASH RING</span>
          <span className="text-cyan-500 text-[10px] mt-1 font-bold">KEYS: {dataRing.length}</span>
          <span className="text-emerald-500 text-[9px] mt-1 tracking-widest uppercase">Nodes: {clusterState.filter(n => n.status === "alive").length}</span>
        </div>
      </div>
    </div>
  );
};

export default HashRing;