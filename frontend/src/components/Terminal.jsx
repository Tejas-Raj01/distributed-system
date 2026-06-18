import { useRef, useEffect } from 'react';
import useStore from '../store/useStore';

const Terminal = () => {
  const terminalLogs = useStore(state => state.terminalLogs);
  const logsEndRef = useRef(null);

  // ==========================================
  // 1. AUTO-SCROLL LOGIC
  // ==========================================
  // Jaise hi terminalLogs array update hoga, yeh automatically neeche scroll kar dega
  useEffect(() => {
    logsEndRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [terminalLogs]);

  // ==========================================
  // 2. LOG PARSING & COLOR FORMATTING
  // ==========================================
  const getLogColor = (log) => {
    if (log.includes("[ERROR]") || log.includes("[CRITICAL]")) return "text-red-400 font-bold";
    if (log.includes("[INFO]")) return "text-blue-400 font-bold"; // Info is Blue
    if (log.includes("[ROUTER]")) return "text-green-400 font-bold"; // Router is Green
    if (log.includes("[SUCCESS]")) return "text-cyan-400 font-bold";
    if (log.includes("[CHAOS]")) return "text-purple-400 font-bold";
    if (log.includes("[ADMIN]")) return "text-emerald-400 font-bold";
    if (log.includes("[GOSSIP]")) return "text-yellow-400 font-bold";
    
    return "text-slate-300 opacity-90"; // Default Text
  };

  return (
    <div className="w-1/4 bg-black p-0 flex flex-col z-10 border-l border-slate-800">
      <div className="bg-slate-900 p-4 border-b border-slate-800">
        <h2 className="text-green-500 text-sm font-bold tracking-widest flex items-center gap-2">
          <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
          SYSTEM TERMINAL
        </h2>
      </div>
      
      {/* Terminal Output Area */}
      <div className="p-4 overflow-y-auto flex-1 text-[13px] space-y-3 font-mono hide-scrollbar">
        {terminalLogs.map((log, index) => (
          <p key={index} className={getLogColor(log)}>
            {log}
          </p>
        ))}
        
        {/* Invisible div for Auto-Scroll Anchor */}
        <div ref={logsEndRef} />
      </div>
    </div>
  );
};

export default Terminal;