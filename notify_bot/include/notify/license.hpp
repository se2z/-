#pragma once
#include <string>
#include <string_view>
namespace notify {
std::string issue(std::string_view plan,unsigned days,std::string_view private_key_path="config/license_private_key.bin");
std::string issue_unbound(std::string_view plan,unsigned days,std::string_view private_key_path="config/license_private_key.bin");
std::string issue_for_hwid(std::string_view plan,unsigned days,std::string_view hwid,std::string_view private_key_path="config/license_private_key.bin");
bool verify(std::string_view token);
bool verify_issued(std::string_view token);
bool verify_for_hwid(std::string_view token,std::string_view hwid,std::string_view public_key_path="config/license_public_key.bin",std::string_view revoked_path="state/revoked.jsonl");
bool activate_for_hwid(std::string_view token,std::string_view hwid,std::string_view activation_path="config/license_activation.json",std::string_view public_key_path="config/license_public_key.bin",std::string_view revoked_path="state/revoked.jsonl");
bool verify_activation_for_hwid(std::string_view token,std::string_view hwid,std::string_view activation_path="config/license_activation.json",std::string_view public_key_path="config/license_public_key.bin",std::string_view revoked_path="state/revoked.jsonl");
bool verify_active_license(std::string_view activation_path="config/license_activation.json",std::string_view public_key_path="config/license_public_key.bin",std::string_view revoked_path="state/revoked.jsonl");
bool revoke(std::string_view token);
bool revoke_any(std::string_view token,std::string_view public_key_path="config/license_public_key.bin",std::string_view revoked_path="state/revoked.jsonl");
std::string reissue_for_hwid(std::string_view old_token,std::string_view plan,unsigned days,std::string_view new_hwid,std::string_view private_key_path="config/license_private_key.bin",std::string_view revoked_path="state/revoked.jsonl",std::string_view public_key_path="config/license_public_key.bin");
std::string reissue_unbound(std::string_view old_token,std::string_view plan,unsigned days,std::string_view private_key_path="config/license_private_key.bin",std::string_view revoked_path="state/revoked.jsonl",std::string_view public_key_path="config/license_public_key.bin");
bool new_secret(std::string_view private_key_path="config/license_private_key.bin",std::string_view public_key_path="config/license_public_key.bin");
}
