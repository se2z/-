#include "notify/scheduler.hpp"

#include "notify/formatting.hpp"
#include "notify/json.hpp"
#include "notify/winhttp_client.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <stdexcept>
#include <mutex>

namespace notify {
Scheduler::Scheduler(BuildConfig config, std::function<BackendResult(const std::string&, const Post&)> send)
    : config_(std::move(config)), send_(std::move(send)) {
    config_.validate();
}

BackendResult Scheduler::enqueue(const Subscriber& subscriber, const Post& post) {
    std::lock_guard lock(mutex_);
    if (subscriber.consent_ts.empty() || subscriber.source.empty()) {
        events_.push_back({"SKIPPED_NO_CONSENT", subscriber.chat_id, "explicit consent missing"});
        return {std::nullopt, "missing explicit consent", {}};
    }
    try {
        validate_post(post, config_);
        if (!post.image_path.empty() && !validate_photo(std::filesystem::path(post.image_path).wstring(),
                static_cast<std::size_t>(config_.integer("max_photo_bytes")))) {
            return {false, "photo validation failed", {}};
        }
    } catch (const std::exception& error) {
        return {false, error.what(), {}};
    }
    if (state_ != AccountState::RUNNING) return {false, "queue paused; manual resume required", {}};
    queue_.emplace_back(subscriber.chat_id, post);
    cv_.notify_one();
    return {std::nullopt, "queued", {}};
}

void Scheduler::set_chat_delay(std::chrono::milliseconds delay) {
    if (delay < std::chrono::milliseconds::zero() || delay > std::chrono::hours(1))
        throw std::invalid_argument("chat delay must be between 0 and 3600 seconds");
    std::lock_guard lock(mutex_);
    chat_delay_ = delay;
}

bool Scheduler::idle() const {
    std::lock_guard lock(mutex_);
    return queue_.empty() && in_flight_ == 0;
}

void Scheduler::_flush() {
    std::lock_guard lock(mutex_);
    if (events_.empty()) return;
    const auto destination = config_.is_set("event_sink") ? config_.text("event_sink") : "state/admin_events.jsonl";
    const std::filesystem::path path(destination);
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) throw std::runtime_error("admin event log open failed");
    for (const auto& event : events_) {
        json::Value::Object row;
        row["type"].data = event.type;
        row["account_id"].data = event.account_id;
        row["detail"].data = event.detail;
        json::Value value;
        value.data = std::move(row);
        file << json::stringify(value) << '\n';
    }
    file.flush();
    if (!file) throw std::runtime_error("admin event log flush failed");
    events_.clear();
}

namespace {
std::mutex failure_log_mutex;
bool has_current_consent(const std::string& chat_id) {
    try {
        const auto subscribers = read_subscribers();
        return std::any_of(subscribers.begin(), subscribers.end(), [&](const Subscriber& item) {
            return item.chat_id == chat_id && !item.consent_ts.empty() && !item.source.empty();
        });
    } catch (...) {
        return false;
    }
}

void record_failure(const std::string& id, const BackendResult& result) {
    std::lock_guard lock(failure_log_mutex);
    std::filesystem::create_directories("state");
    std::ofstream file("state/failures.jsonl", std::ios::binary | std::ios::app);
    if (!file) throw std::runtime_error("failure log open failed");
    json::Value::Object row;
    row["chat_id"].data = id;
    row["detail"].data = result.detail;
    json::Value value;
    value.data = std::move(row);
    file << json::stringify(value) << '\n';
    if (!file) throw std::runtime_error("failure log write failed");
}

std::chrono::milliseconds retry_delay(const BuildConfig& config, unsigned attempt,
        const BackendResult& result, std::mt19937& rng) {
    if (result.retry_after) return std::chrono::seconds(*result.retry_after);
    const auto initial = (std::min)(60000LL, (std::max)(1LL, config.integer("retry_base_ms", 500)));
    const auto factor = 1LL << (std::min)(attempt, 16U);
    const auto base = (std::min)(60000LL, initial * factor);
    return std::chrono::milliseconds(base + std::uniform_int_distribution<long long>(0, (std::max)(1LL, base / 4))(rng));
}
}

void Scheduler::record_outcome(const std::string& chat_id, const BackendResult& result) {
    if (!result.ok.has_value()) return;
    if (!*result.ok) record_failure(chat_id, result);
    std::lock_guard lock(mutex_);
    if (state_ != AccountState::RUNNING) return;
    if (*result.ok) {
        failures_ = 0;
        return;
    }
    if (++failures_ >= static_cast<unsigned>(config_.integer("failure_threshold", 5))) {
        failures_ = 0;
        state_ = AccountState::PAUSED_RETRY;
        events_.push_back({"PAUSED_RETRY", "queue", "failure threshold reached; manual resume required"});
        cv_.notify_all();
    }
}

void Scheduler::work(std::stop_token stop) {
    std::mt19937 rng(std::random_device{}());
    for (;;) {
        std::pair<std::string, Post> job;
        std::chrono::milliseconds delay{};
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, stop, [&] { return (state_ == AccountState::RUNNING && !queue_.empty()) || stop.stop_requested(); });
            if (stop.stop_requested()) return;
            if (queue_.empty()) continue;
            job = std::move(queue_.front());
            queue_.pop_front();
            ++in_flight_;
            delay = chat_delay_;
        }
        auto completion = std::unique_ptr<Scheduler, std::function<void(Scheduler*)>>(
            this, [](Scheduler* owner) {
                std::lock_guard lock(owner->mutex_);
                if (owner->in_flight_) --owner->in_flight_;
                owner->cv_.notify_all();
            });

        BackendResult result{false, "no attempt", {}};
        for (unsigned attempt = 0; attempt < 5; ++attempt) {
            { std::lock_guard lock(mutex_); if (state_ != AccountState::RUNNING) { result = {std::nullopt, "queue paused", {}}; break; } }
            if (!has_current_consent(job.first)) { result = {std::nullopt, "consent missing or revoked", {}}; break; }
            guard_.wait(job.first, delay, delay);
            try { result = send_(job.first, job.second); }
            catch (const std::exception& error) { result = {false, error.what(), {}}; }
            if (result.ok == true || !result.ok.has_value()) break;
            if (attempt < 4) std::this_thread::sleep_for(retry_delay(config_, attempt, result, rng));
        }
        record_outcome(job.first, result);
    }
}

void Scheduler::RunPooled(std::stop_token stop) {
    RunScope scope([this] { _flush(); });
    const auto workers = static_cast<unsigned>(config_.integer("max_workers", 8));
    std::vector<std::jthread> pool;
    pool.reserve(workers);
    for (unsigned i = 0; i < workers; ++i) pool.emplace_back([this](std::stop_token token) { work(token); });
    while (!stop.stop_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    for (auto& worker : pool) worker.request_stop();
    cv_.notify_all();
}

void Scheduler::RunOverlapped(std::stop_token stop) {
    if (!config_.is_set("api_base")) throw std::runtime_error("api_base unset; WinHTTP workers not started");
    RunScope scope([this] { _flush(); });
    auto client = std::make_shared<WinHttpClient>(L"config\\token.txt", config_.text("api_base"));
    const auto workers = static_cast<unsigned>(config_.integer("max_workers", 8));
    std::counting_semaphore<256> slots(static_cast<std::ptrdiff_t>(workers));
    std::vector<std::jthread> pool;
    pool.reserve(workers);
    for (unsigned i = 0; i < workers; ++i) {
        pool.emplace_back([this, &slots, &client](std::stop_token token) {
            std::mt19937 rng(std::random_device{}());
            while (!token.stop_requested()) {
                std::pair<std::string, Post> job;
                std::chrono::milliseconds delay{};
                {
                    std::unique_lock lock(mutex_);
                    cv_.wait_for(lock, std::chrono::milliseconds(50), [&] {
                        return (state_ == AccountState::RUNNING && !queue_.empty()) || token.stop_requested();
                    });
                    if (token.stop_requested()) return;
                    if (queue_.empty()) continue;
                    job = std::move(queue_.front()); queue_.pop_front(); ++in_flight_; delay = chat_delay_;
                }
                auto completion = std::unique_ptr<Scheduler, std::function<void(Scheduler*)>>(
                    this, [](Scheduler* owner) {
                        std::lock_guard lock(owner->mutex_);
                        if (owner->in_flight_) --owner->in_flight_;
                        owner->cv_.notify_all();
                    });
                BackendResult result{false, "no attempt", {}};
                for (unsigned attempt = 0; attempt < 5; ++attempt) {
                    { std::lock_guard lock(mutex_); if (state_ != AccountState::RUNNING) { result = {std::nullopt, "queue paused", {}}; break; } }
                    if (!has_current_consent(job.first)) { result = {std::nullopt, "consent missing or revoked", {}}; break; }
                    slots.acquire();
                    guard_.wait(job.first, delay, delay);
                    try {
                        result = job.second.image_path.empty()
                            ? client->send_message_async(job.first, job.second)
                            : client->send_photo(job.first, job.second, static_cast<std::size_t>(config_.integer("max_photo_bytes")));
                    } catch (const std::exception& error) { result = {false, error.what(), {}}; }
                    slots.release();
                    if (result.ok == true || !result.ok.has_value()) break;
                    if (attempt < 4) std::this_thread::sleep_for(retry_delay(config_, attempt, result, rng));
                }
                record_outcome(job.first, result);
            }
        });
    }
    while (!stop.stop_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(25));
    for (auto& worker : pool) worker.request_stop();
    cv_.notify_all();
}

void Scheduler::resume() { std::lock_guard lock(mutex_); state_ = AccountState::RUNNING; failures_ = 0; cv_.notify_all(); }
AccountState Scheduler::state() const { std::lock_guard lock(mutex_); return state_; }
}
