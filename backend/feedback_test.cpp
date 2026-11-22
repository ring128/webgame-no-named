#include "feedback.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace fs=std::filesystem;

int main(){
    auto original=fs::current_path();
    auto pattern=(fs::temp_directory_path()/"feedback-test-XXXXXX").string();
    if(!mkdtemp(pattern.data())) return 1;
    fs::path dir=pattern;
    fs::create_directories(dir/"data");
    fs::current_path(dir);
    int result=0;
    try{
        auto require=[](bool ok,const char* label){if(!ok)throw std::runtime_error(label);};
        std::ofstream("data/feedback.jsonl")<<"{\"text\":\"legacy\"}\n";
        feedback::initialize();
        feedback::initialize();
        constexpr auto first=std::numeric_limits<std::int64_t>::max();
        auto rows=feedback::list("",first)["items"];
        require(rows.size()==1 && rows[0]["created"].isNull(),"legacy import must run once");
        require(!feedback::block(rows[0]["id"].asInt64()),"unknown legacy author");
        require(feedback::submit("alice","100%") && feedback::submit("bob","100"),"submission");
        auto match=feedback::list("%",first)["items"];
        require(match.size()==1 && match[0]["author"]=="alice","literal search");
        require(feedback::block(match[0]["id"].asInt64()),"block author");
        feedback::initialize();
        require(!feedback::submit("alice","later") && feedback::submit("bob","later"),"persistent block");
        match=feedback::list("alice",first)["items"];
        require(match.size()==1 && match[0]["blocked"].asBool(),"retain existing feedback");
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}
    fs::current_path(original);
    fs::remove_all(dir);
    return result;
}
