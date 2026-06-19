// frontend/src/services/api.js

// 🚀 THE FIX: NGROK URL
const BASE_URL = import.meta.env.VITE_API_BASE_URL;

export const apiService = {
  
  // 1. PUT Data
  putData: async (key, value, ttl = 0) => {
    const response = await fetch(`${BASE_URL}/put`, {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/x-www-form-urlencoded',
        'ngrok-skip-browser-warning': 'true' // 🚀 THE MAGIC HEADER
      },
      body: `key=${encodeURIComponent(key)}&value=${encodeURIComponent(value)}&ttl=${ttl}`
    });

    if (!response.ok) {
      const errData = await response.json().catch(() => ({}));
      throw new Error(errData.message || 'Write Quorum Failed');
    }
    return response.json();
  },

  // 2. GET Data
  getData: async (key) => {
    const response = await fetch(`${BASE_URL}/get?key=${encodeURIComponent(key)}`, {
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
    });
    if (!response.ok) {
      throw new Error('Read Quorum Failed or Key missing');
    }
    return response.text();
  },

  // 3. DELETE Data
  deleteData: async (key) => {
    const response = await fetch(`${BASE_URL}/delete?key=${encodeURIComponent(key)}`, {
      method: 'DELETE',
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
    });
    if (!response.ok) {
      throw new Error('Delete Quorum Failed');
    }
    return response.json();
  },

  // 4. Fetch Live Cluster State
  fetchClusterState: async () => {
    const response = await fetch(`${BASE_URL}/admin/status`, {
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
    });
    if (!response.ok) {
      throw new Error('Cluster state sync failed');
    }
    return response.json();
  },

  // 5. Remote Node Kill Switch
  killNode: async (port) => {
    const response = await fetch(`${BASE_URL}/admin/kill`, {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/x-www-form-urlencoded',
        'ngrok-skip-browser-warning': 'true' 
      },
      body: `port=${port}`
    });
    if (!response.ok) {
      throw new Error(`Remote kill failed on port ${port}`);
    }
    return response.json();
  },

  // 6. Quorum Config Sync
  updateConfig: async (config) => {
    const response = await fetch(`${BASE_URL}/admin/config`, {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/json',
        'ngrok-skip-browser-warning': 'true' // 🚀 MAGIC HEADER
      },
      body: JSON.stringify(config)
    });
    if (!response.ok) {
      throw new Error('Global configuration replication failed');
    }
    return response.json();
  }
};