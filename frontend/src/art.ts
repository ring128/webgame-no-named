// All sprites are drawn on a 32px backing canvas, including the recolorable cat.
type Pixel = (x:number,y:number,w?:number,h?:number)=>void
const cache = new Map<string,HTMLCanvasElement>()
function sprite(key:string, draw:(ctx:CanvasRenderingContext2D,px:Pixel)=>void) {
  let image=cache.get(key)
  if(image) return image
  image=document.createElement('canvas'); image.width=image.height=32
  const c=image.getContext('2d')!
  draw(c,(x,y,w=1,h=1)=>c.fillRect(x,y,w,h))
  if(cache.size>256) cache.clear()
  cache.set(key,image)
  return image
}
// Each span is a pair of inclusive x coordinates. Mirroring makes the ears and feet symmetric.
const halfBody=[
  '', '8-9','7-10','7-11','6-12','6-14','5-15','5-15','4-15','4-15',
  '3-15','3-15','3-15','3-15','3-15','4-15','4-15','5-15','6-15','8-15',
  '9-15','7-15','6-15','5-15','6-7 9-15','9-15','9-15','10-15','10-15',
  '10-15','10-12',''
]
const body=halfBody.map(row=>{
  const cells=new Set<number>()
  for(const span of row.split(' ').filter(Boolean)){
    const [a,b]=span.split('-').map(Number)
    for(let x=a;x<=b;x++){cells.add(x);cells.add(31-x)}
  }
  return cells
})
const eyes:Record<string,string[]>={
  dot:['000','011','011'], flat:['000','111','010'], up:['010','101','000'],
  shut:['100','010','100'], wide:['111','101','111'], star:['010','111','010'],
  low:['000','000','111'], swirl:['111','100','111','001'], big:['111','111','010']
}
const faces:Record<string,[string,string,string]>={
  calm:['dot','dot','w'], fierce:['dot','dot','v'], wink:['dot','shut','w'], happy:['up','up','w'],
  brow:['dot','dot','w'],sleepy:['flat','flat','w'],sad:['low','low','f'], stop:['wide','wide','o'],
  slow_field:['flat','flat','line'],wall:['dot','flat','f'],knockback:['shut','shut','O'],
  pull:['dot','dot','o'],vortex:['swirl','swirl','w'],reverse:['up','dot','f'],
  rewind:['shut','shut','o'],trap:['shut','dot','line'],sticky:['flat','dot','f'],
  ice:['low','wide','O'],gate:['low','low','line'],decoy_goal:['star','star','v'],
  lure:['big','big','o'],portal:['wide','wide','O'],pulse:['flat','flat','o']
}
export function cat(face:string,color:string,skin='snow'){
  return sprite('cat'+face+color+skin,(c,px)=>{
    const rgb=[1,3,5].map(i=>parseInt(color.slice(i,i+2),16))
    const shade='rgb('+rgb.map((n,i)=>Math.round(n*(i===2?.92:.77))).join(',')+')'
    for(let y=0;y<32;y++) for(const x of body[y]){
      const border=!body[y-1]?.has(x)||!body[y+1]?.has(x)||!body[y].has(x-1)||!body[y].has(x+1)
      c.fillStyle=border?'#302535':(!body[y+2]?.has(x)||y===20||x>24)?shade:color;px(x,y)
      if(!border){
        if(skin==='tabby' && ((y>5 && y<10 && [12,15,19].includes(x)) || (y>21 && y<27 && x<14 && y%3===0))) {
          c.fillStyle='#885B43';px(x,y)
        }
        if(skin==='tux' && ((y>20 && Math.abs(x-15)<(29-y)*.6) || (y>14 && Math.abs(x-15)<4))){
          c.fillStyle='#F8EFDC';px(x,y)
        }
        if(skin==='calico' && ((x<12&&y<11)||(x>20&&y>21)||(x>21&&y<8))){
          c.fillStyle=x<12?'#BC8051':'#5B5454';px(x,y)
        }
      }
    }
    c.fillStyle=shade;px(8,6,2,1);px(22,6,2,1)
    c.fillStyle=skin==='tux'||rgb[0]*.3+rgb[1]*.59+rgb[2]*.11<85?'#fffbe8':'#302535'
    const [a,b,m]=faces[face]??faces.calm
    function stamp(rows:string[],x:number,y:number,mirror=false){
      rows.forEach((row,j)=>[...row].forEach((p,i)=>{if(p==='1')px(x+(mirror?row.length-1-i:i),y+j)}))
    }
    stamp(eyes[a],face==='pull'?10:8,11); stamp(eyes[b],face==='pull'?18:20,11,true)
    px(8,face==='fierce'?8:9,3);px(20,face==='brow'?7:9,3)
    if(face==='fierce'){px(10,9,2);px(19,10,2)}
    const mouth:Record<string,string[]>={w:['10001','10101','01010'],v:['10001','01110'],
      f:['01110','10001'],line:['11111'],o:['0110','0110'],O:['01110','11111','01110']}
    if(skin==='tux') c.fillStyle='#302535'
    stamp(mouth[m],13,15)
  })
}
export function icon(id:string){
  return sprite('icon'+id,(c)=>{
    const ink='#372b46',fill='#fdce7c', light='#fff4ca',teal='#87d8cb'
    c.scale(2,2)
    const rect=(x:number,y:number,w:number,h:number,color=fill)=>{c.fillStyle=color;c.fillRect(x,y,w,h)}
    const line=(x:number,y:number,X:number,Y:number,color=light)=>{
      const n=Math.max(Math.abs(x-X),Math.abs(y-Y),1)
      for(let i=0;i<=n;i++)rect(Math.round(x+(X-x)*i/n),Math.round(y+(Y-y)*i/n),1,1,color)
    }
    const ring=(x:number,y:number,r:number,color=light)=>{
      for(let a=0;a<64;a++)rect(Math.round(x+r*Math.cos(a*Math.PI/32)),Math.round(y+r*Math.sin(a*Math.PI/32)),1,1,color)
    }
    const arrow=(x:number,y:number,dir:number)=>{rect(x-2,y,5,2);line(x+2*dir,y-2,x+4*dir,y);line(x+2*dir,y+3,x+4*dir,y+1)}
    switch(id){
    case 'wall':for(let j=0;j<3;j++)for(let i=0;i<2;i++)rect(1+i*7+j%2,3+j*4,6,3,j%2?teal:light);break
    case 'stop':ring(8,8,6);rect(5,5,2,7);rect(9,5,2,7);break
    case 'slow_field':ring(6,7,4,teal);ring(6,7,2);rect(2,11,11,2);rect(11,7,3,5);break
    case 'knockback':arrow(9,7,1);line(1,4,5,6);line(1,12,5,10);break
    case 'pull':arrow(3,7,1);arrow(12,7,-1);break
    case 'vortex':for(let t=0;t<150;t++){let r=t/25;rect(Math.round(8+r*Math.cos(t/12)),Math.round(8+r*Math.sin(t/12)),1,1,teal)}break
    case 'reverse':rect(3,4,9,2);rect(11,5,2,6);arrow(6,10,-1);break
    case 'rewind':ring(8,8,6,teal);arrow(8,8,-1);rect(8,4,1,4);break
    case 'trap':ring(8,9,5);for(let i=3;i<=12;i+=3)rect(i,4,1,4);break
    case 'sticky':rect(5,7,6,5,teal);for(let i=2;i<14;i+=3)rect(i,4,2,2);rect(6,12,1,3);rect(10,12,1,2);break
    case 'ice':line(1,13,12,13,teal);line(4,11,14,11);rect(6,4,4,5);rect(8,7,6,2);break
    case 'gate':rect(2,2,4,12,teal);rect(10,2,4,12,teal);rect(3,7,1,2,ink);rect(12,7,1,2,ink);break
    case 'decoy_goal':rect(3,2,2,12);rect(5,2,8,4,teal);rect(10,4,4,3,teal);break
    case 'lure':line(3,14,10,3);rect(8,3,4,6,teal);rect(10,1,2,3);break
    case 'portal':ring(5,6,4,teal);ring(11,10,4,fill);break
    case 'pulse':ring(8,8,6,teal);ring(8,8,3);rect(8,8,1,1);break
    }
  })
}
