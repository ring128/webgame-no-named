#include "stage.h"
#include <cstdlib>
#include <iostream>
#include <set>

using namespace stage;
void require(bool value,const char* condition){
    if(!value){std::cerr<<"Failed: "<<condition<<"\n";std::exit(1);}
}
void interactionChecks(){
    std::set<int> exits;
    for(unsigned seed=0;seed<32;++seed){
        Stage g(seed); g.begin(Portal);
        auto route=g.route;
        exits.insert(cell(route.front()));
        auto entrance=route[1];
        require(g.use(entrance).empty() && g.route==route,"portal preserves route");
        while(g.time<6000 && g.segment==0) g.step();
        require(g.segment==1 && g.pos==start && !g.route.empty(),"portal replans from exit");
    }
    require(exits.size()>1,"equal initial routes vary with random seed");
    Stage g(1);g.begin(Wall);
    for(int i=0;i<12;i++) g.step();
    Vec pos=g.pos;Vec before=g.route.front();
    require(g.use(before).empty() && g.route.front()!=before,"mid-edge reroute");
    for(int i=0;i<5;i++){g.step();require(g.pos==pos,"100ms turn pause");}
    g.step();require(g.pos!=pos,"walk after turn pause");
    g.begin(Knock);
    require(g.use({10.5,6.5}).empty() && g.cooldown==2000,"miss refunds half of cooldown");
    g.begin(Knock);
    require(g.use({.6,3.5}).empty(),"hit");
    require(g.pos.x<2 && g.pos.x>1.97 && g.cooldown==4000,"push stops at wall");
    require(g.face("sleepy")=="knockback" && g.face("happy")=="knockback","shared hit face");
    g.begin(Gate);
    require(g.use({1.5,2.5}).empty(),"gate placement");
    auto gate=g.objects.front();
    require(g.closed(gate,1999) && !g.closed(gate,2000) && g.closed(gate,4000) && !g.closed(gate,8000),"gate cycle");
}
// Deterministic replay: the same inputs go through the live game's use/step methods.
bool play(Kind kind,unsigned seed,std::vector<std::pair<int,Vec>>& inputs) {
    Stage game(seed);
    game.begin(kind);
    while(game.status=="running"){
        if(game.time>=game.cooldown && game.time>=1000){
            Vec click=game.pos;
            if(book[kind].target==Tile){
                // Anticipate a turn for ice; otherwise cut the corridor at its entrance.
                for(size_t i=0;i<game.route.size();++i){
                    Vec p=game.route[i];
                    if(cell(p)==cell(game.pos) || p==goal || p==start) continue;
                    if(kind==Ice){
                        if(i+1==game.route.size()) continue;
                        Vec incoming=p-(i?game.route[i-1]:game.pos);
                        Vec straight=p+incoming*(1/std::max(.0001,incoming.length()));
                        if(!floorAt(straight) || (straight-game.route[i+1]).length()<.1) continue;
                    }
                    click=p; break;
                }
            }
            if(kind==Vortex || kind==Lure || kind==Decoy || kind==Pull)
                click=game.pos-game.heading*(kind==Decoy?3:1.8);
            if(kind==Knock || kind==Pulse) click=game.pos+game.heading*.2;
            if(kind==Rewind && game.time<3000) {game.step();continue;}
            if(kind==Decoy && !floorAt(click)) click=start;
            if(game.use(click).empty()) inputs.push_back({game.time,click});
        }
        game.step();
    }
    return game.status=="clear";
}
int main(){
    interactionChecks();
    Stage idle(1); idle.begin(Wall);
    while(idle.status=="running") idle.step();
    if(idle.status!="lost") return 1;
    std::cout<<"idle: goal at "<<idle.time<<" ms\n";
    for(int k=0;k<int(book.size());++k){
        bool won=false;
        for(unsigned seed=1;seed<=30 && !won;++seed){
            std::vector<std::pair<int,Vec>> inputs;
            won=play(Kind(k),seed,inputs);
            if(won){
                std::cout<<book[k].id<<" seed="<<seed;
                for(auto [t,p]:inputs) std::cout<<" "<<t<<"@("<<p.x<<","<<p.y<<")";
                std::cout<<"\n";
            }
        }
        if(!won){std::cerr<<"No clear: "<<book[k].id<<"\n"; return 1;}
    }
}
