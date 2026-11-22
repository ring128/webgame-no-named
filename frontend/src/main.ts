import './style.css'
import {cat,icon} from './art'

type Point={x:number;y:number}
type Ability={id:string;name:string;desc:string;cd:number;radius:number;duration:number;target:'cell'|'circle'}
type State={
  status:'ready'|'running'|'clear'|'lost';roundId:string;remainingMs:number;timeLimitMs:number
  width:number;height:number;map:string[];npc:Point;goal:Point;start:Point;face:string;beacon?:Point
  objects:(Point & {closed:boolean;remainingMs:number})[];appearance:{bodyColor:string};abilities:Ability[];owned:string[]
  clearsSinceUnlock:number;nextUnlockAt:number|null;selectedAbilityId:string;unlocked:string;cooldownRemainingMs:number
  skin:string;scoreMs:number;signedIn:boolean;loginConfigured:boolean
}
type Page='home'|'stage'|'equipment'|'skins'|'ranking'|'feedback'|'play'|'result'
const coats:Record<string,{name:string;color:string}>={
  snow:{name:'しろねこ',color:'#F8F1DD'},tabby:{name:'茶トラ',color:'#DAA46F'},
  tux:{name:'ハチワレ',color:'#534E52'},calico:{name:'三毛',color:'#F8EACF'}
}
document.querySelector('#app')!.innerHTML=`
<main class="notebook">
<div class="rings" aria-hidden="true"></div>
<div id="sheet">
  <nav id="nav"><button id="back">← 戻る</button><span>作戦ノート</span></nav>
  <section id="home" hidden><div class="cover">
    <div class="cover-art"><canvas id="cover-cat" width="32" height="32" aria-hidden="true"></canvas><i></i></div>
    <h1>道をふさげ<span>。</span></h1><p>猫のゴールを阻止せよ。</p></div>
    <div class="menu"><button data-page="stage">スタート <b>↗</b></button><button data-page="ranking">ランキング</button><button data-page="skins">スキン</button><button data-page="feedback">意見</button></div></section>
  <section id="stage" hidden><h2>ステージを選ぶ</h2><button id="garden" class="stage-card"><canvas width="220" height="140" id="mini"></canvas><span><small>STAGE 01</small><strong>石垣の庭</strong><em id="stage-limit"></em></span></button></section>
  <section id="equipment" hidden><h2>能力を選ぶ</h2><p id="progress"></p><div id="abilities" class="cards"></div><div id="brief"></div><button class="primary" id="begin">作戦開始 →</button></section>
  <section id="skins" hidden><h2>スキン</h2><div class="cards skins" id="coats"></div></section>
  <section id="ranking" hidden><h2>ランキング</h2><p>妨害を解いた後、ゴールまでにかかる時間。自己ベストを長い順に掲載します。</p><div id="ranks"></div></section>
  <section id="feedback" hidden><h2>意見</h2><form id="form"><label for="comment">石垣の庭を遊んだ感想・改善案</label><textarea id="comment" rows="5" required maxlength="1000"></textarea><button class="primary">意見を保存</button><small>このゲームサーバーの意見帳に保存します。</small></form></section>
  <section id="result" hidden><div id="stamp"></div><canvas id="result-cat" width="64" height="64" aria-hidden="true"></canvas><h2 id="outcome"></h2><p id="score"></p><p id="unlock"></p><div class="actions"><button class="primary" data-page="equipment">再挑戦</button><button data-page="ranking">ランキング</button></div></section>
</div>
<section id="play" hidden><div class="hud"><button id="retire" aria-label="中断してホームに戻る">← 中断</button><span id="clock">残り <b id="timer"></b> 秒</span></div>
<canvas id="map" width="1408" height="896" aria-label="石垣の庭。クリックで能力を発動"></canvas>
<div class="sticky"><canvas id="active-icon" width="32" height="32"></canvas><b id="ability-name"></b><span id="charge"></span></div></section>
<div id="notice" role="status"></div></main>`
function el<T extends HTMLElement=HTMLElement>(id:string){return document.getElementById(id) as T}
const canvas=el<HTMLCanvasElement>('map'),ctx=canvas.getContext('2d')!,tile=128
let s:State|null=null,page:Page='home',selected='wall',sending=false,sequence=0,lastApplied=0
let cursor:Point|null=null,previousNpc:Point|null=null,received=0,mark:(Point&{at:number})|null=null
let wake:ReturnType<typeof setTimeout>,rankingVersion=0
function tell(message:string){clearTimeout(wake);el('notice').textContent=message;wake=setTimeout(()=>el('notice').textContent='',4000)}
function paint(canvas:HTMLCanvasElement,source:HTMLCanvasElement){
  const g=canvas.getContext('2d')!;g.clearRect(0,0,canvas.width,canvas.height);g.imageSmoothingEnabled=false;g.drawImage(source,0,0,canvas.width,canvas.height)
}
function picture(source:HTMLCanvasElement){
  const img=document.createElement('canvas');img.width=img.height=32;paint(img,source);return img
}
function card(label:string,img:HTMLCanvasElement,active:boolean,click:()=>void){
  const button=document.createElement('button');button.className='card';button.setAttribute('aria-pressed',String(active))
  button.append(picture(img),document.createTextNode(label));button.onclick=click;return button
}
function show(next:Page){
  page=next;rankingVersion++;el('notice').textContent=''
  for(const id of ['home','stage','equipment','skins','ranking','feedback','play','result'])el(id).hidden=id!==next
  el('sheet').hidden=next==='play';document.body.classList.toggle('in-game',next==='play')
  el('nav').hidden=next==='home';cursor=null
  if(next!=='play' && document.fullscreenElement)void document.exitFullscreen().catch(()=>{})
  if(next==='ranking')void ranks()
  if(next==='stage' && s){
    const g=el<HTMLCanvasElement>('mini').getContext('2d')!
    for(let y=0;y<s.height;y++)for(let x=0;x<s.width;x++){
      g.fillStyle=s.map[y][x]==='#'?'#515c53':'#e4e0c7';g.fillRect(x*20,y*20,19,19)
    }
    g.fillStyle='#b34c36';g.fillRect(s.goal.x*20-5,s.goal.y*20-5,10,10)
    el('stage-limit').textContent='制限時間 '+s.timeLimitMs/1000+'秒　→'
  }
  if(next==='home')paint(el('cover-cat'),cat('brow',coats[s?.skin??'snow']?.color??coats.snow.color,s?.skin))
  if(!s)return
  if(next==='skins')el('coats').replaceChildren(...Object.entries(coats).map(([id,v])=>
    card(v.name,cat('calm',v.color,id),s!.skin===id,async()=>{if(await send('skin',{skin:id}))show('skins')})))
  if(next==='equipment'){
    el('abilities').replaceChildren(...s.abilities.map(a=>{
      const owned=s!.owned.includes(a.id)
      const btn=card(a.name,icon(a.id),selected===a.id,()=>{selected=a.id;show('equipment')})
      btn.disabled=!owned;btn.title=owned?a.desc:'未解放';return btn
    }))
    const a=s.abilities.find(a=>a.id===selected)!
    el('brief').textContent=a.name+'　'+a.desc+'　再使用 '+a.cd/1000+'秒'
    el('progress').textContent=s.nextUnlockAt===null?'全能力解放済み':'次の解放まであと'+(s.nextUnlockAt-s.clearsSinceUnlock)+'勝'
  }
  if(next==='result'){
    const won=s.status==='clear'
    paint(el('result-cat'),cat(s.face,coats[s.skin]?.color??s.appearance.bodyColor,s.skin))
    el('stamp').textContent=won?'作戦成功':'作戦終了'
    el('outcome').textContent=won?'ゴールを守った！':'猫がゴールに到着'
    el('score').textContent=won?'妨害なしでゴールまで　'+(s.scoreMs/1000).toFixed(2)+' 秒':''
    el('unlock').textContent=s.unlocked?'新能力「'+s.abilities.find(a=>a.id===s!.unlocked)?.name+'」を解放':''
  }
}
async function api(name:string,body?:unknown){
  const res=await fetch('/api/'+name,body===undefined?{cache:'no-store'}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
  const data=await res.json()
  if(!res.ok)throw Error(data.error??'通信に失敗しました')
  return data
}
async function request(name='state',body?:unknown){
  const serial=++sequence
  try{
    const next:State=await api(name,body)
    if(serial<lastApplied)return false
    lastApplied=serial
    const prev=s
    previousNpc=prev?.roundId===next.roundId && Math.hypot(prev.npc.x-next.npc.x,prev.npc.y-next.npc.y)<.5?prev.npc:null
    received=performance.now();s=next
    if(!prev || prev.roundId!==next.roundId)selected=next.selectedAbilityId
    if(next.status==='running'){
      if(page!=='play')show('play')
      el('timer').textContent=(next.remainingMs/1000).toFixed(1)
      const a=next.abilities.find(a=>a.id===selected)!
      paint(el('active-icon'),icon(a.id));el('ability-name').textContent=a.name
      el('charge').textContent=next.cooldownRemainingMs?'あと '+(next.cooldownRemainingMs/1000).toFixed(1)+'秒':'使用可'
    } else if(prev?.status==='running')show(next.status==='ready'?'home':'result')
    else if(!prev)show(location.hash.startsWith('#rank')?'ranking':page)
    return true
  }catch(error){tell(error instanceof Error?error.message:'接続できません');return false}
}
async function send(name:string,body:unknown){
  if(sending)return false
  sending=true
  try{return await request(name,body)}finally{sending=false}
}
async function cast(p:Point){
  if(!s || !await send('use',{...p,roundId:s.roundId}))return
  mark={...p,at:performance.now()}
}
async function ranks(){
  const panel=el('ranks'), version=rankingVersion
  panel.replaceChildren()
  if(!s)return
  if(!s.signedIn){
    const message=document.createElement('p')
    message.textContent=location.hash==='#rank-login-error'?'ログインを完了できませんでした。':s.loginConfigured?'閲覧と記録の登録にはログインしてください。':'このサーバーではGoogleログインが未設定です。'
    panel.append(message)
    if(s.loginConfigured){const login=document.createElement('a');login.href='/auth/google';login.className='primary';login.textContent='Googleでログイン';panel.append(login)}
    return
  }
  try{
    const {rows}=await api('ranking')
    if(version!==rankingVersion)return
    if(!rows.length)panel.textContent='まだ記録がありません。'
    const list=document.createElement('ol')
    for(const r of rows){
      const li=document.createElement('li');li.textContent=r.rank+'位　'+r.player+'　'+r.ability+'　'+(r.ms/1000).toFixed(2)+'秒';list.append(li)
    }
    const logout=document.createElement('button');logout.textContent='ログアウト';logout.onclick=async()=>{if(await send('logout',{}))show('ranking')}
    panel.append(list,logout)
  }catch(e){tell(String(e))}
}
document.querySelectorAll<HTMLButtonElement>('[data-page]').forEach(b=>b.onclick=()=>show(b.dataset.page as Page))
el('garden').onclick=()=>show('equipment')
el('back').onclick=()=>show(page==='equipment'?'stage':'home')
el('begin').onclick=async()=>{
  if(!s || sending)return
  if(!document.fullscreenElement)void document.documentElement.requestFullscreen?.().catch(()=>{})
  if(!await send('start',{abilityId:selected}) && document.fullscreenElement)void document.exitFullscreen().catch(()=>{})
}
el('retire').onclick=()=>{if(s)void send('retire',{roundId:s.roundId})}
el<HTMLFormElement>('form').onsubmit=async e=>{
  e.preventDefault();const field=el<HTMLTextAreaElement>('comment')
  if(await send('feedback',{text:field.value.trim()})){field.value='';show('home');tell('意見を保存しました。')}
}
show('home')
async function poll(){if(!sending)await request();setTimeout(poll,page==='play'?200:1200)}
void poll();draw()
function coords(e:PointerEvent|MouseEvent):Point{
  const r=canvas.getBoundingClientRect()
  return {x:(e.clientX-r.left)*canvas.width/r.width/tile,y:(e.clientY-r.top)*canvas.height/r.height/tile}
}
canvas.onpointermove=e=>cursor=coords(e);canvas.onpointerleave=()=>cursor=null
canvas.onclick=e=>{
  if(s?.status==='running') void cast(coords(e))
}
function draw(){
  requestAnimationFrame(draw)
  if(!s || page!=='play')return
  ctx.imageSmoothingEnabled=false
  const box=(x:number,y:number,w:number,h:number,color:string)=>{ctx.fillStyle=color;ctx.fillRect(x,y,w,h)}
  for(let y=0;y<s.height;y++)for(let x=0;x<s.width;x++){
    const px=x*tile,py=y*tile
    box(px,py,tile,tile,(x+y)%2?'#e3dfc6':'#ebe7d1')
    box(px+14+(x*17+y*3)%80,py+92,4,8,'#bbb997');box(px+19+(x*17+y*3)%80,py+96,5,4,'#bbb997')
    if(s.map[y][x]==='#'){
      box(px,py,128,128,'#454b48');box(px+3,py+3,122,103,'#90988b')
      box(px+3,py+3,122,9,'#bac0ae');box(px+3,py+104,122,8,'#687269')
      box(px+3,py+50,122,5,'#5a655f');box(px+46,py+4,5,46,'#707b71');box(px+86,py+55,5,48,'#707b71')
      box(px+13,py+13,26,5,'#c4c8ad')
    }
  }
  function pad(p:Point,label:string,color:string){
    const x=p.x*tile,y=p.y*tile;box(x-48,y-48,96,96,color);box(x-40,y-40,80,80,'#364d49')
    ctx.font='bold 40px monospace';ctx.textAlign='center';ctx.fillStyle=color;ctx.fillText(label,x,y+14)
  }
  pad(s.start,selected==='portal'?'↩':'S','#dbdfb0');pad(s.goal,'G','#f8d294')
  const a=s.abilities.find(i=>i.id===selected)!
  for(const obj of s.objects){
    const x=obj.x*tile,y=obj.y*tile
    if(a.radius){ctx.beginPath();ctx.arc(x,y,a.radius*tile,0,Math.PI*2);ctx.fillStyle='#7350b24a';ctx.fill()}
    ctx.globalAlpha=a.id==='gate'&&!obj.closed?.52:1
    box(x-48,y-48,96,96,'#49435b');ctx.drawImage(icon(a.id),x-48,y-48,96,96);ctx.globalAlpha=1
    box(x-40,y+40,80*obj.remainingMs/a.duration,6,'#f5e6b7')
  }
  if(s.beacon)ctx.drawImage(icon(a.id),s.beacon.x*tile-40,s.beacon.y*tile-40,80,80)
  const fraction=Math.min(1,(performance.now()-received)/160)
  const p=s.npc
  const x=Math.round(tile*(previousNpc?previousNpc.x+(p.x-previousNpc.x)*fraction:p.x))
  const y=Math.round(tile*(previousNpc?previousNpc.y+(p.y-previousNpc.y)*fraction:p.y))
  box(x-32,y+38,64,6,'#718c70');ctx.drawImage(cat(s.face,coats[s.skin]?.color??s.appearance.bodyColor,s.skin),x-48,y-48,96,96)
  if(mark && performance.now()-mark.at<520){
    ctx.save();ctx.globalAlpha=1-(performance.now()-mark.at)/520
    ctx.strokeStyle='#b64b36';ctx.lineWidth=6;ctx.beginPath()
    const x=mark.x*tile,y=mark.y*tile
    ctx.moveTo(x-23,y-16);ctx.lineTo(x+15,y+21);ctx.moveTo(x-18,y+20);ctx.lineTo(x+24,y-21);ctx.stroke();ctx.restore()
  }
  if(s.status==='running'&&cursor){
    ctx.strokeStyle='#fff8ce';ctx.lineWidth=4
    if(a.target==='cell')ctx.strokeRect(Math.floor(cursor.x)*tile+2,Math.floor(cursor.y)*tile+2,124,124)
    else{ctx.beginPath();ctx.arc(cursor.x*tile,cursor.y*tile,a.radius*tile,0,Math.PI*2);ctx.stroke()}
  }
}
