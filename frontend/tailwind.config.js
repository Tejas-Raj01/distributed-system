/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        darkBg: '#0f172a',
        panelBg: '#1e293b',
        neonGreen: '#22c55e',
        neonRed: '#ef4444',
        neonCyan: '#06b6d4',
      },
    },
  },
  plugins: [],
}