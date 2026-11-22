import { defineConfig } from 'vite'
export default defineConfig({
  build: { rolldownOptions: { input: ['index.html', 'admin.html'] } },
  server: { host: '127.0.0.1', proxy: { '/api': 'http://127.0.0.1:8848' } },
})
