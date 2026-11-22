#include "ranking.h"
#include <iostream>

int main(){
    Json::Value profile,entry;
    auto& scores=profile["rankBests"];
    if(!ranking::standings(scores,"a").empty()) return 1;
    entry["ms"]=1000;entry["ability"]="wall";
    ranking::record(scores,"a",entry);
    entry["ms"]=2000;entry["ability"]="gate";
    ranking::record(profile["guestBests"],"browser-a",entry);
    ranking::claim(profile,"browser-a","a");
    if(profile["guestBests"].isMember("browser-a")) return 1;
    ranking::record(scores,"b",entry);
    entry["ms"]=500;
    ranking::record(scores,"c",entry);
    ranking::record(profile["guestBests"],"browser-a",entry);
    ranking::claim(profile,"browser-a","a");
    auto rows=ranking::standings(scores,"a");
    if(rows.size()!=3 || rows[0]["ms"]!=2000 || rows[0]["ability"]!="gate" ||
       rows[0]["player"]!="あなた" || rows[0]["rank"]!=1 || rows[1]["rank"]!=1 ||
       rows[2]["rank"]!=3 || rows[2]["player"]!="作戦員"){
        std::cerr<<rows<<'\n';
        return 1;
    }
}
