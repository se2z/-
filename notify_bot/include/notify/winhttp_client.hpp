#pragma once
#include "notify/models.hpp"
#include <string>
namespace notify { class WinHttpClient { std::wstring token_; public: explicit WinHttpClient(std::wstring token,std::string api_base); BackendResult get_me(); BackendResult send_message(const std::string&,const Post&); BackendResult send_photo(const std::string&,const Post&,std::size_t); BackendResult send_message_async(const std::string&,const Post&); }; }
