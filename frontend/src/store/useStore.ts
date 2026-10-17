import { create } from 'zustand';

const useStore = create((set, get) => ({
  // 1. clusterState: Active and inactive nodes
  clusterState: [
    { id: 8080, angle: 0, color: "#06b6d4", status: "alive" },
    { id: 8082, angle: 120, color: "#d946ef", status: "alive" },
    { id: 8084, angle: 240, color: "#ef4444", status: "alive" }
  ],

  // 2. dataRing: Injected key mapping
  dataRing: [],

  // 3. terminalLogs: Log message history (max 100 entries)
  terminalLogs: [
    "> [INFO] Global State (Zustand) Initialized...",
    "> [ROUTER] Ready for inputs..."
  ],

  // Edge cases & error handling states
  isInjecting: false,         // Locks UI during stress testing
  isBackendOffline: false,    // Triggers offline indicator when backend is unreachable

  // --- ACTIONS (State Mutators) ---

  // Adds a log entry (keeps up to 100 entries)
  addLog: (msg) => set((state) => {
    const newLogs = [...state.terminalLogs, `> ${msg}`];
    if (newLogs.length > 100) {
      newLogs.shift(); // Evict oldest log when capacity is exceeded
    }
    return { terminalLogs: newLogs };
  }),

  // Adds or updates a data point on the hash ring
  addData: (keyName, angle) => set((state) => {
    const filtered = state.dataRing.filter(k => k.key !== keyName);
    return { dataRing: [...filtered, { key: keyName, angle }] };
  }),

  // Removes a data point from the hash ring
  removeData: (keyName) => set((state) => ({
    dataRing: state.dataRing.filter(k => k.key !== keyName)
  })),

  // Toggles node alive / dead status
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

  // Adds a newly provisioned node to cluster state
  addNode: (newNode) => set((state) => {
    if (state.clusterState.find(n => n.id === newNode.id)) return state;
    return { clusterState: [...state.clusterState, newNode] };
  }),

  // Error handling setters
  setIsInjecting: (bool) => set({ isInjecting: bool }),
  setIsBackendOffline: (bool) => set({ isBackendOffline: bool }),

  syncClusterState: (backendNodes, backendKeys) => set({
    clusterState: backendNodes,
    dataRing: backendKeys
  }),

}));

export default useStore;