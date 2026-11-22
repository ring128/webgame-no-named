#include "feedback.h"
#include <drogon/drogon.h>
#include <charconv>
#include <fstream>
#include <limits>

namespace feedback {
using namespace drogon;
void registerAdmin(){
    std::ifstream keyFile("data/feedback-admin.key");
    std::string key;
    std::getline(keyFile,key);
    const std::string expected=key.size()==64?utils::getSha256("Bearer "+key):"";
    for(bool blocking:{false,true}){
        app().registerHandler(blocking?"/api/admin/feedback/block":"/api/admin/feedback",
            [expected,blocking](const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& reply){
                Json::Value out;HttpStatusCode status=k200OK;
                try{
                    if(expected.empty() || utils::getSha256(req->getHeader("Authorization"))!=expected){
                        status=k401Unauthorized;out["error"]="管理キーを確認してください";
                    }else if(blocking){
                        auto body=req->getJsonObject();
                        if(!body || !body->isObject() || !(*body)["id"].isInt64() || (*body)["id"].asInt64()<=0){
                            status=k400BadRequest;out["error"]="投稿を指定してください";
                        }else if(!block((*body)["id"].asInt64())){
                            status=k404NotFound;out["error"]="この投稿の投稿者は特定できません";
                        }else out["blocked"]=true;
                    }else{
                        auto value=req->getParameter("before");
                        std::int64_t before=std::numeric_limits<std::int64_t>::max();
                        if(!value.empty()){
                            auto [end,ec]=std::from_chars(value.data(),value.data()+value.size(),before);
                            if(ec!=std::errc{} || end!=value.data()+value.size() || before<=0){
                                status=k400BadRequest;out["error"]="ページ番号を確認してください";
                            }
                        }
                        if(status==k200OK) out=list(req->getParameter("q"),before);
                    }
                }catch(const std::exception& e){
                    LOG_ERROR<<e.what();status=k503ServiceUnavailable;out["error"]="意見データベースを確認してください";
                }
                auto res=HttpResponse::newHttpJsonResponse(out);res->setStatusCode(status);
                res->addHeader("Cache-Control","no-store");reply(res);
            },{blocking?Post:Get});
    }
}
}
