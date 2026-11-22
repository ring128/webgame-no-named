// Browser bridge for the static beta. Movement, effects and scoring use Stage.
#include "stage.h"
#include <cstdlib>

// Ubuntu's WASI libc++ can throw on allocation/length errors. This reactor
// has no exception runtime; treat those unrecoverable conditions as traps.
extern "C" void* __cxa_allocate_exception(std::size_t) noexcept {std::abort();}
extern "C" void __cxa_throw(void*,void*,void (*)(void*)){std::abort();}

using namespace stage;
namespace {
Stage game(1);
std::string error;
}
#define IMPORT(name) __attribute__((import_module("view"), import_name(#name)))
#define EXPORT(name) __attribute__((export_name(#name)))
extern "C" {
IMPORT(board) void board(double sx,double sy,double gx,double gy,int limit);
IMPORT(mapRow) void mapRow(const char*);
IMPORT(ability) void ability(const char* id,const char* name,const char* desc,int cd,int duration,double radius,int isCell);
IMPORT(frame) void frame(const char* status,const char* face,double x,double y,int remaining,int cooldown);
IMPORT(object) void object(double x,double y,int remaining,int closed);
IMPORT(beacon) void beacon(double x,double y,int remaining);

EXPORT(configure) void configure(unsigned seed){
    game.rng.seed(seed);
    board(start.x,start.y,goal.x,goal.y,limit);
    for(auto row:map) mapRow(row);
    for(const auto& a:book)
        ability(a.id,a.name,a.desc,a.cd,a.duration,a.radius,a.target==Tile || a.target==Beacon);
}
EXPORT(begin) void beginRound(int kind){game.begin(static_cast<Kind>(kind));}
EXPORT(advance) void advance(double elapsed){
    while(game.status=="running" && game.time+tick<=elapsed) game.step();
}
EXPORT(use) const char* use(double x,double y){
    error=game.use({x,y});
    return error.c_str();
}
EXPORT(retire) void retire(){if(game.status=="running") game.status="ready";}
EXPORT(score) int score(){return game.unimpededMs();}
EXPORT(snapshot) void snapshot(){
    const auto face=game.face("");
    frame(game.status.c_str(),face.c_str(),game.pos.x,game.pos.y,limit-game.time,
          std::max(0,game.cooldown-game.time));
    for(const auto& o:game.objects) object(o.pos.x,o.pos.y,o.end-game.time,game.closed(o,game.time));
    if(game.targetEnd>game.time) beacon(game.target.x,game.target.y,game.targetEnd-game.time);
}
}
