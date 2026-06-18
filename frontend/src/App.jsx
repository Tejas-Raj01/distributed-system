import { useState, useRef, useEffect } from 'react';

// JavaScript version of 64-bit FNV-1a Hash Algorithm
const computeHashAngle = (str) => {
  let hash = 14695981039346656037n;
  for (let i = 0; i < str.length; i++) {
      hash ^= BigInt(str.charCodeAt(i));
      hash = (hash * 1099511628211n) % 18446744073709551616n;
  }
  return Number(hash % 360n);
};

function App() {
  const [keyInput, setKeyInput] = useState("");
  const [valInput, setValInput] = useState("");
  
  // ==========================================
  // 🧠 THE REACT STATE MANAGEMENT (DIMAAG)
  // ==========================================
  const [nodes, setNodes] = useState([
    { id: 8080, angle: 0, color: "#06b6d4", status: "alive" },
    { id: 8082, angle: 120, color: "#d946ef", status: "alive" },
    { id: 8084, angle: 240, color: "#ef4444", status: "alive" }
  ]);

  const [keys, setKeys] = useState([]); // [{ keyName: 'User_1', angle: 45 }]

  const [logs, setLogs] = useState([
    "> [INFO] Cluster UI initialized...",
    "> [ROUTER] Ready for inputs..."
  ]);

  const logsEndRef = useRef(null);
  const addLog = (msg) => setLogs(prev => [...prev, `> ${msg}`]);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [logs]);

  // Dynamic Consistent Hashing Upper Bound Logic
  const getOwnerNode = (keyAngle) => {
    const activeNodes = nodes.filter(n => n.status === "alive");
    if (activeNodes.length === 0) return { color: "#64748b", id: "Offline" }; 
    
    let sortedNodes = [...activeNodes].sort((a, b) => a.angle - b.angle);
    for (let node of sortedNodes) {
      if (node.angle >= keyAngle) return node;
    }
    return sortedNodes[0]; 
  };

  // --- API CALLS & MUTATIONS ---

  const handlePut = async () => {
    if (!keyInput || !valInput) return;
    const currentKey = keyInput; 
    const calculatedAngle = computeHashAngle(currentKey);

    try {
      addLog(`[ROUTER] Sending PUT Request: '${keyInput}'`);
      const response = await fetch('http://127.0.0.1:8080/put', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `key=${keyInput}&value=${valInput}`
      });
      const data = await response.json();
      
      if (data.status === "success") {
        addLog(`[QUORUM] SUCCESS: Key '${currentKey}' saved.`);
        setKeys(prev => {
          const filtered = prev.filter(k => k.keyName !== currentKey);
          return [...filtered, { keyName: currentKey, angle: calculatedAngle }];
        });
      }
      setKeyInput("");
      setValInput("");
    } catch (err) {
      addLog("[CRITICAL] Backend unreachable.");
    }
  };

  const handleGet = async () => {
    if (!keyInput) return;
    try {
      addLog(`[ROUTER] Fetching data for Key: '${keyInput}'...`);
      const response = await fetch(`http://127.0.0.1:8080/get?key=${keyInput}`);
      const textData = await response.text();
      if (response.status === 200) {
        addLog(`[STORAGE] Found Value: '${textData}'`);
        setValInput(textData);
      } else {
        addLog(`[STORAGE] ${textData}`);
      }
    } catch (err) {
      addLog("[CRITICAL] Backend unreachable.");
    }
  };

  const handleDelete = async () => {
    if (!keyInput) return;
    try {
      addLog(`[ROUTER] Sending DELETE Request for: '${keyInput}'...`);
      const response = await fetch(`http://127.0.0.1:8080/delete?key=${keyInput}`, {
        method: 'DELETE'
      });
      if (response.status === 200) {
        addLog(`[QUORUM] SUCCESS: Key '${keyInput}' deleted.`);
        setKeys(prev => prev.filter(k => k.keyName !== keyInput));
        setKeyInput("");
        setValInput("");
      } else {
        const errText = await response.text();
        addLog(`[STORAGE] ${errText}`);
      }
    } catch (err) {
      addLog("[CRITICAL] Backend unreachable.");
    }
  };

  const handleInject = async () => {
    addLog("[CHAOS] 🚀 Injecting 150 random keys into cluster...");
    for(let i=0; i<150; i++) {
      const randomKey = `user_${Math.floor(Math.random() * 99999)}`;
      const randomVal = `data_${i}`;
      const calculatedAngle = computeHashAngle(randomKey);

      fetch('http://127.0.0.1:8080/put', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: `key=${randomKey}&value=${randomVal}`
      }).then(res => res.json()).then(data => {
        if (data.status === "success") {
          setKeys(prev => [...prev.filter(k => k.keyName !== randomKey), { keyName: randomKey, angle: calculatedAngle }]);
        }
      }).catch(err => {});
    }
  };

  const handleRebalance = async () => {
    addLog("[ADMIN] ⚖️ Triggering Cluster Rebalance...");
    try {
      const response = await fetch('http://127.0.0.1:8080/admin/rebalance');
      const data = await response.json();
      addLog(`[REBALANCE] Process completed. Keys transferred: ${data.keys_transferred}`);
    } catch (err) {
      addLog("[CRITICAL] Rebalance failed.");
    }
  };

  const handleAddNode = () => {
    const newNode = { id: 8086, angle: 60, color: "#f59e0b", status: "alive" };
    if (nodes.find(n => n.id === newNode.id)) {
      addLog("[WARNING] Node 8086 is already connected.");
      return;
    }
    addLog("[INFO] ➕ New Node 8086 added to UI ring structure.");
    setNodes(prev => [...prev, newNode]);
  };

  const toggleNodeStatus = (targetId) => {
    setNodes(prev => prev.map(n => {
      if (n.id === targetId) {
        const nextStatus = n.status === "alive" ? "dead" : "alive";
        addLog(`[GOSSIP] Node ${targetId} marked as ${nextStatus.toUpperCase()}.`);
        return { ...n, status: nextStatus };
      }
      return n;
    }));
  };

  return (
    <div className="h-screen w-screen bg-slate-950 text-white flex overflow-hidden">
      
      {/* ⬅️ Left Panel */}
      <div className="w-1/4 border-r border-slate-800 p-6 flex flex-col bg-slate-900 shadow-2xl z-10 overflow-y-auto hide-scrollbar">
        <h1 className="text-cyan-400 font-bold text-2xl tracking-wider">CONTROL CENTER</h1>
        
        {/* CRUD */}
        <div className="flex flex-col gap-4 mt-8">
          <h2 className="text-slate-500 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-slate-800 pb-2">Manual Operations</h2>
          <input className="bg-slate-950 p-3 border border-slate-700 rounded text-white focus:outline-none focus:border-cyan-500 transition" placeholder="Key" value={keyInput} onChange={(e) => setKeyInput(e.target.value)} />
          <input className="bg-slate-950 p-3 border border-slate-700 rounded text-white focus:outline-none focus:border-cyan-500 transition" placeholder="Value" value={valInput} onChange={(e) => setValInput(e.target.value)} />
          <div className="flex gap-2">
            <button onClick={handlePut} className="bg-cyan-500 px-2 py-3 rounded text-slate-950 font-bold hover:bg-cyan-400 transition w-1/3 text-sm">PUT</button>
            <button onClick={handleGet} className="bg-slate-700 px-2 py-3 rounded text-white hover:bg-slate-600 transition w-1/3 font-bold border border-slate-600 text-sm">GET</button>
            <button onClick={handleDelete} className="bg-red-900/50 px-2 py-3 rounded text-red-400 hover:bg-red-800/80 hover:text-white transition w-1/3 font-bold border border-red-700/50 text-sm">DEL</button>
          </div>
        </div>

        {/* CHAOS */}
        <div className="flex flex-col gap-3 mt-8">
          <h2 className="text-purple-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-purple-900/50 pb-2">Chaos & Automation</h2>
          <button onClick={handleInject} className="bg-purple-600/20 border border-purple-500/50 text-purple-300 px-4 py-3 rounded font-bold hover:bg-purple-600/40 transition text-sm">🚀 INJECT 150 KEYS</button>
          <button onClick={handleRebalance} className="bg-emerald-600/20 border border-emerald-500/50 text-emerald-400 px-4 py-3 rounded font-bold hover:bg-emerald-600/40 transition text-sm">⚖️ REBALANCE CLUSTER</button>
          <button onClick={handleAddNode} className="bg-amber-600/20 border border-amber-500/50 text-amber-400 px-4 py-3 rounded font-bold hover:bg-amber-600/40 transition text-sm">➕ CONNECT NODE 8086</button>
        </div>

        {/* STATUS */}
        <div className="flex flex-col gap-3 mt-8 pb-4">
          <h2 className="text-red-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-red-900/50 pb-2">Cluster Status</h2>
          {nodes.map(node => (
            <div key={node.id} className="flex justify-between items-center bg-slate-950 p-3 rounded border border-slate-800">
              <div className="flex items-center gap-2">
                <span className={`w-3 h-3 rounded-full ${node.status === "alive" ? "animate-pulse" : ""}`} style={{ backgroundColor: node.status === "alive" ? node.color : "#334155" }}></span>
                <span className={`font-mono ${node.status === "alive" ? "text-white" : "text-slate-500 line-through"}`}>Node {node.id}</span>
              </div>
              <button 
                onClick={() => toggleNodeStatus(node.id)}
                className={`text-[10px] font-bold px-3 py-1 rounded border ${node.status === "alive" ? "border-red-500/50 text-red-400 hover:bg-red-900/30" : "border-green-500/50 text-green-400 hover:bg-green-900/30"}`}
              >
                {node.status === "alive" ? "☠️ KILL" : "➕ REVIVE"}
              </button>
            </div>
          ))}
        </div>
      </div>

      {/* ⏺️ Center Panel */}
      <div className="w-1/2 flex items-center justify-center border-r border-slate-800 relative bg-slate-950">
        <div className="absolute inset-0 bg-[linear-gradient(to_right,#1e293b_1px,transparent_1px),linear-gradient(to_bottom,#1e293b_1px,transparent_1px)] bg-[size:2rem_2rem] opacity-20"></div>
        
        <div className="w-96 h-96 border-[3px] border-slate-700 rounded-full relative flex items-center justify-center z-10 shadow-[inset_0_0_50px_rgba(0,0,0,0.5)] bg-slate-900/50 backdrop-blur-sm">
          
          {/* Physical Nodes Anchor */}
          {nodes.map((node, i) => {
            const x = 192 * Math.cos((node.angle * Math.PI) / 180);
            const y = 192 * Math.sin((node.angle * Math.PI) / 180);
            return (
              <div 
                key={`node-${i}`}
                style={{ transform: `translate(${x}px, ${y}px)`, backgroundColor: node.status === "alive" ? node.color : "#334155" }}
                className={`w-10 h-10 rounded-full absolute flex items-center justify-center text-[11px] font-bold border-4 border-slate-900 z-20 text-white transition-colors duration-500 ${node.status === "alive" ? "shadow-[0_0_20px_rgba(255,255,255,0.2)]" : "opacity-50"}`}
              >
                {node.id}
              </div>
            );
          })}

          {/* Data Keys Tracker */}
          {keys.map((item, i) => {
            const x = 192 * Math.cos((item.angle * Math.PI) / 180);
            const y = 192 * Math.sin((item.angle * Math.PI) / 180);
            const owner = getOwnerNode(item.angle);
            return (
              <div 
                key={`key-${i}`}
                style={{ transform: `translate(${x}px, ${y}px)`, backgroundColor: owner.color, boxShadow: `0 0 12px ${owner.color}` }}
                className="w-2.5 h-2.5 rounded-full absolute z-15 border border-white/50 group cursor-pointer transition-all duration-500"
              >
                <span className="absolute hidden group-hover:block -top-6 -left-4 bg-slate-800 text-white text-[10px] px-2 py-1 rounded shadow-lg whitespace-nowrap z-50">
                  {item.keyName} ({owner.id})
                </span>
              </div>
            );
          })}

          <div className="flex flex-col items-center">
            <span className="text-slate-400 text-sm tracking-[0.3em] font-bold">HASH RING</span>
            <span className="text-cyan-500 text-[10px] mt-1 font-bold">KEYS: {keys.length}</span>
          </div>
        </div>
      </div>

      {/* ➡️ Right Panel */}
      <div className="w-1/4 bg-black p-0 flex flex-col z-10 border-l border-slate-800">
        <div className="bg-slate-900 p-4 border-b border-slate-800">
          <h2 className="text-green-500 text-sm font-bold tracking-widest flex items-center gap-2">
            <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
            SYSTEM TERMINAL
          </h2>
        </div>
        <div className="p-4 overflow-y-auto flex-1 text-[13px] space-y-3 font-mono">
          {logs.map((log, index) => (
            <p key={index} className={
              log.includes("ERROR") || log.includes("CRITICAL") ? "text-red-400" : 
              log.includes("SUCCESS") ? "text-cyan-400 font-bold" : 
              log.includes("CHAOS") ? "text-purple-400 font-bold" : 
              log.includes("ADMIN") ? "text-emerald-400 font-bold" : 
              log.includes("GOSSIP") ? "text-yellow-400 font-bold" : 
              log.includes("INFO") ? "text-blue-400 font-bold" : 
              "text-green-500 opacity-90"
            }>
              {log}
            </p>
          ))}
          <div ref={logsEndRef} />
        </div>
      </div>

    </div>
  );
}

export default App;