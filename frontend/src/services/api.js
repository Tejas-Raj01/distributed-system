// frontend/src/services/api.js

// 🚀 THE FIX: Use Vercel Serverless Function as a Proxy in Production to bypass ngrok CORS
const isProd = import.meta.env.PROD;
const hasVercelApi = !!import.meta.env.VITE_API_BASE_URL;

// If we are in production on Vercel, we use the local /api/proxy function we created.
// Otherwise, we hit the backend directly (localhost).
const BASE_URL = (isProd && hasVercelApi) 
  ? '/api/proxy?path=' 
  : (typeof window !== 'undefined' ? `http://${window.location.hostname}:8080` : 'http://localhost:8080');

// Helper to construct the final URL depending on whether we use proxy
const getUrl = (path) => {
  if (BASE_URL.startsWith('/api/proxy')) {
    return `${BASE_URL}${encodeURIComponent(path)}`;
  }
  return `${BASE_URL}${path}`;
};

export const apiService = {
  
  // 1. PUT Data
  putData: async (key, value, ttl = 0) => {
    const response = await fetch(getUrl('/put'), {
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
    const response = await fetch(getUrl(`/get?key=${encodeURIComponent(key)}`), {
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
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
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
    });
    if (!response.ok) {
      throw new Error('Delete Quorum Failed');
    }
    return response.json();
  },

  // 4. Fetch Live Cluster State
  fetchClusterState: async () => {
    const response = await fetch(getUrl('/admin/status'), {
      headers: { 'ngrok-skip-browser-warning': 'true' } // 🚀 MAGIC HEADER
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

  // 6. Quorum Config Sync
  updateConfig: async (config) => {
    const response = await fetch(getUrl('/admin/config'), {
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