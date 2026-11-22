#pragma once
#include <json/json.h>
#include <cstdint>
#include <string>

namespace feedback {
void initialize();
bool submit(const std::string& author,const std::string& text);
Json::Value list(const std::string& query,std::int64_t before);
bool block(std::int64_t post);
void registerAdmin();
}
