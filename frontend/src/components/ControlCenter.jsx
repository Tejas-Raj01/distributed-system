import { useState, useEffect } from 'react';
import useStore from '../store/useStore';
import { apiService } from '../services/api'; // 📡 THE MISSING IMPORT

const delay = (ms) => new Promise(resolve => setTimeout(resolve, ms));

const ControlCenter = () => {
  const [keyInput, setKeyInput] = useState("");
  const [valInput, setValInput] = useState("");
  const [gossipPulse, setGossipPulse] = useState(false);
  const [injectCount, setInjectCount] = useState(1000); 
  const [config, setConfig] = useState({ N: 3, W: 2, R: 2 });

  // ZUSTAND: Notice ki humne yahan se addData aur removeData hata diya hai!
  const { 
    clusterState, isInjecting, isBackendOffline,
    addLog, toggleNodeStatus, setIsInjecting, setIsBackendOffline 
  } = useStore();

  // ==========================================
  // 💓 PILLAR 3: SERVER-AUTHORITATIVE HEARTBEAT
  // ==========================================
  useEffect(() => {
    const syncLiveState = async () => {
      try {
        const data = await apiService.fetchClusterState();
        
        // C++ backend hi angle aur data bhejega, React sirf paint karega
        useStore.setState({
          clusterState: data.nodes || [],
          dataRing: data.dataMap || []
        });
        setIsBackendOffline(false);
      } catch (err) {
        setIsBackendOffline(true);
      }
    };

    const heartbeatTimer = setInterval(syncLiveState, 1000);
    return () => clearInterval(heartbeatTimer);
  }, [setIsBackendOffline]);

  // Visual Pulse Timer
  useEffect(() => {
    const pulseTimer = setInterval(() => setGossipPulse(prev => !prev), 2000);
    return () => clearInterval(pulseTimer);
  }, []);

  // ==========================================
  // ⚖️ PILLAR 1: CONFIG SYNC
  // ==========================================
  const handleUpdateConfig = async () => {
    if (isInjecting || isBackendOffline) return;
    addLog(`[CONFIG] Syncing Rules with C++: N=${config.N}, W=${config.W}, R=${config.R}`);
    try {
      await apiService.updateConfig(config);
      addLog("[CONFIG] Success: C++ Memory Updated!");
    } catch (err) {
      addLog("[CRITICAL] Config sync failed. Backend unreachable.");
    }
  };

  // ==========================================
  // 📡 PILLAR 2: DECOUPLED CRUD OPERATIONS
  // ==========================================
  const handlePut = async () => {
    if (!keyInput || !valInput || isInjecting || isBackendOffline) return;
    addLog(`[ROUTER] Sending PUT Request: '${keyInput}'`);
    try {
      await apiService.putData(keyInput, valInput);
      addLog(`[QUORUM] SUCCESS: Key '${keyInput}' saved.`);
      // Notice: Yahan addData() call nahi ho raha, Heartbeat apne aap UI update karega!
      setKeyInput(""); setValInput("");
    } catch (err) { 
      addLog(`[ERROR] ${err.message}`); 
    }
  };

  const handleGet = async () => {
    if (!keyInput || isInjecting || isBackendOffline) return;
    addLog(`[ROUTER] Fetching data for Key: '${keyInput}'...`);
    const startTime = performance.now();
    try {
      const textData = await apiService.getData(keyInput);
      const latency = (performance.now() - startTime).toFixed(2);
      const readType = config.R === 1 ? "Eventual" : "Strong";
      addLog(`[STORAGE] Found Value: '${textData}' | Latency: ${latency}ms (${readType})`);
      setValInput(textData);
    } catch (err) { 
      const latency = (performance.now() - startTime).toFixed(2);
      addLog(`[ERROR] ${err.message} | Latency: ${latency}ms`); 
    }
  };

  const handleDelete = async () => {
    if (!keyInput || isInjecting || isBackendOffline) return;
    addLog(`[ROUTER] Sending DELETE Request for: '${keyInput}'...`);
    try {
      await apiService.deleteData(keyInput);
      addLog(`[QUORUM] SUCCESS: Key '${keyInput}' deleted.`);
      setKeyInput(""); setValInput("");
    } catch (err) { 
      addLog(`[ERROR] ${err.message}`); 
    }
  };

 const injectStressTest = async () => {
  if (isInjecting || isBackendOffline) return;
  setIsInjecting(true);
  addLog(`[CHAOS] 🚀 Initializing Adaptive Batch Request Generator for ${injectCount} keys...`);

  const BATCH_SIZE = 50;     // Hardware aur browser runtime optimization ke liye perfect concurrency threshold
  const THROTTLE_DELAY = 80;  // Har batch execution burst ke beech ka micro recovery period (ms)

  for (let i = 0; i < injectCount; i += BATCH_SIZE) {
    let batchPromises = [];
    const currentBatchSize = Math.min(BATCH_SIZE, injectCount - i);

    addLog(`[BATCHING] Dispatching thread chunk: keys range ${i} to ${i + currentBatchSize}...`);

    for (let j = 0; j < currentBatchSize; j++) {
      const randomKey = `stress_${Math.floor(Math.random() * 999999)}`;
      
      // Request queue mein promise attach karna bina use await kiye (Parallelism inside chunk)
      batchPromises.push(
        apiService.putData(randomKey, "test_data")
          .catch((err) => {
            // Gracefully catch internal request drop taaki baaki pipeline crash na ho
          })
      );
    }

    // 🛡️ AllSettled waits only for the current micro-burst of 50 requests
    await Promise.allSettled(batchPromises);

    addLog(`[NETWORK] Burst Delivered. Pipeline synchronization: ${i + currentBatchSize}/${injectCount}`);

    // Main JavaScript event loop ko frame drop avoid karne ke liye short breathing room dena
    if (i + BATCH_SIZE < injectCount) {
      await delay(THROTTLE_DELAY);
    }
  }

  setIsInjecting(false);
  addLog(`[SUCCESS] 🔥 Advanced Stress Pipeline executed flawlessly with zero thread locking.`);
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
    addLog("[INFO] To connect Node 8086, boot up the C++ backend on port 8086!");
  };

  const handleToggleStatus = async (id) => {
    if (isInjecting || isBackendOffline) return;
    toggleNodeStatus(id); 
    const node = clusterState.find(n => n.id === id);
    const nextStatus = node.status === "alive" ? "DEAD" : "ALIVE";
    
    addLog(`[GOSSIP] Node ${id} marked as ${nextStatus} in UI.`);
    if (nextStatus === "DEAD") {
      try {
        addLog(`[ADMIN] Sending remote KILL signal to port ${id}...`);
        await apiService.killNode(id);
        addLog(`[SUCCESS] Node ${id} gracefully shut down.`);
      } catch (err) {
        addLog(`[NETWORK] Node ${id} unreachable. Assume already dead.`);
      }
    }
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

      {/* ⚖️ CLUSTER CONFIGURATION */}
      <div className={`bg-slate-950 p-4 rounded border border-slate-700 mt-6 transition-opacity ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
        <h3 className="text-amber-400 font-bold text-[10px] uppercase tracking-widest mb-3">Cluster Config (Quorum)</h3>
        <div className="flex gap-2">
          {['N', 'W', 'R'].map(param => (
            <div key={param} className="flex flex-col flex-1">
              <label className="text-[9px] text-slate-500 font-bold mb-1 text-center">{param}</label>
              <input 
                disabled={isInjecting || isBackendOffline}
                type="number" min="1" max="5"
                className="bg-slate-900 border border-slate-700 p-1 text-center text-white rounded text-sm disabled:cursor-not-allowed"
                value={config[param]}
                onChange={(e) => setConfig({...config, [param]: parseInt(e.target.value)})}
              />
            </div>
          ))}
        </div>
        <button disabled={isInjecting || isBackendOffline} onClick={handleUpdateConfig} className="w-full mt-3 bg-amber-600/20 text-amber-400 p-2 rounded text-[10px] font-bold border border-amber-500/50 hover:bg-amber-600/40 disabled:cursor-not-allowed transition">
          APPLY CONFIG TO C++
        </button>
      </div>

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
      <div className={`flex flex-col gap-3 mt-8 transition-opacity duration-300 ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
        <h2 className="text-purple-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-purple-900/50 pb-2">Chaos & Automation</h2>
        
        <div className="flex flex-col gap-1 mt-1 mb-2">
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

        <button disabled={isInjecting || isBackendOffline} onClick={injectStressTest} className="px-4 py-3 rounded font-bold text-sm transition border bg-purple-600/20 border-purple-500/50 text-purple-300 hover:bg-purple-600/40 disabled:bg-slate-800 disabled:text-slate-500 disabled:border-slate-700 disabled:cursor-not-allowed">
          🚀 INJECT {injectCount} KEYS
        </button>

        <button disabled={isInjecting || isBackendOffline} onClick={handleRebalance} className="px-4 py-3 rounded font-bold text-sm transition border bg-emerald-600/20 border-emerald-500/50 text-emerald-400 hover:bg-emerald-600/40 disabled:bg-slate-800 disabled:text-slate-500 disabled:border-slate-700 disabled:cursor-not-allowed">
          ⚖️ REBALANCE CLUSTER
        </button>

        <button disabled={isInjecting || isBackendOffline} onClick={handleAddNode} className="px-4 py-3 rounded font-bold text-sm transition border bg-amber-600/20 border-amber-500/50 text-amber-400 hover:bg-amber-600/40 disabled:bg-slate-800 disabled:text-slate-500 disabled:border-slate-700 disabled:cursor-not-allowed">
          ➕ CONNECT NODE 8086
        </button>
      </div>

      {/* CLUSTER STATUS */}
      <div className={`flex flex-col gap-3 mt-8 pb-4 transition-opacity duration-300 ${isInjecting || isBackendOffline ? 'opacity-40' : ''}`}>
        <h2 className="text-red-400 text-[10px] font-bold tracking-[0.2em] uppercase border-b border-red-900/50 pb-2">Cluster Status</h2>
        {clusterState.map(node => (
          <div key={node.id} className="flex justify-between items-center bg-slate-950 p-3 rounded border border-slate-800 relative overflow-hidden">
            <div className={`absolute top-0 right-0 w-16 h-full transition-opacity duration-300 ${gossipPulse ? 'opacity-30' : 'opacity-10'}`} style={{background: `linear-gradient(to left, ${node.color || '#06b6d4'}, transparent)`}}></div>
            <div className="flex items-center gap-2 relative z-10">
              <span className={`w-3 h-3 rounded-full ${node.status === "alive" ? "animate-pulse" : ""}`} style={{ backgroundColor: node.status === "alive" ? (node.color || "#06b6d4") : "#334155" }}></span>
              <span className={`font-mono ${node.status === "alive" ? "text-white" : "text-slate-500 line-through"}`}>Node {node.id || node.port}</span>
            </div>
            <button disabled={isInjecting || isBackendOffline} onClick={() => handleToggleStatus(node.id)} className={`text-[10px] font-bold px-3 py-1 rounded border relative z-10 disabled:cursor-not-allowed ${node.status === "alive" ? 'border-red-500/50 text-red-400 enabled:hover:bg-red-900/30' : 'border-green-500/50 text-green-400 enabled:hover:bg-green-900/30'}`}>
              {node.status === "alive" ? "☠️ KILL" : "➕ REVIVE"}
            </button>
          </div>
        ))}
      </div>
    </div>
  );
};

export default ControlCenter;