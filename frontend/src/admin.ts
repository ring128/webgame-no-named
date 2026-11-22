import './style.css'

type Post={id:number;author:string|null;created:string|null;text:string;blocked:boolean}
type Results={items:Post[];before?:number}
const login=document.querySelector<HTMLFormElement>('#login')!
const panel=document.querySelector<HTMLElement>('#panel')!
const password=document.querySelector<HTMLInputElement>('#key')!
const search=document.querySelector<HTMLFormElement>('#search')!
const query=document.querySelector<HTMLInputElement>('#query')!
const posts=document.querySelector<HTMLOListElement>('#posts')!
const more=document.querySelector<HTMLButtonElement>('#more')!
const empty=document.querySelector<HTMLElement>('#empty')!
const message=document.querySelector<HTMLElement>('#message')!
let secret='',filter='',before:number|undefined,version=0

function closePanel(){
  ++version;secret='';query.value='';password.value='';posts.replaceChildren()
  panel.hidden=true;login.hidden=false;message.textContent='';password.focus()
}
async function api(path:string,body?:object){
  const headers:Record<string,string>={Authorization:'Bearer '+secret}
  if(body)headers['Content-Type']='application/json'
  const res=await fetch('/api/admin/feedback'+path,{
    method:body?'POST':'GET',headers,cache:'no-store',body:body?JSON.stringify(body):undefined
  })
  const data=await res.json()
  if(!res.ok)throw Error(data.error??'通信に失敗しました')
  return data
}
function row(post:Post){
  const li=document.createElement('li'),meta=document.createElement('small'),body=document.createElement('p')
  meta.textContent=(post.created?new Date(post.created).toLocaleString('ja-JP'):'日時不明')+'　'+(post.author??'投稿者不明')
  body.textContent=post.text;li.append(meta,body)
  if(post.author){
    const block=document.createElement('button')
    block.dataset.author=post.author;block.disabled=post.blocked
    block.textContent=post.blocked?'ブロック済み':'この投稿者をブロック'
    block.onclick=async()=>{
      if(!confirm('この匿名IDからの今後の投稿を拒否しますか？'))return
      block.disabled=true
      try{
        await api('/block',{id:post.id})
        for(const button of posts.querySelectorAll('button')){
          if(button.dataset.author===post.author){button.disabled=true;button.textContent='ブロック済み'}
        }
      }catch(e){block.disabled=false;message.textContent=String(e)}
    }
    li.append(block)
  }
  return li
}
async function load(append=false){
  const request=++version
  if(!append)filter=query.value.trim()
  const params=new URLSearchParams({q:filter})
  if(append && before)params.set('before',String(before))
  more.disabled=true;message.textContent=''
  try{
    const data:Results=await api('?'+params)
    if(request!==version)return
    if(!append)posts.replaceChildren()
    posts.append(...data.items.map(row))
    before=data.before;more.hidden=!before;empty.hidden=posts.childElementCount>0
    password.value='';login.hidden=true;panel.hidden=false
  }catch(e){if(request===version)message.textContent=String(e)}
  finally{if(request===version)more.disabled=false}
}
login.onsubmit=e=>{e.preventDefault();secret=password.value.trim();void load()}
search.onsubmit=e=>{e.preventDefault();void load()}
more.onclick=()=>void load(true)
document.querySelector<HTMLButtonElement>('#close')!.onclick=closePanel
window.addEventListener('pagehide',closePanel)
