// frontend/src/store/useStore.js
import { create } from 'zustand';

const useStore = create((set, get) => ({
  // 1. clusterState: Zinda aur murda nodes
  clusterState: [
    { id: 8080, angle: 0, color: "#06b6d4", status: "alive" },
    { id: 8082, angle: 120, color: "#d946ef", status: "alive" },
    { id: 8084, angle: 240, color: "#ef4444", status: "alive" }
  ],

  // 2. dataRing: Saari injected keys
  dataRing: [],

  // 3. terminalLogs: Strings array with 100 limit
  terminalLogs: [
    "> [INFO] Global State (Zustand) Initialized...",
    "> [ROUTER] Ready for inputs..."
  ],

  // --- ACTIONS (State Mutators) ---

  // Log add karna (Max 100 lines RAM save karne ke liye)
  addLog: (msg) => set((state) => {
    const newLogs = [...state.terminalLogs, `> ${msg}`];
    if (newLogs.length > 100) {
      newLogs.shift(); // Purane logs nikal do agar 100 se zyada ho jayein
    }
    return { terminalLogs: newLogs };
  }),

  // Naya data dot add karna
  addData: (keyName, angle) => set((state) => {
    const filtered = state.dataRing.filter(k => k.key !== keyName);
    return { dataRing: [...filtered, { key: keyName, angle }] };
  }),

  // Data dot delete karna
  removeData: (keyName) => set((state) => ({
    dataRing: state.dataRing.filter(k => k.key !== keyName)
  })),

  // Node ko kill/revive karna
  toggleNodeStatus: (targetId) => set((state) => {
    const updated = state.clusterState.map(n => {
      if (n.id === targetId) {
        const nextStatus = n.status === "alive" ? "dead" : "alive";
        return { ...n, status: nextStatus };
      }
      return n;
    });
    return { clusterState: updated };
  }),

  // Naya node provision karna
  addNode: (newNode) => set((state) => {
    if (state.clusterState.find(n => n.id === newNode.id)) return state;
    return { clusterState: [...state.clusterState, newNode] };
  })
}));

export default useStore;