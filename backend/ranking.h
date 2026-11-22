#pragma once
#include <json/json.h>
#include <algorithm>
#include <string>

namespace ranking {
inline void record(Json::Value& rows,const std::string& player,const Json::Value& entry){
    if(!rows.isMember(player) || entry["ms"].asInt()>rows[player]["ms"].asInt())
        rows[player]=entry;
}
inline void claim(Json::Value& profile,const std::string& browser,const std::string& user){
    record(profile["rankBests"],user,profile["guestBests"][browser]);
    profile["guestBests"].removeMember(browser);
}
inline Json::Value standings(const Json::Value& scores,const std::string& user){
    auto keys=scores.getMemberNames();
    std::stable_sort(keys.begin(),keys.end(),[&](const auto& a,const auto& b){
        return scores[a]["ms"].asInt()>scores[b]["ms"].asInt();
    });
    Json::Value rows(Json::arrayValue);
    int rank=0,previous=-1;
    for(const auto& key:keys){
        auto row=scores[key];
        int ms=row["ms"].asInt();
        if(ms!=previous) rank=int(rows.size())+1;
        previous=ms;
        row["rank"]=rank;
        row["player"]=key==user?"あなた":"作戦員";
        rows.append(row);
    }
    return rows;
}
}
