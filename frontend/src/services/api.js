// frontend/src/services/api.js

const BASE_URL = 'http://127.0.0.1:8080'; // Router/Gateway Node Address

export const apiService = {
  
  // 1. PUT Data (With Form URL Encoding & Error Capture)
  putData: async (key, value, ttl = 0) => {
    const response = await fetch(`${BASE_URL}/put`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      body: `key=${encodeURIComponent(key)}&value=${encodeURIComponent(value)}&ttl=${ttl}`
    });

    if (!response.ok) {
      const errData = await response.json().catch(() => ({}));
      throw new Error(errData.message || 'Write Quorum Failed');
    }
    return response.json();
  },

  // 2. GET Data (Returns raw text value from storage)
  getData: async (key) => {
    const response = await fetch(`${BASE_URL}/get?key=${encodeURIComponent(key)}`);
    if (!response.ok) {
      throw new Error('Read Quorum Failed or Key missing');
    }
    return response.text();
  },

  // 3. DELETE Data
  deleteData: async (key) => {
    const response = await fetch(`${BASE_URL}/delete?key=${encodeURIComponent(key)}`, {
      method: 'DELETE'
    });
    if (!response.ok) {
      throw new Error('Delete Quorum Failed');
    }
    return response.json();
  },

  // 4. Fetch Live Cluster State (Canvas Ring Sync Helper)
  // Backend JSON Structure: {"nodes": [{port: 8080, status: "alive"}], "dataMap": [{"key": "u1", "owner": 8080}]}
  fetchClusterState: async () => {
    const response = await fetch(`${BASE_URL}/admin/status`);
    if (!response.ok) {
      throw new Error('Cluster state sync failed');
    }
    return response.json();
  },

  // 5. Remote Node Kill Switch (Hits specific node port directly)
  killNode: async (port) => {
    const response = await fetch(`http://127.0.0.1:${port}/admin/kill`, {
      method: 'POST'
    });
    if (!response.ok) {
      throw new Error(`Remote kill failed on port ${port}`);
    }
    return response.json();
  },

  // 6. Quorum Config Sync (Pillar 1 Helper)
  updateConfig: async (config) => {
    const response = await fetch(`${BASE_URL}/admin/config`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(config)
    });
    if (!response.ok) {
      throw new Error('Global configuration replication failed');
    }
    return response.json();
  }
};