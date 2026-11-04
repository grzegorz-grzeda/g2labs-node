import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// The admin page. Built into dist/public, which the Hub serves itself.
// `npm run dev` serves it with hot reload and forwards /api to a Hub on :8080.
export default defineConfig({
  root: 'web',
  plugins: [react()],
  build: {
    outDir: '../dist/public',
    emptyOutDir: true,
  },
  server: {
    proxy: { '/api': 'http://localhost:8080' },
  },
});
