#pragma once
#include "notify/json.hpp"
#include "notify/models.hpp"
#include <string>
namespace notify {
class BuildConfig {
 json::Value root_;
public:
 static BuildConfig load(const std::wstring& path);
 const json::Value* field(std::string_view name) const;
 std::string text(std::string_view name,std::string fallback={}) const;
 long long integer(std::string_view name,long long fallback=0) const;
 State state(std::string_view name) const;
 bool is_set(std::string_view name) const;
 void validate() const;
};
}
