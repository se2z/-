#include "notify/json.hpp"
#include "notify/formatting.hpp"
#include "notify/license.hpp"
#include "notify/scheduler.hpp"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace notify::tests {
void require(bool b){if(!b)throw std::runtime_error("pure test failed");}
void license_hwid_lifecycle(){
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();auto root=std::filesystem::temp_directory_path()/("notify_bot_license_"+std::to_string(unique));std::filesystem::create_directories(root);auto private_key=(root/"private.bin").string();auto public_key=(root/"public.bin").string();auto other_private=(root/"other_private.bin").string();auto other_public=(root/"other_public.bin").string();auto revoked=(root/"revoked.jsonl").string();
    try{require(new_secret(private_key,public_key));require(new_secret(other_private,other_public));auto original=issue_for_hwid("premium",30,"TEST-HWID-A",private_key);require(verify_for_hwid(original,"TEST-HWID-A",public_key,revoked));require(!verify_for_hwid(original,"TEST-HWID-A",other_public,revoked));require(!verify_for_hwid(original,"TEST-HWID-B",public_key,revoked));auto replacement=reissue_for_hwid(original,"premium",30,"TEST-HWID-B",private_key,revoked,public_key);require(!verify_for_hwid(original,"TEST-HWID-A",public_key,revoked));require(verify_for_hwid(replacement,"TEST-HWID-B",public_key,revoked));require(revoke_any(replacement,public_key,revoked));require(!verify_for_hwid(replacement,"TEST-HWID-B",public_key,revoked));}catch(...){std::filesystem::remove(private_key);std::filesystem::remove(public_key);std::filesystem::remove(other_private);std::filesystem::remove(other_public);std::filesystem::remove(revoked);std::filesystem::remove_all(root);throw;}
    std::filesystem::remove(private_key);std::filesystem::remove(public_key);std::filesystem::remove(other_private);std::filesystem::remove(other_public);std::filesystem::remove(revoked);std::filesystem::remove_all(root);
}
void license_activation_lifecycle(){
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();auto root=std::filesystem::temp_directory_path()/ ("notify_bot_activation_"+std::to_string(unique));std::filesystem::create_directories(root);auto private_key=(root/"private.bin").string();auto public_key=(root/"public.bin").string();auto revoked=(root/"revoked.jsonl").string();auto activation=(root/"activation.json").string();
    try{require(new_secret(private_key,public_key));auto first=issue_unbound("premium",30,private_key);require(!verify_for_hwid(first,"TEST-HWID-A",public_key,revoked));require(activate_for_hwid(first,"TEST-HWID-A",activation,public_key,revoked));require(activate_for_hwid(first,"TEST-HWID-A",activation,public_key,revoked));require(verify_activation_for_hwid(first,"TEST-HWID-A",activation,public_key,revoked));require(!verify_activation_for_hwid(first,"TEST-HWID-B",activation,public_key,revoked));require(!activate_for_hwid(first,"TEST-HWID-B",activation,public_key,revoked));auto second=issue_unbound("basic",30,private_key);require(!activate_for_hwid(second,"TEST-HWID-A",activation,public_key,revoked));require(revoke_any(first,public_key,revoked));require(!verify_activation_for_hwid(first,"TEST-HWID-A",activation,public_key,revoked));}catch(...){std::filesystem::remove(private_key);std::filesystem::remove(public_key);std::filesystem::remove(revoked);std::filesystem::remove(activation);std::filesystem::remove_all(root);throw;}
    std::filesystem::remove(private_key);std::filesystem::remove(public_key);std::filesystem::remove(revoked);std::filesystem::remove(activation);std::filesystem::remove_all(root);
}
void scheduler_dispatch_mock(){
    const auto config=BuildConfig::load(L"config/build_config.json");
    const auto original=std::filesystem::current_path();
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto sandbox=std::filesystem::temp_directory_path()/("notify_bot_dispatch_"+std::to_string(unique));
    std::filesystem::create_directories(sandbox);std::mutex mutex;std::condition_variable cv;bool delivered=false;std::string seen_chat,seen_text;
    try{
        std::filesystem::current_path(sandbox);
        std::filesystem::create_directories("state");
        {std::ofstream consent("state/subscribers.jsonl",std::ios::binary|std::ios::trunc);consent<<"{\"chat_id\":\"test-chat\",\"consent_ts\":\"2026-11-22T00:00:00Z\",\"source\":\"test consent\"}\n";require(static_cast<bool>(consent));}
        Scheduler scheduler(config,[&](const std::string& chat,const Post& post){std::lock_guard lock(mutex);seen_chat=chat;seen_text=post.text;delivered=true;cv.notify_one();return BackendResult{true,"mock accepted",{}};});scheduler.set_chat_delay(std::chrono::milliseconds(0));
        Post post;post.text="local dispatch check";Subscriber subscriber{"test-chat","2026-11-22T00:00:00Z","test consent"};
        const auto queued=scheduler.enqueue(subscriber,post);require(!queued.ok.has_value()&&queued.detail=="queued");require(!scheduler.idle());
        std::jthread worker([&](std::stop_token stop){scheduler.RunPooled(stop);});
        bool observed=false;{std::unique_lock lock(mutex);observed=cv.wait_for(lock,std::chrono::seconds(5),[&]{return delivered;});}
        worker.request_stop();worker.join();require(observed);
        require(seen_chat=="test-chat"&&seen_text=="local dispatch check");
        require(scheduler.state()==AccountState::RUNNING&&scheduler.idle());
    }catch(...){std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);throw;}
    std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);
}
void scheduler_threshold_pause_and_flush(){
    const auto config=BuildConfig::load(L"config/build_config.json");
    const auto original=std::filesystem::current_path();
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto sandbox=std::filesystem::temp_directory_path()/("notify_bot_threshold_"+std::to_string(unique));
    std::filesystem::create_directories(sandbox);
    try{
        std::filesystem::current_path(sandbox);
        std::filesystem::create_directories("state");
        {std::ofstream consent("state/subscribers.jsonl",std::ios::binary|std::ios::trunc);consent<<"{\"chat_id\":\"test-chat\",\"consent_ts\":\"2026-11-22T00:00:00Z\",\"source\":\"test consent\"}\n";require(static_cast<bool>(consent));}
        Scheduler skipped(config,[](const std::string&,const Post&){return BackendResult{false,"must not send",0};});
        Post post;post.text="failure threshold test";
        Subscriber no_consent{"unconsented","",""};
        require(!skipped.enqueue(no_consent,post).ok.has_value());
        require(skipped.state()==AccountState::RUNNING);
        Scheduler scheduler(config,[](const std::string&,const Post&){return BackendResult{false,"mock refusal",0};});
        scheduler.set_chat_delay(std::chrono::milliseconds(0));
        post.text="failure threshold test";
        Subscriber subscriber{"test-chat","2026-11-22T00:00:00Z","test consent"};
        for(unsigned i=0;i<5;++i)require(scheduler.enqueue(subscriber,post).detail=="queued");
        std::jthread worker([&](std::stop_token stop){scheduler.RunPooled(stop);});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        while(scheduler.state()!=AccountState::PAUSED_RETRY&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const bool paused=scheduler.state()==AccountState::PAUSED_RETRY;
        worker.request_stop();worker.join();
        require(paused);
        std::ifstream events("state/admin_events.jsonl",std::ios::binary);
        std::string row;require(static_cast<bool>(std::getline(events,row)));
        const auto event=json::parse(row,65536,16);
        require(event.find("type")&&event.find("type")->string_or()=="PAUSED_RETRY");
        require(!std::getline(events,row));
        scheduler.resume();
        require(scheduler.state()==AccountState::RUNNING);
    }catch(...){std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);throw;}
    std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);
}
void scheduler_null_outcome_does_not_pause(){
    const auto config=BuildConfig::load(L"config/build_config.json");
    const auto original=std::filesystem::current_path();
    const auto unique=std::chrono::steady_clock::now().time_since_epoch().count();
    const auto sandbox=std::filesystem::temp_directory_path()/("notify_bot_null_result_"+std::to_string(unique));
    std::filesystem::create_directories(sandbox);
    try{
        std::filesystem::current_path(sandbox);
        std::filesystem::create_directories("state");
        {std::ofstream consent("state/subscribers.jsonl",std::ios::binary|std::ios::trunc);consent<<"{\"chat_id\":\"test-chat\",\"consent_ts\":\"2026-11-22T00:00:00Z\",\"source\":\"test consent\"}\n";require(static_cast<bool>(consent));}
        Scheduler scheduler(config,[](const std::string&,const Post&){return BackendResult{std::nullopt,"dry run",{}, {},true};});
        scheduler.set_chat_delay(std::chrono::milliseconds(0));
        Post post;post.text="dry run test";Subscriber subscriber{"test-chat","2026-11-22T00:00:00Z","test consent"};
        for(unsigned i=0;i<5;++i)require(scheduler.enqueue(subscriber,post).detail=="queued");
        std::jthread worker([&](std::stop_token stop){scheduler.RunPooled(stop);});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!scheduler.idle()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const bool drained=scheduler.idle();worker.request_stop();worker.join();
        require(drained&&scheduler.state()==AccountState::RUNNING);
        require(!std::filesystem::exists("state/admin_events.jsonl"));
        require(!std::filesystem::exists("state/failures.jsonl"));
    }catch(...){std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);throw;}
    std::filesystem::current_path(original);std::filesystem::remove_all(sandbox);
}
void send_guard_delay(){SendGuard guard;const auto start=std::chrono::steady_clock::now();guard.wait("first",std::chrono::milliseconds(0),std::chrono::milliseconds(0));guard.wait("second",std::chrono::milliseconds(0),std::chrono::milliseconds(60));const auto elapsed=std::chrono::steady_clock::now()-start;require(elapsed>=std::chrono::milliseconds(45));}
void run(){auto j=json::parse("{\"a\":[1,true,null],\"z\":0}");require(j.find("a")!=nullptr);require(j.find("z")->number_or(-1)==0);auto exact=json::parse("{\"chat_id\":-1001234567890123,\"escaped\":\"\\u0410\"}");require(std::holds_alternative<std::int64_t>(exact.find("chat_id")->data));require(std::get<std::int64_t>(exact.find("chat_id")->data)==-1001234567890123LL);require(exact.find("escaped")->string_or()=="\xD0\x90");bool rejected=false;try{json::parse("[1,");}catch(...){rejected=true;}require(rejected);require(json::valid_utf8("ok"));require(!json::valid_utf8("\xC0\x80"));require(escape_text("<x>","html")=="&lt;x&gt;");require(parse_entities("@samplebot tg://user?id=42","plain").size()==2);const auto mentioned=plain_with_mentions_html("Hi @samplebot <b> & tg://user?id=42");require(mentioned.find("https://t.me/samplebot")!=std::string::npos);require(mentioned.find("tg://user?id=42")!=std::string::npos);require(mentioned.find("&lt;b&gt; &amp;")!=std::string::npos);Post p;p.entity_unit_state=State::unset;require(p.entity_unit_state==State::unset);auto config=BuildConfig::load(L"config/build_config.json");require(config.state("api_base")==State::unset&&!config.is_set("api_base"));require(config.field("api_base")&&config.field("api_base")->find("value")->is_null());require(config.integer("card_count")==5);require(config.state("phone_input_state")==State::unset&&!config.is_set("phone_input_state"));require(config.state("code_input_state")==State::unset&&!config.is_set("code_input_state"));require(config.state("connect_action")==State::unset&&!config.is_set("connect_action"));license_hwid_lifecycle();license_activation_lifecycle();send_guard_delay();scheduler_dispatch_mock();scheduler_threshold_pause_and_flush();scheduler_null_outcome_does_not_pause();}
}

