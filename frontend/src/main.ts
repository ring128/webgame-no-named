import './style.css'
type Point = { x: number; y: number }
type Block = Point & { remainingMs: number }
type State = {
  status: 'ready' | 'running' | 'clear' | 'lost'
  width: number; height: number; map: string[]
  start: Point; goal: Point; npc: Point; path: Point[]; blocks: Block[]
  remainingMs: number; cooldownRemainingMs: number; blockDurationMs: number
  error?: string
}
document.querySelector<HTMLDivElement>('#app')!.innerHTML = `
<main class="shell">
  <header><span class="eyebrow">SINGLE STAGE / ONE NPC</span><h1>道をふさげ。</h1>
    <p>ゴールへ向かう NPC を、一時的な障害物で足止めしよう。</p></header>
  <section class="game"><div class="board">
    <canvas id="map" width="660" height="420" aria-label="ゲームマップ"></canvas>
    <div class="legend"><span>🟡 NPC</span><span>🟢 スタート</span><span>🔴 ゴール</span><span>🟦 障害物</span></div>
  </div><aside class="panel">
    <div class="stat"><span>残り時間</span><strong id="time">24.0</strong><small>秒</small></div>
    <div class="stat"><span>能力の準備</span><strong id="cooldown">準備完了</strong></div>
    <p id="message" class="message" aria-live="polite">開始を押してゲームを始めてください。</p>
    <button id="start" type="button">ゲーム開始</button>
    <p class="help">通れるマスをクリックして障害物を設置。青いブロックは一定時間で消えます。同じマスをクリックすると延長できます。</p>
  </aside></section>
</main>`
const canvas = document.querySelector<HTMLCanvasElement>('#map')!
const ctx = canvas.getContext('2d')!
const time = document.querySelector<HTMLElement>('#time')!
const cooldown = document.querySelector<HTMLElement>('#cooldown')!
const message = document.querySelector<HTMLElement>('#message')!
const startButton = document.querySelector<HTMLButtonElement>('#start')!
let state: State | null = null
let busy = false
let notice = ''

async function request(path: string, body?: Point): Promise<void> {
  if (busy) return
  busy = true
  try {
    const response = await fetch('/api/' + path, {
      method: body || path === 'start' ? 'POST' : 'GET',
      headers: body ? { 'Content-Type': 'application/json' } : undefined,
      body: body ? JSON.stringify(body) : undefined,
      cache: 'no-store',
    })
    const result = await response.json() as State
    state = result
    notice = response.ok ? '' : (result.error || '操作に失敗しました')
    render()
  } catch {
    notice = 'サーバーに接続できません。C++ サーバーを起動してください。'
    render()
  } finally { busy = false }
}
function render(): void {
  if (!state) { message.textContent = notice || '接続中…'; return }
  const s = state
  const cell = canvas.width / s.width
  ctx.fillStyle = '#101c2b'
  ctx.fillRect(0, 0, canvas.width, canvas.height)
  for (let y = 0; y < s.height; y++) for (let x = 0; x < s.width; x++) {
    const px = x * cell, py = y * cell
    ctx.fillStyle = s.map[y][x] === '#' ? '#263647' : (x + y) % 2 ? '#1b2b3a' : '#203143'
    ctx.fillRect(px + 2, py + 2, cell - 4, cell - 4)
  }
  for (const p of s.path) {
    ctx.fillStyle = 'rgba(255, 211, 107, .17)'
    ctx.fillRect(p.x * cell + 9, p.y * cell + 9, cell - 18, cell - 18)
  }
  for (const b of s.blocks) {
    ctx.fillStyle = '#5cd1ec'
    ctx.fillRect(b.x * cell + 7, b.y * cell + 7, cell - 14, cell - 14)
    ctx.fillStyle = '#0b3144'
    ctx.fillRect(b.x * cell + 12, b.y * cell + 12, cell - 24, cell - 24)
    ctx.fillStyle = '#5cd1ec'
    ctx.fillRect(b.x * cell + 12, b.y * cell + cell - 17,
      (cell - 24) * b.remainingMs / s.blockDurationMs, 5)
  }
  function disc(p: Point, color: string, label: string): void {
    const cx = (p.x + .5) * cell, cy = (p.y + .5) * cell
    ctx.beginPath(); ctx.arc(cx, cy, 20, 0, Math.PI * 2)
    ctx.fillStyle = color; ctx.fill()
    ctx.fillStyle = '#10202f'; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'
    ctx.font = 'bold 15px system-ui'; ctx.fillText(label, cx, cy)
  }
  disc(s.start, '#74d2a3', 'S'); disc(s.goal, '#ff8e7b', 'G'); disc(s.npc, '#ffce69', '●')
  time.textContent = (s.remainingMs / 1000).toFixed(1)
  cooldown.textContent = s.cooldownRemainingMs > 0 ? (s.cooldownRemainingMs / 1000).toFixed(1) + ' 秒' : '準備完了'
  const messages = {
    ready: '開始を押してゲームを始めてください。',
    running: 'マスをクリックして NPC の進路をふさいでください。',
    clear: 'クリア！制限時間までゴールを守りました。',
    lost: '敗北。NPC がゴールに到着しました。',
  }
  message.textContent = notice || messages[s.status]
  startButton.textContent = s.status === 'ready' ? 'ゲーム開始' : '再挑戦'
}
canvas.addEventListener('click', event => {
  if (!state || state.status !== 'running') return
  const rect = canvas.getBoundingClientRect()
  const x = Math.floor((event.clientX - rect.left) / rect.width * state.width)
  const y = Math.floor((event.clientY - rect.top) / rect.height * state.height)
  if (x >= 0 && y >= 0 && x < state.width && y < state.height) void request('block', { x, y })
})
startButton.addEventListener('click', () => void request('start'))
void request('state')
window.setInterval(() => { if (!busy) void request('state') }, 200)
