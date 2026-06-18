import { useRef, useEffect } from 'react';
import useStore from '../store/useStore';

const Terminal = () => {
  const terminalLogs = useStore(state => state.terminalLogs);
  const logsEndRef = useRef(null);

  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [terminalLogs]);

  return (
    <div className="w-1/4 bg-black p-0 flex flex-col z-10 border-l border-slate-800">
      <div className="bg-slate-900 p-4 border-b border-slate-800">
        <h2 className="text-green-500 text-sm font-bold tracking-widest flex items-center gap-2">
          <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
          SYSTEM TERMINAL
        </h2>
      </div>
      <div className="p-4 overflow-y-auto flex-1 text-[13px] space-y-3 font-mono">
        {terminalLogs.map((log, index) => (
          <p key={index} className={
            log.includes("ERROR") || log.includes("CRITICAL") ? "text-red-400" : 
            log.includes("SUCCESS") ? "text-cyan-400 font-bold" : 
            log.includes("CHAOS") ? "text-purple-400 font-bold" : 
            log.includes("ADMIN") ? "text-emerald-400 font-bold" : 
            log.includes("GOSSIP") ? "text-yellow-400 font-bold" : 
            log.includes("INFO") ? "text-blue-400 font-bold" : "text-green-500 opacity-90"
          }>{log}</p>
        ))}
        <div ref={logsEndRef} />
      </div>
    </div>
  );
};

export default Terminal;