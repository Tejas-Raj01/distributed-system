// frontend/src/services/api.js

const isProd = import.meta.env.PROD;

// 🚀 THE PERMANENT FIX:
// In production on Vercel, requests use Vercel's Native Edge Proxy (/api/backend).
// In local development, requests hit the local C++ server directly (http://localhost:8080).
// Automatically ignore stale ngrok/tunnel env overrides in production.
const envBaseUrl = import.meta.env.VITE_API_BASE_URL;
const BASE_URL = (isProd && (!envBaseUrl || envBaseUrl.includes('ngrok') || envBaseUrl.includes('lhr.life')))
  ? '/api/backend'
  : (envBaseUrl || (typeof window !== 'undefined' ? `http://${window.location.hostname}:8080` : 'http://localhost:8080'));

const getUrl = (path) => `${BASE_URL}${path}`;

export const apiService = {
  
  // 1. PUT Data
  putData: async (key, value, ttl = 0) => {
    const response = await fetch(getUrl('/put'), {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/x-www-form-urlencoded',
        'ngrok-skip-browser-warning': 'true'
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
    const response = await fetch(getUrl(`/get?key=${encodeURIComponent(key)}`), {
      headers: { 'ngrok-skip-browser-warning': 'true' }
    });
    if (!response.ok) {
      throw new Error('Read Quorum Failed or Key missing');
    }
    return response.text();
  },

  // 3. DELETE Data
  deleteData: async (key) => {
    const response = await fetch(getUrl(`/delete?key=${encodeURIComponent(key)}`), {
      method: 'DELETE',
      headers: { 'ngrok-skip-browser-warning': 'true' }
    });
    if (!response.ok) {
      throw new Error('Delete Quorum Failed');
    }
    return response.json();
  },

  // 4. Fetch Live Cluster State
  fetchClusterState: async () => {
    const response = await fetch(getUrl('/admin/status'), {
      headers: { 'ngrok-skip-browser-warning': 'true' }
    });
    if (!response.ok) {
      throw new Error('Cluster state sync failed');
    }
    return response.json();
  },

  // 5. Remote Node Kill Switch
  killNode: async (port) => {
    const response = await fetch(getUrl('/admin/kill'), {
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

  // 6. Spawn Node
  spawnNode: async (port) => {
    const response = await fetch(getUrl('/admin/spawn'), {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/x-www-form-urlencoded',
        'ngrok-skip-browser-warning': 'true' 
      },
      body: `port=${port}`
    });
    if (!response.ok) {
      throw new Error(`Spawn node failed on port ${port}`);
    }
    return response.json();
  },

  // 7. Quorum Config Sync
  updateConfig: async (config) => {
    const response = await fetch(getUrl('/admin/config'), {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/json',
        'ngrok-skip-browser-warning': 'true'
      },
      body: JSON.stringify(config)
    });
    if (!response.ok) {
      throw new Error('Global configuration replication failed');
    }
    return response.json();
  },

  // 8. Rebalance Cluster
  rebalanceCluster: async () => {
    const response = await fetch(getUrl('/admin/rebalance'), {
      headers: { 'ngrok-skip-browser-warning': 'true' }
    });
    if (!response.ok) {
      throw new Error('Cluster rebalance failed');
    }
    return response.json();
  },

  // 9. Clear All Data
  clearAllData: async () => {
    const response = await fetch(getUrl('/admin/clear'), {
      method: 'POST',
      headers: { 'ngrok-skip-browser-warning': 'true' }
    });
    if (!response.ok) {
      throw new Error('Cluster clear failed');
    }
    return response.json();
  }
};