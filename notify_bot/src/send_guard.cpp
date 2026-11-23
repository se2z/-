#include "notify/send_guard.hpp"
#include <thread>
#include <algorithm>
namespace notify {void SendGuard::wait(const std::string&id,std::chrono::milliseconds per,std::chrono::milliseconds global){std::chrono::steady_clock::time_point due;{std::lock_guard l(mutex_);auto now=std::chrono::steady_clock::now();due=(std::max)(now,(std::max)(recipient_[id]+per,global_+global));recipient_[id]=due;global_=due;}std::this_thread::sleep_until(due);}}
