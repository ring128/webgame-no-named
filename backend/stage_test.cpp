#include "stage.h"
#include <cstdlib>
#include <iostream>

using namespace stage;
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
