#pragma once
#include <map>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
namespace notify::json {
struct Value {
 using Array=std::vector<Value>; using Object=std::map<std::string,Value,std::less<>>;
 std::variant<std::nullptr_t,bool,std::int64_t,double,std::string,Array,Object> data=nullptr;
 const Value* find(std::string_view key) const;
 std::string string_or(std::string fallback={}) const;
 double number_or(double fallback=0) const;
 bool is_null() const;
};
Value parse(std::string_view input,std::size_t max_bytes=4*1024*1024,std::size_t max_depth=64);
std::string stringify(const Value& value);
bool valid_utf8(std::string_view value);
}
