import { defineConfig } from 'vite'
export default defineConfig(({mode})=>({
  base: mode==='pages'?'./':'/',
  build: {
    outDir: mode==='pages'?'dist-pages':'dist',
    rolldownOptions: {input: mode==='pages'?['index.html']:['index.html','admin.html']}
  },
  server: { host: '127.0.0.1', proxy: { '/api': 'http://127.0.0.1:8848' } },
}))
