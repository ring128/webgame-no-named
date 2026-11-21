#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

namespace stage {
struct Vec {
    double x, y;
    bool operator==(const Vec&) const = default;
    Vec operator+(Vec b) const { return {x+b.x,y+b.y}; }
    Vec operator-(Vec b) const { return {x-b.x,y-b.y}; }
    Vec operator*(double k) const { return {x*k,y*k}; }
    double length() const { return std::hypot(x,y); }
};
constexpr int W=11, H=7, tick=20, limit=24000;
constexpr double speed=.8; // tiles per second
constexpr Vec start{1.5,3.5}, goal{9.5,3.5};
constexpr std::array<const char*,H> map{
    ".....#..#..","..#..#.....","..#..#..#..","..#.....#..",
    "..#..#..#..","..#..#.....",".....#..#.."};
inline bool inside(Vec p) { return p.x>=0 && p.y>=0 && p.x<W && p.y<H; }
inline int cell(Vec p) { return inside(p) ? int(p.x)+W*int(p.y) : -1; }
inline Vec center(int id) { return {id%W+.5,id/W+.5}; }
inline bool floorAt(Vec p) { return inside(p) && map[int(p.y)][int(p.x)]!='#'; }

enum Kind { Wall, Stop, Field, Knock, Pull, Vortex, Reverse, Rewind, Trap, Sticky,
            Ice, Gate, Decoy, Lure, Portal, Pulse };
enum Target { Tile, Area, Burst, Beacon };
struct Ability { const char *id,*name,*desc; Target target; int cd, duration; double radius; };
inline constexpr std::array<Ability,16> book{{
    {"wall","経路妨害","通路を一時的にふさぐ。同じマスで延長。",Tile,1200,4500,0},
    {"stop","時間停止","円内の猫の時間を3秒止める。",Burst,5000,3000,2},
    {"slow_field","減速フィールド","円内を歩く猫の速度を半分にする。",Area,5000,6000,2},
    {"knockback","ノックバック","猫を中心から外向きに最大2マス飛ばす。",Burst,4000,0,2},
    {"pull","引き寄せ","円内の猫を中心へ最大2マス引き寄せる。",Burst,4000,0,3},
    {"vortex","渦","毎秒、円内の猫を中心へ1マス引き込む。",Area,7000,5000,2},
    {"reverse","逆走","円内の猫が3秒間、来た道を逆走する。",Burst,6000,3000,2},
    {"rewind","巻き戻し","円内の猫を3秒前の位置へ戻す。",Burst,6000,3000,2},
    {"trap","足止め罠","最初に踏んだ猫を3秒止める。",Tile,4000,8000,0},
    {"sticky","粘着床","踏んでから3秒間、歩く速さを半分にする。",Tile,3000,6000,0},
    {"ice","滑る床","曲がろうとした猫をもう1マス直進させる。",Tile,3000,6000,0},
    {"gate","開閉ゲート","2秒ごとに開閉するゲートを置く。",Tile,6000,8000,0},
    {"decoy_goal","偽ゴール","5秒間、指定したマスをゴールと思わせる。",Beacon,8000,5000,0},
    {"lure","おとり","円内の猫を3秒間クリック地点へ誘う。",Burst,5000,3000,3},
    {"portal","ポータル","踏んだ猫をスタートへ転送する。",Tile,6000,6000,0},
    {"pulse","範囲パルス","広い円内の猫を外向きに1マス押す。",Burst,2500,0,3}
}};
struct Object { Vec pos; int born,end; };
struct Sample { Vec pos; int time,segment; };
struct Stage {
    explicit Stage(unsigned seed=std::random_device{}()): rng(seed) {}
    std::mt19937 rng;
    Kind kind=Wall;
    std::string status="ready", reaction;
    Vec pos=start, heading{1,0}, target=goal;
    int time=0, cooldown=0, paused=0, stopped=0, sticky=0, reversing=0, targetEnd=0;
    int reactionEnd=0, changedEnd=0, segment=0;
    std::vector<Object> objects;
    std::deque<Vec> route, backwards;
    std::deque<Sample> history;
    bool sliding=false;
    Vec slideDir{};

    void begin(Kind k);
    void step();
    std::string use(Vec click);
    std::string face(const std::string& idle) const;
    bool closed(const Object& ob,int at) const;
private:
    bool pass(Vec from,Vec to,int at) const;
    double travel(Vec a,Vec b,double at) const;
    void plan(bool forced=false);
    void shift(Vec delta);
    void moved(Vec next);
    void walk(double distance);
    void react(int duration=800);
};
}
