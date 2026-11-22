// GitHub Pages runs the same C++ Stage locally. Only static assets are fetched.
import wasmUrl from './stage.wasm?url'
import type {Ability,Point,State} from './main'

type Engine={
  memory:WebAssembly.Memory
  _initialize():void
  configure(seed:number):void
  begin(kind:number):void
  advance(elapsed:number):void
  use(x:number,y:number):number
  retire():void
  score():number
  snapshot():void
}
type Profile={owned:string[];wins:number;selected:string;skin:string;idleFace:string}
const saveKey='webgame-no-named.beta.v1'
const pick=(items:string[])=>items[Math.floor(Math.random()*items.length)]

function readProfile(abilities:Ability[]):Profile{
  const ids=abilities.map(a=>a.id)
  try{
    const p=JSON.parse(localStorage.getItem(saveKey)??'null') as Profile|null
    if(p && Array.isArray(p.owned) && p.owned.length && p.owned.every(id=>ids.includes(id)) &&
       new Set(p.owned).size===p.owned.length && p.owned.includes(p.selected) &&
       Number.isInteger(p.wins) && p.wins>=0 && p.wins<p.owned.length &&
       typeof p.skin==='string' && typeof p.idleFace==='string')return p
  }catch{ /* Storage can be unavailable in private browsing. */ }
  const starter=pick(ids)
  return {owned:[starter],selected:starter,wins:0,skin:'snow',idleFace:'calm'}
}
function save(p:Profile){
  try{localStorage.setItem(saveKey,JSON.stringify(p))}catch{ /* Play continues in memory. */ }
}

async function createGame(){
  let engine:Engine
  const decoder=new TextDecoder()
  function stringAt(pointer:number){
    const bytes=new Uint8Array(engine.memory.buffer)
    return decoder.decode(bytes.subarray(pointer,bytes.indexOf(0,pointer)))
  }
  const abilities:Ability[]=[],map:string[]=[]
  let start:Point,goal:Point,timeLimitMs=0
  let npc:Point,status:State['status']='ready',face='',remainingMs=0,cooldownRemainingMs=0
  let objects:State['objects']=[],beacon:State['beacon']
  const response=await fetch(wasmUrl)
  if(!response.ok)throw Error('ゲームを読み込めませんでした')
  const {instance}=await WebAssembly.instantiate(await response.arrayBuffer(),{
    // No filesystem in this build. libc's unused stream operations return BADF.
    wasi_snapshot_preview1:{fd_close:()=>8,fd_seek:()=>8,fd_write:()=>8},
    view:{
      board(sx:number,sy:number,gx:number,gy:number,limit:number){start={x:sx,y:sy};goal={x:gx,y:gy};timeLimitMs=limit},
      mapRow(row:number){map.push(stringAt(row))},
      ability(id:number,name:number,desc:number,cd:number,duration:number,radius:number,cell:number){
        abilities.push({id:stringAt(id),name:stringAt(name),desc:stringAt(desc),cd,duration,radius,target:cell?'cell':'circle'})
      },
      frame(phase:number,reaction:number,x:number,y:number,left:number,cd:number){
        status=stringAt(phase) as State['status'];face=stringAt(reaction);npc={x,y};remainingMs=left;cooldownRemainingMs=cd
      },
      object(x:number,y:number,remainingMs:number,closed:number){objects.push({x,y,remainingMs,closed:!!closed})},
      beacon(x:number,y:number,remainingMs:number){beacon={x,y,remainingMs}}
    }
  })
  engine=instance.exports as unknown as Engine
  engine._initialize()
  engine.configure(crypto.getRandomValues(new Uint32Array(1))[0])
  const profile=readProfile(abilities)
  save(profile)
  let epoch=0,roundId='',scoreMs=0,unlocked='',rewarded=false
  function sync(){
    engine.advance(performance.now()-epoch)
    objects=[];beacon=undefined;engine.snapshot()
    if(status==='clear' && !rewarded){
      rewarded=true;scoreMs=engine.score()
      const locked=abilities.map(a=>a.id).filter(id=>!profile.owned.includes(id))
      if(locked.length && ++profile.wins>=profile.owned.length){
        unlocked=pick(locked);profile.owned.push(unlocked);profile.wins=0
      }
      save(profile)
    }
  }
  return (action:string,payload:unknown):State=>{
    const data=(payload??{}) as {skin?:string;idleFace?:string;abilityId?:string;roundId?:string;x:number;y:number}
    sync()
    switch(action){
    case 'state':break
    case 'skin':
      if(data.skin)profile.skin=data.skin
      if(data.idleFace)profile.idleFace=data.idleFace
      save(profile);break
    case 'start':{
      const id=data.abilityId??profile.selected
      if(status==='running' || !profile.owned.includes(id))throw Error('この能力で開始できません')
      profile.selected=id;save(profile)
      roundId=crypto.randomUUID();epoch=performance.now();unlocked='';scoreMs=0;rewarded=false
      engine.begin(abilities.findIndex(a=>a.id===id));sync();break
    }
    case 'use':
    case 'retire':{
      if(data.roundId!==roundId)throw Error('ラウンドが切り替わりました')
      if(action==='retire')engine.retire()
      else{
        const error=stringAt(engine.use(data.x,data.y))
        if(error)throw Error(error)
      }
      sync();break
    }
    default:throw Error('この操作はβ版で利用できません')
    }
    return {width:map[0].length,height:map.length,map,start,goal,npc,objects,beacon,abilities,timeLimitMs,
      status,roundId,remainingMs,cooldownRemainingMs,face:face||profile.idleFace,scoreMs,unlocked,
      skin:profile.skin,appearance:{bodyColor:'#F8F1DD',idleFace:profile.idleFace},owned:[...profile.owned],
      selectedAbilityId:profile.selected,clearsSinceUnlock:profile.wins,
      nextUnlockAt:profile.owned.length===abilities.length?null:profile.owned.length,signedIn:false,loginConfigured:false}
  }
}

const game=createGame()
export async function request(action:string,payload:unknown){return (await game)(action,payload)}
