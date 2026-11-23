#pragma once
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
namespace notify { class SendGuard { std::mutex mutex_; std::unordered_map<std::string,std::chrono::steady_clock::time_point> recipient_; std::chrono::steady_clock::time_point global_{}; public: void wait(const std::string&,std::chrono::milliseconds,std::chrono::milliseconds); }; }
