#pragma once
#include <optional>
#include <string>
#include <vector>
namespace notify {
enum class State { unset, configured, derived, measured, provided, invalid };
enum class AccountState { RUNNING, PAUSED_RETRY };
enum class EntityType { mention, text_mention, bold, italic, code, url, custom_emoji };
struct Entity { EntityType type{}; std::size_t offset{},length{}; std::string value; };
struct Post { std::string text; std::vector<Entity> entities; std::string image_path; std::string style="plain"; State entity_unit_state=State::unset; std::string entity_unit; };
struct BackendResult { std::optional<bool> ok; std::string detail; std::optional<unsigned> retry_after; std::string account_id; bool dry_run=false; std::string meta; };
struct AdminEvent { std::string type,account_id,detail; };
struct Subscriber { std::string chat_id,consent_ts,source; };
}
