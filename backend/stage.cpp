#include "stage.h"
#include <limits>
#include <queue>

namespace stage {
namespace {
constexpr double eps=1e-7;
constexpr std::array<Vec,4> directions{{{1,0},{0,1},{-1,0},{0,-1}}};
double dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y; }
Vec unit(Vec v) { return v.length()<eps ? Vec{0,0} : v*(1/v.length()); }
double crossing(Vec a,Vec b) {
    Vec d=b-a; double t=1;
    if (int(a.x)!=int(b.x)) t=std::min(t,((d.x>0?std::floor(a.x)+1:std::floor(a.x))-a.x)/d.x);
    if (int(a.y)!=int(b.y)) t=std::min(t,((d.y>0?std::floor(a.y)+1:std::floor(a.y))-a.y)/d.y);
    return t;
}
}
void Stage::begin(Kind k) {
    Stage clean(rng());
    *this=std::move(clean);
    kind=k; status="running"; history.push_back({pos,0,0}); plan();
}
void Stage::react(int duration) {
    reaction=book[kind].id; reactionEnd=time+std::max(800,duration);
}
std::string Stage::face(const std::string& idle) const {
    if(status=="lost") return "happy";
    if(status=="clear") return "sad";
    if(time<reactionEnd) return reaction;
    if(time<changedEnd) return "brow";
    if(status=="running" && time<800) return "fierce";
    return idle;
}
bool Stage::closed(const Object& ob,int at) const {
    return at<ob.end && (kind==Wall || (kind==Gate && (at-ob.born)%4000<2000));
}
bool Stage::pass(Vec from,Vec to,int at) const {
    auto allowed=[&](Vec p){
        if(!floorAt(p)) return false;
        for(const auto& ob:objects)
            if(cell(from)!=cell(p) && cell(ob.pos)==cell(p) && closed(ob,at)) return false;
        return true;
    };
    if(!allowed(to)) return false;
    if(int(from.x)!=int(to.x) && int(from.y)!=int(to.y)){
        auto d=to-from;
        double tx=((d.x>0?std::floor(from.x)+1:std::floor(from.x))-from.x)/d.x;
        double ty=((d.y>0?std::floor(from.y)+1:std::floor(from.y))-from.y)/d.y;
        if(tx<=ty+eps && !allowed({to.x,from.y})) return false;
        if(ty<=tx+eps && !allowed({from.x,to.y})) return false;
    }
    return true;
}
// Evaluate the partial first edge exactly like later center-to-center edges.
double Stage::travel(Vec a,Vec b,double at) const {
    double len=(b-a).length(), base=len*1000/speed;
    if(!pass(a,b,limit+1)) return 1e12;
    double elapsed=base;
    std::vector<std::pair<double,double>> slow;
    for(const auto& ob:objects){
        if(ob.end<=at) continue;
        if((kind==Field || kind==Vortex) && len>eps){
            Vec u=unit(b-a), rel=ob.pos-a;
            double mid=dot(rel,u), sq=book[kind].radius*book[kind].radius-dot(rel,rel)+mid*mid;
            if(sq<=0) continue;
            double lo=std::max(0.,mid-std::sqrt(sq)), hi=std::min(len,mid+std::sqrt(sq));
            if(hi>lo && at+lo*1000/speed<ob.end)
                slow.emplace_back(lo,std::min(hi,lo+(ob.end-at-lo*1000/speed)*speed/2000));
        }
        if(cell(a)==cell(b) || cell(b)!=cell(ob.pos)) continue;
        double entry=at+base*crossing(a,b);
        if(entry>=ob.end) continue;
        if(closed(ob,int(entry))){
            double open=kind==Wall?ob.end:std::min(ob.end,ob.born+(int(entry-ob.born)/4000)*4000+2000);
            elapsed+=std::max(0.,open-entry);
        } else if(kind==Trap) elapsed+=3000;
        else if(kind==Sticky) elapsed+=1500;
    }
    std::sort(slow.begin(),slow.end());
    double last=0;
    for(auto [lo,hi]:slow){ elapsed+=std::max(0.,hi-std::max(last,lo))*1000/speed; last=std::max(last,hi); }
    return elapsed;
}
void Stage::plan(bool forced) {
    if(reversing>time || sliding) return;
    struct Node {double f,g; int id; unsigned tie; bool operator<(const Node& n)const{
        return f==n.f?tie<n.tie:f>n.f;
    }};
    std::priority_queue<Node> open;
    std::array<double,W*H> best; best.fill(1e12);
    std::array<int,W*H> parent; parent.fill(-1);
    auto offer=[&](int id,int prev,double cost){
        if(cost>=best[id]-eps) return;
        best[id]=cost; parent[id]=prev;
        Vec d=center(id)-center(cell(target));
        open.push({cost+(std::abs(d.x)+std::abs(d.y))*1000/speed,cost,id,static_cast<unsigned>(rng())});
    };
    int here=cell(pos), dest=cell(target);
    Vec origin=center(here), off=pos-origin;
    if(off.length()<eps) offer(here,-1,time);
    else {
        auto seed=[&](Vec p){ if(floorAt(p)) offer(cell(p),-1,time+travel(pos,p,time)); };
        seed(origin);
        for(auto d:directions)
            if((std::abs(off.x)>eps && std::abs(off.y)>eps) || dot(off,d)>eps)
                seed(origin+d);
    }
    while(!open.empty()){
        auto n=open.top(); open.pop();
        if(n.g!=best[n.id]) continue;
        if(n.id==dest) break;
        auto p=center(n.id);
        for(auto d:directions) if(floorAt(p+d))
            offer(cell(p+d),n.id,n.g+travel(p,p+d,n.g));
    }
    std::deque<Vec> fresh;
    if(best[dest]<1e12){
        for(int i=dest;i>=0;i=parent[i]) fresh.push_front(center(i));
        if(!fresh.empty() && (fresh.front()-pos).length()<eps) fresh.pop_front();
        if(target!=center(dest)) fresh.push_back(target);
    }
    if(fresh==route) return;
    if(!route.empty() && !forced && route.back()==target){
        double oldTime=time;
        Vec prev=pos;
        for(auto p:route){ oldTime+=travel(prev,p,oldTime); prev=p; }
        if(oldTime<=best[dest]+(paused>time?0:100)+eps) return;
    }
    if(!route.empty() || forced){
        if(time>=paused) paused=time+100;
        changedEnd=time+600;
        if(kind==Wall || kind==Gate) react();
    }
    route=std::move(fresh);
}
void Stage::moved(Vec next) {
    int before=cell(pos);
    if((next-pos).length()>eps) heading=unit(next-pos);
    pos=next;
    if(cell(pos)==cell(goal)){status="lost"; return;}
    if(before==cell(pos)) return;
    for(auto& ob:objects){
        if(ob.end<=time || cell(ob.pos)!=cell(pos)) continue;
        switch(kind){
        case Portal:
            pos=start; ++segment; react(); plan(true); return;
        case Trap:
            stopped=time+3000; ob.end=time; react(3000); return;
        case Sticky:
            sticky=time+3000; react(3000); return;
        case Ice: slideDir=heading; return;
        default: break;
        }
    }
}
void Stage::shift(Vec delta){
    const int pieces=std::max(1,int(std::ceil(delta.length()*80)));
    Vec stride=delta*(1./pieces);
    for(int i=0;i<pieces && status=="running";++i){
        if(!pass(pos,pos+stride,time)) break;
        moved(pos+stride);
    }
    plan(true);
}
void Stage::walk(double budget){
    while(budget>eps && !route.empty() && status=="running"){
        Vec next=route.front(), d=next-pos;
        double length=d.length(), amount=std::min(length,budget);
        Vec step=length<=budget ? next : pos+unit(d)*amount;
        if(!pass(pos,step,time)){
            if(kind==Wall || kind==Gate) react();
            return;
        }
        if(length<=budget) route.pop_front();
        int priorSegment=segment;
        moved(step);
        if(status!="running" || priorSegment!=segment || stopped>time) return;
        budget-=amount;
        if(length>amount) break;
        if(sliding){sliding=false; plan(true); return;}
        if(slideDir.length()>.1){
            Vec straight=next+slideDir;
            if(!route.empty() && (route.front()-straight).length()>.1 && pass(pos,straight,time)){
                route={straight}; sliding=true; react();
            }
            slideDir={};
        }
    }
}
void Stage::step(){
    if(status!="running") return;
    time+=tick;
    bool dirty=false;
    for(auto& ob:objects){
        if(kind==Vortex && time<=ob.end && (time-ob.born)%1000==0 &&
           (pos-ob.pos).length()<=book[kind].radius){
            Vec d=ob.pos-pos; shift(unit(d)*std::min(1.,d.length())); react();
        }
        if(kind==Gate && (time-ob.born)%2000==0) dirty=true;
    }
    if(std::erase_if(objects,[&](auto& o){return time>=o.end;}) && kind!=Portal) dirty=true;
    if(targetEnd && time>=targetEnd){targetEnd=0; target=goal; dirty=true;}
    if(dirty) plan();
    if(time>=paused && time>=stopped && status=="running"){
        if(reversing){
            double budget=speed*tick/1000.;
            while(time<reversing && !backwards.empty() && budget>eps){
                auto d=backwards.front()-pos;
                double length=d.length(), amount=std::min(length,budget);
                Vec next=pos+unit(d)*amount;
                if(!pass(pos,next,time)){backwards.clear(); break;}
                moved(next); budget-=amount;
                if(amount>=length) backwards.pop_front();
            }
            if(time>=reversing || backwards.empty()){ reversing=0; plan(true); }
        } else {
            bool slow=time<sticky;
            if(kind==Field) for(auto& ob:objects) if((pos-ob.pos).length()<=2){slow=true;react();}
            walk(speed*tick/1000.*(slow?.5:1));
        }
    }
    history.push_back({pos,time,segment});
    while(history.front().time<time-8000) history.pop_front();
    if(time>=limit && status=="running") status="clear";
}
std::string Stage::use(Vec click){
    if(status!="running") return "ゲームを開始してください";
    if(!inside(click)) return "盤面の中を選んでください";
    if(time<cooldown) return "再使用までお待ちください";
    const auto& a=book[kind];
    Vec tile=center(cell(click));
    if(a.target==Tile && (!floorAt(click) || tile==start || tile==goal || cell(click)==cell(pos)))
        return "このマスには置けません";
    if((kind==Lure || kind==Decoy) && !floorAt(click)) return "通路を選んでください";
    Vec delta=pos-click;
    bool hit=delta.length()<=a.radius;
    cooldown=time+((a.target==Burst && !hit)?a.cd/2:a.cd);
    if(a.target==Burst && !hit) return {};
    if(a.target==Area || a.target==Tile){
        if(a.target==Tile) std::erase_if(objects,[&](auto& o){return cell(o.pos)==cell(click);});
        objects.push_back({a.target==Tile?tile:click,time,time+a.duration});
        if(kind!=Portal) plan();
        return {};
    }
    react(a.duration);
    if(kind==Stop) stopped=time+a.duration;
    if(kind==Pulse || kind==Knock || kind==Pull){
        Vec dir=delta.length()>eps?unit(delta):heading;
        shift(kind==Pull?dir*(-std::min(2.,delta.length())):dir*(kind==Pulse?1:2));
    }
    if(kind==Reverse){
        backwards.clear();
        for(auto it=history.rbegin();it!=history.rend() && it->segment==segment;++it)
            if(backwards.empty() || backwards.back()!=it->pos) backwards.push_back(it->pos);
        reversing=time+a.duration;
    }
    if(kind==Rewind){
        for(auto& h:history) if(h.time>=time-a.duration && pass(h.pos,h.pos,time)){
            pos=h.pos; ++segment; break;
        }
        plan(true);
    }
    if(kind==Decoy || kind==Lure){target=kind==Decoy?tile:click;targetEnd=time+a.duration;plan();}
    return {};
}
}
