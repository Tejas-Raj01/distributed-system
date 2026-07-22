export default async function handler(req, res) {
  // Target API URL from Vercel Environment Variables
  const targetBaseUrl = process.env.VITE_API_BASE_URL;
  
  if (!targetBaseUrl) {
    return res.status(500).json({ error: 'VITE_API_BASE_URL environment variable is missing in Vercel' });
  }

  // The path we want to hit on the backend (e.g., /admin/status)
  // Vercel routes /api/proxy/* to this function if we use a rewrite, 
  // or we can pass it as a query param ?path=/admin/status
  const path = req.query.path || '';
  const queryString = req.url.split('?')[1];
  
  // Construct the target URL
  let targetUrl = `${targetBaseUrl}${path.startsWith('/') ? '' : '/'}${path}`;
  if (queryString) {
    // Remove the 'path=' part from the querystring to forward the rest
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
        'Content-Type': req.headers['content-type'] || 'application/json',
      },
    };

    // Forward the body if it's a POST/PUT/DELETE
    if (req.method !== 'GET' && req.method !== 'HEAD' && req.body) {
      // Vercel parses req.body, so we need to stringify it based on content-type
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
    
    // We don't need to forward all headers, just the content type
    res.setHeader('Content-Type', response.headers.get('content-type') || 'application/json');
    
    // Add CORS headers so the frontend can read it if called from another domain
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, PUT, DELETE, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, ngrok-skip-browser-warning');

    res.send(data);
  } catch (error) {
    console.error('Proxy Error:', error);
    res.status(500).json({ error: 'Proxy Request Failed', details: error.message });
  }
}
