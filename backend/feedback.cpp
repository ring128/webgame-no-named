#include "feedback.h"
#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sys/stat.h>

namespace feedback {
namespace {
constexpr auto directory="data/feedback";
struct Db {
    std::unique_ptr<sqlite3,decltype(&sqlite3_close)> handle{nullptr,sqlite3_close};
    Db(){
        sqlite3* raw=nullptr;
        int rc=sqlite3_open_v2("data/feedback/posts.sqlite3",&raw,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE,nullptr);
        handle.reset(raw);
        if(rc!=SQLITE_OK) throw std::runtime_error("Cannot open feedback database");
        sqlite3_busy_timeout(raw,5000);
    }
    void exec(const char* sql){
        if(sqlite3_exec(handle.get(),sql,nullptr,nullptr,nullptr)!=SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(handle.get()));
    }
};
class Query {
    sqlite3* db;
    std::unique_ptr<sqlite3_stmt,decltype(&sqlite3_finalize)> stmt{nullptr,sqlite3_finalize};
    void check(int rc){if(rc!=SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db));}
public:
    Query(Db& connection,const char* sql):db(connection.handle.get()){
        sqlite3_stmt* raw=nullptr;
        check(sqlite3_prepare_v2(db,sql,-1,&raw,nullptr));stmt.reset(raw);
    }
    void bind(int index,const std::string& value){
        check(sqlite3_bind_text(stmt.get(),index,value.data(),int(value.size()),SQLITE_TRANSIENT));
    }
    void bind(int index,std::int64_t value){check(sqlite3_bind_int64(stmt.get(),index,value));}
    bool next(){
        int rc=sqlite3_step(stmt.get());
        if(rc==SQLITE_ROW) return true;
        if(rc!=SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(db));
        return false;
    }
    std::int64_t number(int col){return sqlite3_column_int64(stmt.get(),col);}
    Json::Value text(int col){
        const auto* value=sqlite3_column_text(stmt.get(),col);
        if(!value) return {};
        return std::string(reinterpret_cast<const char*>(value),sqlite3_column_bytes(stmt.get(),col));
    }
};
}
void initialize(){
    std::filesystem::create_directories(directory);
    if(chmod(directory,0700)!=0) throw std::runtime_error("Cannot protect feedback directory");
    Db db;
    db.exec("BEGIN IMMEDIATE");
    db.exec("CREATE TABLE IF NOT EXISTS posts(id INTEGER PRIMARY KEY, author TEXT, created TEXT, body TEXT NOT NULL);"
            "CREATE TABLE IF NOT EXISTS blocked(author TEXT PRIMARY KEY NOT NULL)");
    Query version(db,"PRAGMA user_version");version.next();
    if(version.number(0)==0){
        const std::filesystem::path old="data/feedback.jsonl";
        if(std::filesystem::exists(old)){
            std::ifstream in(old);
            if(!in) throw std::runtime_error("Cannot open legacy feedback");
            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            for(std::string line;std::getline(in,line);){
                Json::Value row;std::string error;
                if(!reader->parse(line.data(),line.data()+line.size(),&row,&error) || !row["text"].isString())
                    throw std::runtime_error("Invalid legacy feedback");
                Query insert(db,"INSERT INTO posts(body) VALUES(?1)");
                insert.bind(1,row["text"].asString());insert.next();
            }
            if(in.bad()) throw std::runtime_error("Cannot read legacy feedback");
        }
        db.exec("PRAGMA user_version=1");
    }
    db.exec("COMMIT");
}
bool submit(const std::string& author,const std::string& text){
    Db db;
    Query insert(db,"INSERT INTO posts(author,created,body) SELECT ?1,strftime('%Y-%m-%dT%H:%M:%SZ','now'),?2 "
                    "WHERE NOT EXISTS(SELECT 1 FROM blocked WHERE author=?1)");
    insert.bind(1,author);insert.bind(2,text);insert.next();
    return sqlite3_changes(db.handle.get())!=0;
}
Json::Value list(const std::string& query,std::int64_t before){
    Db db;
    Query rows(db,"SELECT p.id,p.author,p.created,p.body,b.author IS NOT NULL FROM posts p "
                  "LEFT JOIN blocked b ON b.author=p.author WHERE p.id<?1 AND "
                  "(instr(p.body,?2)>0 OR instr(p.author,?2)>0) ORDER BY p.id DESC LIMIT 51");
    rows.bind(1,before);rows.bind(2,query);
    Json::Value result;
    auto& items=result["items"];items=Json::arrayValue;
    while(rows.next()){
        if(items.size()==50){result["before"]=items[49]["id"];break;}
        Json::Value row;row["id"]=Json::Int64(rows.number(0));row["author"]=rows.text(1);
        row["created"]=rows.text(2);row["text"]=rows.text(3);row["blocked"]=bool(rows.number(4));
        items.append(std::move(row));
    }
    return result;
}
bool block(std::int64_t post){
    Db db;
    Query insert(db,"INSERT INTO blocked(author) SELECT author FROM posts WHERE id=?1 AND author IS NOT NULL "
                    "ON CONFLICT(author) DO UPDATE SET author=excluded.author");
    insert.bind(1,post);insert.next();
    return sqlite3_changes(db.handle.get())!=0;
}
}
