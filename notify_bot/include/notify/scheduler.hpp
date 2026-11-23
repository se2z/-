#pragma once
#include "notify/config.hpp"
#include "notify/models.hpp"
#include "notify/send_guard.hpp"
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <semaphore>
#include <stop_token>
#include <thread>
namespace notify {
class RunScope { std::function<void()> flush_; public: explicit RunScope(std::function<void()> f):flush_(std::move(f)){} ~RunScope() noexcept {if(flush_)try{flush_();}catch(...){}} RunScope(const RunScope&)=delete; };
class Scheduler {
 BuildConfig config_; mutable std::mutex mutex_; std::condition_variable_any cv_; std::deque<std::pair<std::string,Post>> queue_; unsigned in_flight_=0; std::chrono::milliseconds chat_delay_{1000}; unsigned failures_=0; AccountState state_=AccountState::RUNNING; std::vector<AdminEvent> events_; SendGuard guard_; std::function<BackendResult(const std::string&,const Post&)> send_;
 void _flush(); void work(std::stop_token);
 void record_outcome(const std::string&,const BackendResult&);
public:
 Scheduler(BuildConfig,std::function<BackendResult(const std::string&,const Post&)>);
 BackendResult enqueue(const Subscriber&,const Post&);
 void set_chat_delay(std::chrono::milliseconds);
 bool idle() const;
 void RunPooled(std::stop_token={}); void RunOverlapped(std::stop_token={});
 void resume(); AccountState state() const;
};
}
