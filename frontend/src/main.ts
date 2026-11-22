import './style.css'
import {cat,icon} from './art'

type Point={x:number;y:number}
type Look={bodyColor:string;idleFace:string}
type Ability={id:string;name:string;desc:string;cd:number;radius:number;duration:number;target:'cell'|'circle'}
type State={
  status:'ready'|'running'|'clear'|'lost';roundId:string;remainingMs:number;timeLimitMs:number
  width:number;height:number;map:string[];npc:Point;goal:Point;start:Point;face:string;beacon?:Point
  objects:(Point & {closed:boolean;remainingMs:number})[];appearance:Look;abilities:Ability[];owned:string[]
  clearsSinceUnlock:number;nextUnlockAt:number|null;selectedAbilityId:string;unlocked:string;cooldownRemainingMs:number
}
const app=document.querySelector<HTMLElement>('#app')!
app.innerHTML=`
<main>
  <header><div class="title"><span class="gate-logo" aria-hidden="true"></span><h1>道をふさげ。</h1></div><span class="stage-tag">STAGE 01 <b>石垣の庭</b></span></header>
  <div class="layout">
    <section class="board-frame" aria-label="ステージ">
      <div class="hud"><strong id="phase">接続中</strong><span>残り <b id="timer">––.–</b> 秒</span></div>
      <div class="meter"><i id="clock-bar"></i></div>
      <canvas id="map" width="1408" height="896" aria-label="石垣の庭。プレイ中にクリックまたはタップで能力を発動">Canvas対応のブラウザが必要です。</canvas>
      <footer><span><i class="key start"></i> スタート</span><span><i class="key goal"></i> ゴール</span><span id="board-help">猫がゴールするまでに進路をふさごう</span></footer>
    </section>
    <aside>
      <div class="panel"><h2>使う能力</h2>
        <div class="ability-heading"><canvas id="active-icon" width="32" height="32"></canvas><div><strong id="ability-name">…</strong><small id="charge"></small></div></div>
        <p id="ability-desc"></p>
        <div id="collection" aria-label="所持能力"></div><p id="progress"></p>
      </div>
      <details class="panel appearance" id="appearance"><summary>猫の見た目</summary>
        <div class="custom"><canvas id="preview" width="32" height="32"></canvas><label>平常時の表情<select id="idle">
          <option value="calm">すまし顔</option><option value="fierce">きりっ</option><option value="wink">ウインク</option>
          <option value="happy">にっこり</option><option value="brow">片眉上げ</option><option value="sleepy">半目</option>
        </select></label></div>
        <label>体色 <span class="colors"><input type="color" id="picker" aria-label="体色を選ぶ"><input id="hex" maxlength="7" aria-label="カラーコード" spellcheck="false"></span></label>
      </details>
      <button id="action" disabled>ゲーム開始</button><p id="notice" role="status"></p>
    </aside>
  </div>
</main>`
function el<T extends HTMLElement>(id:string){return document.getElementById(id) as T}
const canvas=el<HTMLCanvasElement>('map'), ctx=canvas.getContext('2d')!
const idle=el<HTMLSelectElement>('idle'), picker=el<HTMLInputElement>('picker'), hex=el<HTMLInputElement>('hex')
const action=el<HTMLButtonElement>('action')
let s:State|null=null, selected='wall', look:Look={idleFace:'calm',bodyColor:'#efe9ff'}
let choosing=true, sending=false, sequence=0, lastApplied=0, cursor:Point|null=null
let collectionKey='', syncedRound='', notice='', noticeUntil=0
let previousNpc:Point|null=null, received=0
const tile=128
function tell(msg:string){notice=msg;noticeUntil=performance.now()+2600;el('notice').textContent=msg}
function ability(){return s!.abilities.find(a=>a.id===selected)!}
function image(id:string,source:HTMLCanvasElement){
  const c=el<HTMLCanvasElement>(id),g=c.getContext('2d')!
  g.clearRect(0,0,c.width,c.height);g.drawImage(source,0,0)
}
function preview(){image('preview',cat(look.idleFace,look.bodyColor))}
function setLook(v:Look){
  look={...v}; idle.value=look.idleFace;picker.value=look.bodyColor;hex.value=look.bodyColor
  hex.setCustomValidity('');preview()
}
idle.onchange=()=>{look.idleFace=idle.value;preview()}
picker.oninput=()=>setLook({...look,bodyColor:picker.value})
hex.oninput=()=>{
  if(/^#[\da-f]{6}$/i.test(hex.value)) setLook({...look,bodyColor:hex.value})
  else hex.setCustomValidity('#と6桁の16進数を入力してください')
}
function update(){
  if(!s) return
  const playing=s.status==='running', setup=!playing && choosing
  const a=ability()
  el('phase').textContent=playing?'プレイ中':setup?'準備':s.status==='clear'?'クリア！':'ゴール到着'
  el('timer').textContent=((setup?s.timeLimitMs:s.remainingMs)/1000).toFixed(1)
  el('clock-bar').style.width=(setup?100:s.remainingMs/s.timeLimitMs*100)+'%'
  image('active-icon',icon(a.id));el('ability-name').textContent=a.name;el('ability-desc').textContent=a.desc
  el('charge').textContent=playing?(s.cooldownRemainingMs?'あと '+(s.cooldownRemainingMs/1000).toFixed(1)+' 秒':'発動できます'):'再使用 '+a.cd/1000+' 秒'
  const key=[...s.owned,selected,String(setup)].join('/')
  if(key!==collectionKey){
    collectionKey=key
    const list=el('collection');list.replaceChildren()
    for(const id of s.owned){
      const item=s.abilities.find(a=>a.id===id)!
      const btn=document.createElement('button'), img=document.createElement('canvas')
      img.width=img.height=32;img.getContext('2d')!.drawImage(icon(id),0,0)
      btn.append(img);btn.type='button';btn.title=item.name;btn.setAttribute('aria-label',item.name)
      btn.setAttribute('aria-pressed',String(id===selected));btn.disabled=!setup
      btn.onclick=()=>{selected=id;update()};list.append(btn)
    }
  }
  el('progress').textContent='所持 '+s.owned.length+' / '+s.abilities.length+'　'+
    (s.nextUnlockAt===null?'全能力解放済み':'次の解放まであと'+(s.nextUnlockAt-s.clearsSinceUnlock)+'勝')
  el('appearance').hidden=!setup
  action.disabled=sending
  action.textContent=playing?'中断して選び直す':setup?'ゲーム開始':'再挑戦'
  const unlocked=s.abilities.find(a=>a.id===s!.unlocked)
  el('notice').textContent=performance.now()<noticeUntil?notice:setup?'24秒間、猫のゴール到着を阻止しよう。':playing?'盤面をクリックして発動':s.status==='clear'?(unlocked?'クリア！「'+unlocked.name+'」を解放しました。':'クリア！ ゴールを守りました。'):'猫がゴールに到着しました。'
}
async function request(name='state',body?:unknown){
  const serial=++sequence
  try{
    const response=await fetch('/api/'+name,body===undefined?{cache:'no-store'}:{
      method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)
    })
    const data=await response.json()
    if(serial<lastApplied) return
    lastApplied=serial
    if(!response.ok) tell(data.error??'操作を受け付けられませんでした')
    if(!data.abilities) return
    const next=data as State
    const changed=!s || next.roundId!==syncedRound
    previousNpc=s && !changed && next.status==='running' && Math.hypot(s.npc.x-next.npc.x,s.npc.y-next.npc.y)<.5?s.npc:null
    received=performance.now()
    if(changed){selected=next.selectedAbilityId;setLook(next.appearance);syncedRound=next.roundId}
    if(next.status==='ready') choosing=true
    else if(changed || s?.status!==next.status) choosing=false
    s=next;update()
  }catch{tell('サーバーに接続できません。再接続しています。')}
}
async function send(name:string,body:unknown){
  if(sending) return
  sending=true;action.disabled=true
  try{await request(name,body)}finally{sending=false;update()}
}
action.onclick=()=>{
  if(!s)return
  if(s.status==='running') void send('retire',{roundId:s.roundId})
  else if(!choosing){choosing=true;update()}
  else if(hex.reportValidity()) void send('start',{abilityId:selected,appearance:look})
}
function coords(e:PointerEvent|MouseEvent):Point{
  const r=canvas.getBoundingClientRect()
  return {x:(e.clientX-r.left)*canvas.width/r.width/tile,y:(e.clientY-r.top)*canvas.height/r.height/tile}
}
canvas.onpointermove=e=>cursor=coords(e);canvas.onpointerleave=()=>cursor=null
canvas.onclick=e=>{
  if(s?.status==='running') void send('use',{roundId:s.roundId,...coords(e)})
}
function draw(){
  requestAnimationFrame(draw)
  if(!s)return
  ctx.imageSmoothingEnabled=false
  const box=(x:number,y:number,w:number,h:number,color:string)=>{ctx.fillStyle=color;ctx.fillRect(x,y,w,h)}
  for(let y=0;y<s.height;y++)for(let x=0;x<s.width;x++){
    const px=x*tile,py=y*tile
    box(px,py,tile,tile,(x+y)%2?'#9aba91':'#a3c29a')
    box(px+14+(x*17+y*3)%80,py+92,4,8,'#87ac80');box(px+19+(x*17+y*3)%80,py+96,5,4,'#87ac80')
    if(s.map[y][x]==='#'){
      box(px,py,128,128,'#676679');box(px+3,py+3,122,103,'#ab9fa9')
      box(px+3,py+3,122,9,'#d0c4c5');box(px+3,py+104,122,8,'#817986')
      box(px+3,py+50,122,5,'#7e7584');box(px+46,py+4,5,46,'#847c89');box(px+86,py+55,5,48,'#847c89')
      box(px+13,py+13,26,5,'#c7bbbc')
    }
  }
  function pad(p:Point,label:string,color:string){
    const x=p.x*tile,y=p.y*tile;box(x-48,y-48,96,96,color);box(x-40,y-40,80,80,'#364d49')
    ctx.font='bold 40px monospace';ctx.textAlign='center';ctx.fillStyle=color;ctx.fillText(label,x,y+14)
  }
  pad(s.start,selected==='portal'?'↩':'S','#c3efa5');pad(s.goal,'G','#ffcf76')
  const a=ability()
  for(const obj of s.objects){
    const x=obj.x*tile,y=obj.y*tile
    if(a.radius){ctx.beginPath();ctx.arc(x,y,a.radius*tile,0,Math.PI*2);ctx.fillStyle='#7350b24a';ctx.fill()}
    ctx.globalAlpha=a.id==='gate'&&!obj.closed?.52:1
    box(x-48,y-48,96,96,'#49435b');ctx.drawImage(icon(a.id),x-48,y-48,96,96);ctx.globalAlpha=1
    box(x-40,y+40,80*obj.remainingMs/a.duration,6,'#f5e6b7')
  }
  if(s.beacon)ctx.drawImage(icon(a.id),s.beacon.x*tile-40,s.beacon.y*tile-40,80,80)
  const fraction=Math.min(1,(performance.now()-received)/160),setup=choosing&&s.status!=='running'
  const p=setup?s.start:s.npc
  const x=Math.round(tile*(previousNpc&&!setup?previousNpc.x+(p.x-previousNpc.x)*fraction:p.x))
  const y=Math.round(tile*(previousNpc&&!setup?previousNpc.y+(p.y-previousNpc.y)*fraction:p.y))
  box(x-32,y+38,64,6,'#718c70');ctx.drawImage(cat(setup?look.idleFace:s.face,setup?look.bodyColor:s.appearance.bodyColor),x-48,y-48,96,96)
  if(s.status==='running'&&cursor){
    ctx.strokeStyle='#fff8ce';ctx.lineWidth=4
    if(a.target==='cell')ctx.strokeRect(Math.floor(cursor.x)*tile+2,Math.floor(cursor.y)*tile+2,124,124)
    else{ctx.beginPath();ctx.arc(cursor.x*tile,cursor.y*tile,a.radius*tile,0,Math.PI*2);ctx.stroke()}
  }
}
async function poll(){
  if(!sending)await request()
  window.setTimeout(poll,200)
}
void poll();draw()
