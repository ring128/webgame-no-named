#include "stage.h"
#include "feedback.h"
#include "google_login.h"
#include "ranking.h"
#include <drogon/drogon.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <set>

namespace fs=std::filesystem;
using namespace stage;
using Clock=std::chrono::steady_clock;
using JsonValue=Json::Value;
namespace {
const std::set<std::string> faces{"fierce","wink","happy","calm","brow","sleepy"};
const std::regex hexColor("#[0-9a-fA-F]{6}");
const std::map<std::string,std::string> coats{
    {"snow","#F8F1DD"},{"tabby","#DAA46F"},{"tux","#534E52"},{"calico","#F8EACF"}
};
int indexOf(const std::string& id){
    for(size_t i=0;i<book.size();++i) if(id==book[i].id) return int(i);
    return -1;
}
JsonValue point(Vec p){JsonValue v;v["x"]=p.x;v["y"]=p.y;return v;}
bool validLook(const JsonValue& v){
    return v.isObject() && v["idleFace"].isString() && faces.contains(v["idleFace"].asString())
        && v["bodyColor"].isString() && std::regex_match(v["bodyColor"].asString(),hexColor);
}
struct RequestError {int status; std::string message;};
class GameSession {
    Stage game;
    JsonValue profile, pending;
    fs::path file="data/progress.json";
    Clock::time_point epoch;
    std::string round, unlocked;
    std::string roundBrowser, roundUser;
    int scoreMs=0;
    bool owned(const std::string& id) const{
        return std::find(profile["owned"].begin(),profile["owned"].end(),JsonValue(id))!=profile["owned"].end();
    }
    void save(const JsonValue& value){
        fs::create_directories(file.parent_path());
        fs::path tmp=file; tmp+=".tmp";
        std::ofstream out(tmp);
        out.exceptions(std::ios::badbit|std::ios::failbit);
        out<<value; out.close(); fs::rename(tmp,file);
        profile=value;
    }
    void award(){
        if(profile["lastRewardRound"]==round) return;
        if(pending.isNull()){
            pending=profile;
            scoreMs=game.unimpededMs();
            JsonValue entry;entry["ms"]=scoreMs;entry["ability"]=book[game.kind].name;
            if(roundUser.empty()) ranking::record(pending["guestBests"],roundBrowser,entry);
            else ranking::record(pending["rankBests"],roundUser,entry);
            std::vector<std::string> remaining;
            for(auto& a:book) if(!owned(a.id)) remaining.emplace_back(a.id);
            if(!remaining.empty()){
                int n=pending["clearsSinceUnlock"].asInt()+1;
                if(n>=int(profile["owned"].size())){
                    unlocked=remaining[std::uniform_int_distribution<size_t>(0,remaining.size()-1)(game.rng)];
                    pending["owned"].append(unlocked); n=0;
                }
                pending["clearsSinceUnlock"]=n;
            }
            pending["lastRewardRound"]=round;
        }
        save(pending); pending=JsonValue();
    }
public:
    GameSession(){
        JsonValue look;look["bodyColor"]="#EFE9FF";look["idleFace"]="calm";
        if(fs::exists(file)){
            std::ifstream in(file); in>>profile;
            auto& list=profile["owned"]; std::set<std::string> ids;
            if(!list.isArray() || profile["version"]!=1) throw std::runtime_error("Invalid progress file");
            for(auto& id:list){
                if(!id.isString() || indexOf(id.asString())<0 || !ids.insert(id.asString()).second)
                    throw std::runtime_error("Invalid saved ability");
            }
            if(ids.empty() || !profile["clearsSinceUnlock"].isUInt() ||
               profile["clearsSinceUnlock"].asUInt()>=ids.size()) throw std::runtime_error("Invalid saved progress");
        } else {
            JsonValue initial;
            auto starter=std::uniform_int_distribution<size_t>(0,book.size()-1)(game.rng);
            initial["version"]=1; initial["owned"].append(book[starter].id);
            initial["clearsSinceUnlock"]=0; initial["appearance"]=look;
            save(initial);
        }
        if(!profile.isMember("appearance")) profile["appearance"]=look;
        if(!validLook(profile["appearance"])) throw std::runtime_error("Invalid saved appearance");
        auto first=profile["owned"][0].asString();
        auto id=profile.get("lastAbility",first).asString();
        game.kind=Kind(indexOf(owned(id)?id:first));
    }
    void sync(){
        if(game.status=="running"){
            auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-epoch).count();
            while(game.status=="running" && game.time+tick<=ms) game.step();
        }
        if(game.status=="clear") award();
    }
    JsonValue ranking(const std::string& browser,const std::string& user){
        if(user.empty()) throw RequestError{401,"ランキングにはGoogleログインが必要です"};
        sync();
        if(profile["guestBests"].isMember(browser)){
            auto next=profile;
            ranking::claim(next,browser,user);
            save(next);
        }
        JsonValue result;
        result["rows"]=ranking::standings(profile["rankBests"],user);
        return result;
    }
    void action(const std::string& name,const JsonValue& body,const std::string& browser,const std::string& user){
        sync();
        if(name=="state") return;
        if(!body.isObject()) throw RequestError{400,"操作の形式が正しくありません"};
        if(name=="skin"){
            if(game.status=="running") throw RequestError{409,"プレイ中です"};
            if(!body.isMember("skin") && !body.isMember("idleFace"))
                throw RequestError{400,"種類か表情を選んでください"};
            auto next=profile;
            if(body.isMember("skin")){
                const auto& id=body["skin"];
                if(!id.isString() || !coats.contains(id.asString())) throw RequestError{400,"種類を選んでください"};
                next["skin"]=id;
                next["appearance"]["bodyColor"]=coats.at(id.asString());
            }
            if(body.isMember("idleFace")){
                const auto& face=body["idleFace"];
                if(!face.isString() || !faces.contains(face.asString())) throw RequestError{400,"表情を選んでください"};
                next["appearance"]["idleFace"]=face;
            }
            if(next!=profile) save(next);
            return;
        }
        if(name=="start"){
            if(game.status=="running") throw RequestError{409,"プレイ中です"};
            auto id=body.get("abilityId",profile["owned"][0]);
            if(!id.isString()) throw RequestError{400,"能力IDを指定してください"};
            if(!owned(id.asString())) throw RequestError{409,"未解放の能力です"};
            auto look=body.get("appearance",profile["appearance"]);
            if(!validLook(look)) throw RequestError{400,"顔または色を確認してください"};
            auto next=profile; next["appearance"]=look; next["lastAbility"]=id;
            if(next!=profile) save(next);
            round=drogon::utils::getUuid(); unlocked.clear();
            roundBrowser=browser;roundUser=user;scoreMs=0;
            game.begin(Kind(indexOf(id.asString()))); epoch=Clock::now(); return;
        }
        if(browser!=roundBrowser) throw RequestError{409,"開始したブラウザで操作してください"};
        if(name!="block" && body["roundId"]!=round) throw RequestError{409,"ラウンドが切り替わりました"};
        if(name=="retire"){
            if(game.status=="running") game.status="ready";
            return;
        }
        if(!body["x"].isNumeric() || !body["y"].isNumeric()) throw RequestError{400,"盤面の座標を指定してください"};
        Vec p{body["x"].asDouble(),body["y"].asDouble()};
        if(name=="block"){
            if(game.kind!=Wall) throw RequestError{409,"経路妨害を選んでください"};
            if(!body["x"].isInt() || !body["y"].isInt()) throw RequestError{400,"マスの番号を指定してください"};
            p=p+Vec{.5,.5};
        }
        auto error=game.use(p); if(!error.empty()) throw RequestError{409,error};
    }
    JsonValue state() const{
        JsonValue v, abilities(Json::arrayValue), objects(Json::arrayValue);
        for(auto& a:book){
            JsonValue row;row["id"]=a.id;row["name"]=a.name;row["desc"]=a.desc;
            row["cd"]=a.cd;row["radius"]=a.radius;row["duration"]=a.duration;
            row["target"]=a.target==Tile || a.target==Beacon?"cell":"circle";abilities.append(row);
        }
        for(auto& ob:game.objects){
            auto row=point(ob.pos);row["remainingMs"]=ob.end-game.time;row["closed"]=game.closed(ob,game.time);
            objects.append(row);
        }
        v["width"]=W;v["height"]=H;
        for(auto row:map) v["map"].append(row);
        v["start"]=point(start);v["goal"]=point(goal);v["npc"]=point(game.pos);
        auto look=profile["appearance"];v["appearance"]=look;
        v["skin"]=profile.get("skin","snow");v["scoreMs"]=scoreMs;
        v["face"]=game.face(look["idleFace"].asString());
        v["timeLimitMs"]=limit;v["remainingMs"]=limit-game.time;
        v["cooldownRemainingMs"]=std::max(0,game.cooldown-game.time);
        v["abilities"]=abilities;v["objects"]=objects;v["owned"]=profile["owned"];
        if(game.targetEnd>game.time){
            v["beacon"]=point(game.target);
            v["beacon"]["remainingMs"]=game.targetEnd-game.time;
        }
        v["selectedAbilityId"]=book[game.kind].id;v["clearsSinceUnlock"]=profile["clearsSinceUnlock"];
        v["nextUnlockAt"]=profile["owned"].size()==book.size()?JsonValue():JsonValue(profile["owned"].size());
        v["roundId"]=round;v["unlocked"]=unlocked;v["status"]=game.status;
        return v;
    }
};
}
int main(int argc,char** argv){
    using namespace drogon;
    GameSession session; std::mutex gate;
    // Serve only the frontend output (never the working directory or save file).
    fs::path root=argc>1?argv[1]:"frontend/dist";
    if(!fs::is_regular_file(root/"index.html")){
        std::cerr<<"Build the frontend first, then pass the dist directory.\n";return 1;
    }
    const int port=argc>2?std::stoi(argv[2]):8848;
    app().enableSession(86400*7,Cookie::SameSite::kLax);
    feedback::initialize();
    feedback::registerAdmin();
    google_login::routes(port);
    for(std::string name:{"state","start","use","block","retire","ranking","feedback","skin","logout"}){
        app().registerHandler("/api/"+name,[&,name](const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback){
            int status=200;JsonValue result;
            std::string browser=req->getCookie("notebook_player");
            bool issueCookie=name=="logout" || browser.size()!=32 ||
                browser.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos;
            if(issueCookie) browser=utils::getUuid();
            auto feedbackId=req->getCookie("feedback_id");
            bool newAuthor=name=="feedback" && (feedbackId.size()!=32 ||
                feedbackId.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos);
            if(newAuthor) feedbackId=utils::getSha256("feedback cookie:"+browser).substr(0,32);
            {
                std::lock_guard lock(gate);
                try{
                    JsonValue body(Json::objectValue);
                    if(!req->body().empty()){
                        auto json=req->getJsonObject();
                        if(!json) throw RequestError{400,"JSONを確認してください"};
                        body=*json;
                    }
                    auto auth=req->session();
                    if(name=="logout"){auth->erase("googleSub");auth->changeSessionIdToClient();}
                    auto user=auth->get<std::string>("googleSub");
                    if(name=="ranking") result=session.ranking(browser,user);
                    else{
                        if(name=="feedback"){
                            if(!body.isObject() || !body["text"].isString()) throw RequestError{400,"意見を入力してください"};
                            auto message=body["text"].asString();
                            auto chars=std::count_if(message.begin(),message.end(),[](unsigned char c){return (c&0xc0)!=0x80;});
                            if(message.find_first_not_of(" \r\n\t")==std::string::npos || chars>1000 || message.size()>4000)
                                throw RequestError{400,"意見は1〜1000文字で入力してください"};
                            session.sync();
                            if(!feedback::submit(utils::getSha256("feedback:"+feedbackId).substr(0,32),message))
                                throw RequestError{403,"この投稿者からの意見は受け付けていません"};
                        }else if(name!="logout") session.action(name,body,browser,user);
                        result=session.state();
                    }
                    result["signedIn"]=!user.empty();result["loginConfigured"]=google_login::configured();
                }catch(const RequestError& e){
                    status=e.status; result["error"]=e.message;
                }catch(const std::exception& e){
                    status=503;LOG_ERROR<<e.what();result["error"]="進捗を保存できません。保存先を確認してください";
                }
            }
            auto resp=HttpResponse::newHttpJsonResponse(result);
            if(issueCookie){
                Cookie cookie("notebook_player",browser);
                cookie.setHttpOnly(true);cookie.setSameSite(Cookie::SameSite::kLax);
                cookie.setPath("/");cookie.setMaxAge(365*86400);resp->addCookie(cookie);
            }
            if(newAuthor){
                Cookie cookie("feedback_id",feedbackId);
                cookie.setHttpOnly(true);cookie.setSameSite(Cookie::SameSite::kLax);
                cookie.setPath("/api/feedback");cookie.setMaxAge(365*86400);resp->addCookie(cookie);
            }
            resp->setStatusCode(static_cast<HttpStatusCode>(status));
            resp->addHeader("Cache-Control","no-store");callback(resp);
        },{name=="state" || name=="ranking"?Get:Post});
    }
    app().setUploadPath("data/uploads").setDocumentRoot(fs::absolute(root).string())
         .addListener("127.0.0.1",port).run();
}
