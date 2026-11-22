#pragma once
#include <drogon/drogon.h>
#include <cstdlib>
#include <ctime>

namespace google_login {
using namespace drogon;
using Reply=std::function<void(const HttpResponsePtr&)>;
inline std::string env(const char* key){
    auto value=std::getenv(key);return value?value:"";
}
inline bool configured(){
    return !env("GOOGLE_CLIENT_ID").empty() && !env("GOOGLE_CLIENT_SECRET").empty();
}
inline HttpResponsePtr redirect(const std::string& url){
    auto r=HttpResponse::newRedirectionResponse(url);
    r->addHeader("Cache-Control","no-store");r->addHeader("Referrer-Policy","no-referrer");return r;
}
inline void routes(int port){
    const auto callbackFor=[port](const HttpRequestPtr& req){
        auto host=req->getHeader("host");
        if(host!="localhost:"+std::to_string(port) && host!="127.0.0.1:"+std::to_string(port)) return std::string{};
        return "http://"+host+"/auth/google/callback";
    };
    app().registerHandler("/auth/google",[callbackFor](const HttpRequestPtr& req,Reply&& reply){
        auto callback=callbackFor(req);
        if(!configured()){reply(redirect("/#rank-unavailable"));return;}
        if(callback.empty()){reply(redirect("/#rank-login-error"));return;}
        auto state=utils::getUuid();
        auto s=req->session();
        s->modify<std::string>("oauthState",[&](auto& v){v=state;});
        s->modify<std::time_t>("oauthUntil",[](auto& v){v=std::time(nullptr)+600;});
        reply(redirect("https://accounts.google.com/o/oauth2/v2/auth?response_type=code&scope=openid&client_id="+
          utils::urlEncodeComponent(env("GOOGLE_CLIENT_ID"))+"&redirect_uri="+utils::urlEncodeComponent(callback)+"&state="+state));
    },{Get});
    app().registerHandler("/auth/google/callback",[callbackFor](const HttpRequestPtr& req,Reply&& reply){
        auto callback=callbackFor(req);
        auto s=req->session();std::string expected;
        s->modify<std::string>("oauthState",[&](auto& v){expected.swap(v);});
        auto fail=[reply]{reply(redirect("/#rank-login-error"));};
        if(!configured() || callback.empty() || expected.empty() || expected!=req->getParameter("state") ||
           s->get<std::time_t>("oauthUntil")<std::time(nullptr) || req->getParameter("code").empty()){fail();return;}
        auto form=HttpRequest::newHttpRequest();
        form->setPath("/token");form->setMethod(Post);form->setContentTypeCode(CT_APPLICATION_X_FORM);
        form->setBody("grant_type=authorization_code&code="+utils::urlEncodeComponent(req->getParameter("code"))+
          "&redirect_uri="+utils::urlEncodeComponent(callback)+"&client_id="+utils::urlEncodeComponent(env("GOOGLE_CLIENT_ID"))+
          "&client_secret="+utils::urlEncodeComponent(env("GOOGLE_CLIENT_SECRET")));
        auto token=HttpClient::newHttpClient("https://oauth2.googleapis.com",nullptr,false,true);
        token->sendRequest(form,[token,s,reply,fail](ReqResult r,const HttpResponsePtr& res){
            if(r!=ReqResult::Ok || res->statusCode()!=k200OK){fail();return;}
            auto json=res->getJsonObject();
            if(!json || !(*json)["access_token"].isString()){fail();return;}
            auto info=HttpRequest::newHttpRequest();info->setPath("/v1/userinfo");
            info->addHeader("Authorization","Bearer "+(*json)["access_token"].asString());
            auto provider=HttpClient::newHttpClient("https://openidconnect.googleapis.com",nullptr,false,true);
            provider->sendRequest(info,[provider,s,reply,fail](ReqResult r,const HttpResponsePtr& res){
                if(r!=ReqResult::Ok || res->statusCode()!=k200OK){fail();return;}
                auto json=res->getJsonObject();
                if(!json || !(*json)["sub"].isString() || (*json)["sub"].asString().empty()){fail();return;}
                s->modify<std::string>("googleSub",[&](auto& v){v=(*json)["sub"].asString();});
                s->changeSessionIdToClient();reply(redirect("/#ranking"));
            },10);
        },10);
    },{Get});
}
}
