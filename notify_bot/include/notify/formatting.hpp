#pragma once
#include "notify/models.hpp"
#include "notify/config.hpp"
#include <string_view>
namespace notify { std::vector<Entity> parse_entities(std::string_view text,std::string_view style); std::string entities_json(const std::vector<Entity>&); std::string escape_text(std::string_view text,std::string_view style); std::string plain_with_mentions_html(std::string_view text); void validate_post(const Post&,const BuildConfig&); bool validate_photo(const std::wstring&,std::size_t); std::vector<Subscriber> read_subscribers(const std::wstring& path=L"state/subscribers.jsonl"); }
