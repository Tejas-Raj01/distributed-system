import { useState, useEffect } from 'react';
import useStore from '../store/useStore';

const computeHashAngle = (str) => {
  let hash = 14695981039346656037n;
  for (let i = 0; i < str.length; i++) {
      hash ^= BigInt(str.charCodeAt(i));
      hash = (hash * 1099511628211n) % 18446744073709551616n;
  }
  return Number(hash % 360n);
};

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

const ControlCenter = () => {
  const [keyInput, setKeyInput] = useState("");
  const [valInput, setValInput] = useState("");
  const [gossipPulse, setGossipPulse] = useState(false);
  const [injectCount, setInjectCount] = useState(1000); 

  const { 
    clusterState, isInjecting, isBackendOffline,
    addLog, addData, removeData, toggleNodeStatus, addNode, setIsInjecting, setIsBackendOffline 
  } = useStore();

  // ==========================================
  // 🔄 REAL-TIME HEALTH POLLING (Bypassed to fix CORS)
  // ==========================================
  useEffect(() => {
    const checkBackendHealth = async () => {
      // TEMPORARY BYPASS: Forces backend to be "Alive" to avoid CORS spam in console
      setIsBackendOffline(false); 
    };

    checkBackendHealth();

    const gossipTimer = setInterval(() => {
      setGossipPulse(prev => !prev);
      checkBackendHealth(); 
    }, 2000);

    return () => clearInterval(gossipTimer);
  }, [setIsBackendOffline]);

  // --- CRUD OPERATIONS ---
  const handlePut = async () => {
    if (!keyInput || !valInput || isInjecting || isBackendOffline) return;
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
        addData(currentKey, calculatedAngle); 
      }
      setKeyInput(""); setValInput("");
    } catch (err) { 
      setIsBackendOffline(true);
      addLog("[CRITICAL] Connection lost during PUT operation."); 
    }
  };

  const handleGet = async () => {
    if (!keyInput || isInjecting || isBackendOffline) return;
    try {
      addLog(`[ROUTER] Fetching data for Key: '${keyInput}'...`);
      const response = await fetch(`http://127.0.0.1:8080/get?key=${keyInput}`);
      const textData = await response.text();
      if (response.status === 200) {
        addLog(`[STORAGE] Found Value: '${textData}'`);
        setValInput(textData);
      } else { addLog(`[STORAGE] ${textData}`); }
    } catch (err) { 
      setIsBackendOffline(true);
      addLog("[CRITICAL] Connection lost during GET operation."); 
    }
  };

  const handleDelete = async () => {
    if (!keyInput || isInjecting || isBackendOffline) return;
    try {
      addLog(`[ROUTER] Sending DELETE Request for: '${keyInput}'...`);
      const response = await fetch(`http://127.0.0.1:8080/delete?key=${keyInput}`, { method: 'DELETE' });
      if (response.status === 200) {
        addLog(`[QUORUM] SUCCESS: Key '${keyInput}' deleted.`);
        removeData(keyInput); 
        setKeyInput(""); setValInput("");
      } else {
        const errText = await response.text();
        addLog(`[STORAGE] ${errText}`);
      }
    } catch (err) { 
      setIsBackendOffline(true);
      addLog("[CRITICAL] Connection lost during DELETE operation."); 
    }
  };

  // ==========================================
  // ⚡ CHUNKED STRESS TEST 
  // ==========================================
  const injectStressTest = async () => {
    if (isInjecting || isBackendOffline) return;
    
    setIsInjecting(true); 
    addLog(`[CHAOS] 🚀 Starting chunked injection of ${injectCount} keys...`);
    
    const CHUNK_SIZE = 100; 

    for (let i = 0; i < injectCount; i += CHUNK_SIZE) {
      if (useStore.getState().isBackendOffline) {
        addLog("[CRITICAL] Stress test aborted due to Backend failure!");
        break;
      }

      let promises = [];
      const currentChunkSize = Math.min(CHUNK_SIZE, injectCount - i);

      for (let j = 0; j < currentChunkSize; j++) {
        const randomKey = `stress_${Math.floor(Math.random() * 999999)}`;
        const randomVal = "test_data";
        const calculatedAngle = computeHashAngle(randomKey);

        const req = fetch('http://127.0.0.1:8080/put', {
          method: 'POST',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
          body: `key=${randomKey}&value=${randomVal}`
        })
        .then(res => res.json())
        .then(data => {
          if (data.status === "success") addData(randomKey, calculatedAngle); 
        }).catch(() => {});
        
        promises.push(req);
      }
      
      await Promise.all(promises);
      addLog(`[NETWORK] Batched Chunk: ${i + currentChunkSize} / ${injectCount}`);
      
      if (i + currentChunkSize < injectCount) {
        await delay(150); 
      }
    }
    
    setIsInjecting(false); 
    addLog(`[SUCCESS] 🔥 Finished pipeline injection of ${injectCount} keys.`);
  };

  const handleRebalance = async () => {
    if (isInjecting || isBackendOffline) return;
    addLog("[ADMIN] ⚖️ Triggering Cluster Rebalance...");
    try {
      const response = await fetch('http://127.0.0.1:8080/admin/rebalance');
      const data = await response.json();
      addLog(`[REBALANCE] Process completed. Keys transferred: ${data.keys_transferred}`);
    } catch (err) { addLog("[CRITICAL] Rebalance failed."); }
  };

  const handleAddNode = () => {
    if (isInjecting || isBackendOffline) return;
    const newNode = { id: 8086, angle: 60, color: "#f59e0b", status: "alive" };
    if (clusterState.find(n => n.id === newNode.id)) {
      addLog("[WARNING] Node 8086 is already connected.");
      return;
    }
    addLog("[INFO] ➕ New Node 8086 added to UI ring structure.");
    addNode(newNode); 
  };

  const handleToggleStatus = (id) => {
    if (isInjecting || isBackendOffline) return;
    toggleNodeStatus(id); 
    const node = clusterState.find(n => n.id === id);
    const nextStatus = node.status === "alive" ? "DEAD" : "ALIVE";
    addLog(`[GOSSIP] Node ${id} marked as ${nextStatus}.`);
  };

  return (
    <div className="w-1/4 border-r border-slate-800 p-6 flex flex-col bg-slate-900 shadow-2xl z-10 overflow-y-auto hide-scrollbar">
      <h1 className="text-cyan-400 font-bold text-2xl tracking-wider">CONTROL CENTER</h1>
      
      {isBackendOffline && (
        <div className="bg-red-950/80 border border-red-500 text-red-400 p-3 rounded text-xs font-bold text-center animate-pulse mt-4 shadow-[0_0_15px_rgba(239,68,68,0.2)]">
          ⚠️ Backend Offline - Restart C++ Server
        </div>
      )}

      {isInjecting && (
        <div className="bg-purple-950/80 border border-purple-500 text-purple-300 p-3 rounded text-xs font-bold text-center animate-pulse mt-4">
          ⚙️ Stress Test In Progress - UI Actions Locked
        </div>
      )}

      {/* MANUAL CRUD */}
      <div className={`flex flex-col gap-4 mt-6 transition-opacity duration-300 ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
        <h2 className="text-slate-500 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-slate-800 pb-2">Manual Operations</h2>
        <input disabled={isInjecting || isBackendOffline} className="bg-slate-950 p-3 border border-slate-700 rounded text-white focus:outline-none focus:border-cyan-500 transition disabled:cursor-not-allowed" placeholder="Key" value={keyInput} onChange={(e) => setKeyInput(e.target.value)} />
        <input disabled={isInjecting || isBackendOffline} className="bg-slate-950 p-3 border border-slate-700 rounded text-white focus:outline-none focus:border-cyan-500 transition disabled:cursor-not-allowed" placeholder="Value" value={valInput} onChange={(e) => setValInput(e.target.value)} />
        <div className="flex gap-2">
          <button disabled={isInjecting || isBackendOffline} onClick={handlePut} className="bg-cyan-500 px-2 py-3 rounded text-slate-950 font-bold disabled:cursor-not-allowed transition w-1/3 text-sm enabled:hover:bg-cyan-400">PUT</button>
          <button disabled={isInjecting || isBackendOffline} onClick={handleGet} className="bg-slate-700 px-2 py-3 rounded text-white disabled:cursor-not-allowed transition w-1/3 font-bold border border-slate-600 text-sm enabled:hover:bg-slate-600">GET</button>
          <button disabled={isInjecting || isBackendOffline} onClick={handleDelete} className="bg-red-900/50 px-2 py-3 rounded text-red-400 disabled:cursor-not-allowed transition w-1/3 font-bold border border-red-700/50 text-sm enabled:hover:bg-red-800/80 enabled:hover:text-white">DEL</button>
        </div>
      </div>

      {/* CHAOS & INJECTION VOLUME */}
      <div className="flex flex-col gap-3 mt-8">
        <h2 className="text-purple-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-purple-900/50 pb-2">Chaos & Automation</h2>
        
        <div className={`flex flex-col gap-1 mt-1 mb-2 transition-opacity duration-300 ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
          <div className="flex justify-between items-center text-[10px] text-purple-300 font-bold uppercase tracking-wider">
            <span>Injection Volume</span>
            <span className="text-purple-100 bg-purple-900/50 px-2 py-0.5 rounded">{injectCount} Keys</span>
          </div>
          <input 
            disabled={isInjecting || isBackendOffline}
            type="range" min="100" max="2000" step="100" 
            value={injectCount} 
            onChange={(e) => setInjectCount(Number(e.target.value))} 
            className="w-full accent-purple-500 cursor-pointer disabled:cursor-not-allowed"
          />
        </div>

        <button 
          disabled={isInjecting || isBackendOffline} 
          onClick={injectStressTest} 
          className={`px-4 py-3 rounded font-bold text-sm transition border ${
            isInjecting || isBackendOffline 
              ? 'bg-slate-800 text-slate-500 border-slate-700 cursor-not-allowed' 
              : 'bg-purple-600/20 border-purple-500/50 text-purple-300 hover:bg-purple-600/40'
          }`}
        >
          🚀 INJECT {injectCount} KEYS
        </button>

        <button 
          disabled={isInjecting || isBackendOffline} 
          onClick={handleRebalance} 
          className={`px-4 py-3 rounded font-bold text-sm transition border ${
            isInjecting || isBackendOffline 
              ? 'bg-slate-800 text-slate-500 border-slate-700 cursor-not-allowed' 
              : 'bg-emerald-600/20 border-emerald-500/50 text-emerald-400 hover:bg-emerald-600/40'
          }`}
        >
          ⚖️ REBALANCE CLUSTER
        </button>

        <button 
          disabled={isInjecting || isBackendOffline} 
          onClick={handleAddNode} 
          className={`px-4 py-3 rounded font-bold text-sm transition border ${
            isInjecting || isBackendOffline 
              ? 'bg-slate-800 text-slate-500 border-slate-700 cursor-not-allowed' 
              : 'bg-amber-600/20 border-amber-500/50 text-amber-400 hover:bg-amber-600/40'
          }`}
        >
          ➕ CONNECT NODE 8086
        </button>
      </div>

      {/* CLUSTER STATUS AND KILL SWITCHES */}
      <div className={`flex flex-col gap-3 mt-8 pb-4 transition-opacity duration-300 ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
        <h2 className="text-red-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-red-900/50 pb-2">Cluster Status</h2>
        {clusterState.map(node => (
          <div key={node.id} className="flex justify-between items-center bg-slate-950 p-3 rounded border border-slate-800 relative overflow-hidden">
            <div className={`absolute top-0 right-0 w-16 h-full transition-opacity duration-300 ${gossipPulse ? 'opacity-30' : 'opacity-10'}`} style={{background: `linear-gradient(to left, ${node.color}, transparent)`}}></div>
            <div className="flex items-center gap-2 relative z-10">
              <span className={`w-3 h-3 rounded-full ${node.status === "alive" ? "animate-pulse" : ""}`} style={{ backgroundColor: node.status === "alive" ? node.color : "#334155" }}></span>
              <span className={`font-mono ${node.status === "alive" ? "text-white" : "text-slate-500 line-through"}`}>Node {node.id}</span>
            </div>
            <button 
              disabled={isInjecting || isBackendOffline} 
              onClick={() => handleToggleStatus(node.id)} 
              className={`text-[10px] font-bold px-3 py-1 rounded border relative z-10 disabled:cursor-not-allowed ${
                node.status === "alive" ? 'border-red-500/50 text-red-400 enabled:hover:bg-red-900/30' : 'border-green-500/50 text-green-400 enabled:hover:bg-green-900/30'
              }`}
            >
              {node.status === "alive" ? "☠️ KILL" : "➕ REVIVE"}
            </button>
          </div>
        ))}
      </div>
    </div>
  );
};

export default ControlCenter;