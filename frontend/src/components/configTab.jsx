import { useState } from 'react';
import useStore from '../store/useStore';

const ConfigTab = () => {
  const [config, setConfig] = useState({ N: 3, W: 2, R: 2 });
  const { addLog, isBackendOffline } = useStore();

  const handleUpdateConfig = async () => {
    addLog(`[CONFIG] Sending new Quorum rules to C++: N=${config.N}, W=${config.W}, R=${config.R}`);
    try {
      const res = await fetch('http://127.0.0.1:8080/admin/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(config)
      });
      if (res.ok) addLog("[CONFIG] Success: C++ Memory Updated!");
    } catch (err) {
      addLog("[CRITICAL] Failed to sync config with C++.");
    }
  };

  return (
    <div className="bg-slate-900 p-4 rounded border border-slate-700 mt-6">
      <h3 className="text-amber-400 font-bold text-xs uppercase tracking-widest mb-4">Cluster Configuration</h3>
      <div className="flex gap-2">
        {['N', 'W', 'R'].map(param => (
          <div key={param} className="flex flex-col flex-1">
            <label className="text-[9px] text-slate-500 font-bold">{param}</label>
            <input 
              type="number" className="bg-slate-950 border border-slate-700 p-1 text-center text-white rounded"
              value={config[param]}
              onChange={(e) => setConfig({...config, [param]: parseInt(e.target.value)})}
            />
          </div>
        ))}
      </div>
      <button onClick={handleUpdateConfig} className="w-full mt-4 bg-amber-600/20 text-amber-400 p-2 rounded text-xs font-bold border border-amber-500/50 hover:bg-amber-600/40">
        APPLY CONFIG
      </button>
    </div>
  );
};
export default ConfigTab;