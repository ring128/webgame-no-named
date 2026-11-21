#include <drogon/drogon.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

using namespace std::chrono;
namespace {
// 仮のバランス値はここで調整する。
constexpr int limitMs = 24000, stepMs = 1250, barrierMs = 4500, cooldownMs = 1200;
constexpr std::array<const char *, 7> map = {
    ".....#.....", "...........", ".....#.....", ".....#.....",
    ".....#.....", "...........", ".....#....."};
constexpr int width = 11, height = 7;
struct Point { int x, y; bool operator==(const Point &) const = default; };
constexpr Point start{1, 3}, goal{9, 3};
using Clock = steady_clock;

class Game {
    std::string status_ = "ready";
    Point npc_ = start;
    Clock::time_point deadline_{}, nextStep_{}, nextBlock_{};
    std::array<std::array<Clock::time_point, width>, height> barriers_{};
    static bool inside(int x, int y) { return x >= 0 && x < width && y >= 0 && y < height; }
    static Json::Value point(Point p) {
        Json::Value v(Json::objectValue); v["x"] = p.x; v["y"] = p.y; return v;
    }
    // 4方向の A*。返す経路に現在地は含めない。
    std::vector<Point> path(Clock::time_point at) const {
        struct Node { int f, g, x, y; };
        auto greater = [](Node a, Node b) { return a.f > b.f; };
        std::priority_queue<Node, std::vector<Node>, decltype(greater)> open(greater);
        std::array<std::array<int, width>, height> cost;
        std::array<std::array<Point, width>, height> previous{};
        for (auto &row : cost) row.fill(1000000);
        cost[npc_.y][npc_.x] = 0;
        open.push({0, 0, npc_.x, npc_.y});
        constexpr std::array<Point, 4> directions{{{0,-1}, {1,0}, {0,1}, {-1,0}}};
        while (!open.empty()) {
            auto n = open.top(); open.pop();
            if (n.g != cost[n.y][n.x]) continue;
            if (Point{n.x, n.y} == goal) {
                std::vector<Point> result;
                for (Point p = goal; !(p == npc_); p = previous[p.y][p.x]) result.push_back(p);
                std::reverse(result.begin(), result.end());
                return result;
            }
            for (auto d : directions) {
                int x = n.x + d.x, y = n.y + d.y;
                if (!inside(x,y) || map[y][x] == '#' || barriers_[y][x] > at || n.g + 1 >= cost[y][x]) continue;
                cost[y][x] = n.g + 1;
                previous[y][x] = {n.x,n.y};
                open.push({n.g + 1 + std::abs(goal.x-x) + std::abs(goal.y-y), n.g + 1, x, y});
            }
        }
        return {};
    }
    void advance() {
        if (status_ != "running") return;
        auto now = Clock::now();
        while (nextStep_ <= now && nextStep_ <= deadline_) {
            auto route = path(nextStep_);
            if (!route.empty()) npc_ = route.front();
            if (npc_ == goal) { status_ = "lost"; return; }
            nextStep_ += milliseconds(stepMs);
        }
        if (now >= deadline_) status_ = "clear";
    }
public:
    void begin() {
        npc_ = start;
        for (auto &row : barriers_) row.fill(Clock::time_point{});
        auto now = Clock::now();
        deadline_ = now + milliseconds(limitMs);
        nextStep_ = now + milliseconds(stepMs);
        nextBlock_ = now;
        status_ = "running";
    }
    std::string block(int x, int y) {
        advance();
        if (status_ != "running") return "ゲームを開始してください";
        if (!inside(x,y) || map[y][x] == '#' || Point{x,y} == npc_ || Point{x,y} == start || Point{x,y} == goal)
            return "そのマスには置けません";
        auto now = Clock::now();
        if (now < nextBlock_) return "能力のクールダウン中です";
        barriers_[y][x] = now + milliseconds(barrierMs); // 同じマスを再クリックすると延長
        nextBlock_ = now + milliseconds(cooldownMs);
        return {};
    }
    Json::Value state() {
        advance();
        auto now = Clock::now();
        Json::Value v(Json::objectValue), rows(Json::arrayValue), blocks(Json::arrayValue), route(Json::arrayValue);
        v["status"] = status_; v["width"] = width; v["height"] = height;
        v["timeLimitMs"] = limitMs; v["moveIntervalMs"] = stepMs;
        v["blockDurationMs"] = barrierMs; v["cooldownMs"] = cooldownMs;
        v["remainingMs"] = status_ == "ready" ? limitMs :
            std::max(0, static_cast<int>(duration_cast<milliseconds>(deadline_ - now).count()));
        v["cooldownRemainingMs"] = status_ == "running" ?
            std::max(0, static_cast<int>(duration_cast<milliseconds>(nextBlock_ - now).count())) : 0;
        for (auto row : map) rows.append(row);
        for (int y=0; y<height; ++y) for (int x=0; x<width; ++x)
            if (barriers_[y][x] > now) {
                auto b = point({x,y});
                b["remainingMs"] = static_cast<int>(duration_cast<milliseconds>(barriers_[y][x]-now).count());
                blocks.append(b);
            }
        for (auto p : path(now)) route.append(point(p));
        v["map"] = rows; v["start"] = point(start); v["goal"] = point(goal);
        v["npc"] = point(npc_); v["blocks"] = blocks; v["path"] = route;
        return v;
    }
};
std::mutex mutex;
Game game;
void reply(const std::function<void(const drogon::HttpResponsePtr &)> &callback,
           drogon::HttpStatusCode code = drogon::k200OK, const std::string &error = {}) {
    auto state = game.state();
    if (!error.empty()) state["error"] = error;
    auto response = drogon::HttpResponse::newHttpJsonResponse(state);
    response->setStatusCode(code);
    callback(response);
}
}

int main() {
    using namespace drogon;
    app().registerHandler("/api/state", [](const HttpRequestPtr &,
        std::function<void(const HttpResponsePtr &)> &&callback) {
        std::lock_guard lock(mutex); reply(callback);
    }, {Get});
    app().registerHandler("/api/start", [](const HttpRequestPtr &,
        std::function<void(const HttpResponsePtr &)> &&callback) {
        std::lock_guard lock(mutex); game.begin(); reply(callback);
    }, {Post});
    app().registerHandler("/api/block", [](const HttpRequestPtr &request,
        std::function<void(const HttpResponsePtr &)> &&callback) {
        std::lock_guard lock(mutex);
        auto json = request->getJsonObject();
        if (!json || !(*json)["x"].isInt() || !(*json)["y"].isInt()) {
            reply(callback, k400BadRequest, "x と y に整数を指定してください"); return;
        }
        auto error = game.block((*json)["x"].asInt(), (*json)["y"].asInt());
        reply(callback, error.empty() ? k200OK : k409Conflict, error);
    }, {Post});
    app().addListener("127.0.0.1", 8848).run();
}
