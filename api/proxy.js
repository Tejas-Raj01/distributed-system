export default async function handler(req, res) {
  // Always handle OPTIONS preflight
  if (req.method === 'OPTIONS') {
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, ngrok-skip-browser-warning, Authorization');
    return res.status(204).end();
  }

  const targetBaseUrl = process.env.VITE_API_BASE_URL || 'https://paycheck-polar-wildfire.ngrok-free.dev';
  
  const path = req.query.path || '';
  const queryString = req.url.includes('?') ? req.url.split('?')[1] : '';
  
  let targetUrl = `${targetBaseUrl}${path.startsWith('/') ? '' : '/'}${path}`;
  if (queryString) {
    const params = new URLSearchParams(queryString);
    params.delete('path');
    const remainingParams = params.toString();
    if (remainingParams) {
      targetUrl += `?${remainingParams}`;
    }
  }

  try {
    const fetchOptions = {
      method: req.method,
      headers: {
        'ngrok-skip-browser-warning': 'true',
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36',
        'Content-Type': req.headers['content-type'] || 'application/json',
      },
    };

    if (req.method !== 'GET' && req.method !== 'HEAD' && req.body) {
      if (typeof req.body === 'object') {
        if (req.headers['content-type']?.includes('application/x-www-form-urlencoded')) {
          fetchOptions.body = new URLSearchParams(req.body).toString();
        } else {
          fetchOptions.body = JSON.stringify(req.body);
        }
      } else {
        fetchOptions.body = req.body;
      }
    }

    const response = await fetch(targetUrl, fetchOptions);
    const data = await response.text();

    res.status(response.status);
    res.setHeader('Content-Type', response.headers.get('content-type') || 'application/json');
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, ngrok-skip-browser-warning');

    res.send(data);
  } catch (error) {
    console.error('Proxy Error:', error);
    res.status(500).json({ error: 'Proxy Request Failed', details: error.message });
  }
}
