#include "notify/license.hpp"
#include "notify/hwid.hpp"
#include "notify/json.hpp"
#include <windows.h>
#include <bcrypt.h>
#include <fstream>
#include <vector>
#include <array>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <stdexcept>
#include <limits>
#include <optional>

namespace notify {
namespace {
constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string base64url(const unsigned char* bytes,size_t size){
    std::string output;
    for(size_t i=0;i<size;i+=3){unsigned value=(unsigned)bytes[i]<<16;if(i+1<size)value|=(unsigned)bytes[i+1]<<8;if(i+2<size)value|=bytes[i+2];output+=alphabet[(value>>18)&63];output+=alphabet[(value>>12)&63];if(i+1<size)output+=alphabet[(value>>6)&63];if(i+2<size)output+=alphabet[value&63];}
    return output;
}

std::vector<unsigned char> unbase64url(std::string_view text){
    if(text.size()>65536)throw std::runtime_error("license token too large");
    std::vector<unsigned char> output;unsigned accumulator=0,bits=0;
    for(char c:text){const char* item=std::find(std::begin(alphabet),std::end(alphabet)-1,c);if(item==std::end(alphabet)-1)throw std::runtime_error("bad base64url");accumulator=(accumulator<<6)|(unsigned)(item-alphabet);bits+=6;if(bits>=8){bits-=8;output.push_back((unsigned char)((accumulator>>bits)&255));}}
    if(bits>=6)throw std::runtime_error("bad base64url length");return output;
}

std::vector<unsigned char> read_file(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("key file missing");std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});if(file.bad()||bytes.empty()||bytes.size()>65536)throw std::runtime_error("key file read failed");return bytes;
}

bool write_exclusive(const std::filesystem::path& path,const std::vector<unsigned char>& bytes){
    const auto parent=path.parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;const BOOL ok=WriteFile(file,bytes.data(),(DWORD)bytes.size(),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);CloseHandle(file);if(!ok)DeleteFileW(path.c_str());return ok!=FALSE;
}

std::vector<unsigned char> sha256(std::string_view message){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE hash{};DWORD object_size=0,returned=0;
    NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);if(status<0)throw std::runtime_error("CNG SHA-256 provider failed");
    status=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,(PUCHAR)&object_size,sizeof object_size,&returned,0);std::vector<unsigned char> object(object_size),digest(32);
    if(status>=0)status=BCryptCreateHash(algorithm,&hash,object.data(),object_size,nullptr,0,0);
    if(status>=0)status=BCryptHashData(hash,(PUCHAR)message.data(),(ULONG)message.size(),0);
    if(status>=0)status=BCryptFinishHash(hash,digest.data(),(ULONG)digest.size(),0);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("CNG SHA-256 failed");return digest;
}

std::vector<unsigned char> sign_payload(std::string_view private_path,std::string_view payload){
    auto blob=read_file(std::filesystem::path(private_path));BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE key{};NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_RSA_ALGORITHM,nullptr,0);
    if(status>=0)status=BCryptImportKeyPair(algorithm,nullptr,BCRYPT_RSAPRIVATE_BLOB,&key,blob.data(),(ULONG)blob.size(),0);
    if(status<0){if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);throw std::runtime_error("RSA private key import failed");}
    auto digest=sha256(payload);BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};ULONG needed=0;
    status=BCryptSignHash(key,&padding,digest.data(),(ULONG)digest.size(),nullptr,0,&needed,BCRYPT_PAD_PKCS1);std::vector<unsigned char> signature(needed);
    if(status>=0)status=BCryptSignHash(key,&padding,digest.data(),(ULONG)digest.size(),signature.data(),(ULONG)signature.size(),&needed,BCRYPT_PAD_PKCS1);
    BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0||needed==0)throw std::runtime_error("RSA license signing failed");signature.resize(needed);return signature;
}

bool verify_signature(std::string_view public_path,std::string_view payload,const std::vector<unsigned char>& signature){
    if(signature.size()<128||signature.size()>512)return false;std::vector<unsigned char> blob;HRSRC resource=FindResourceW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(207),RT_RCDATA);
    if(resource){HGLOBAL loaded=LoadResource(GetModuleHandleW(nullptr),resource);const DWORD size=SizeofResource(GetModuleHandleW(nullptr),resource);const auto* bytes=loaded?static_cast<const unsigned char*>(LockResource(loaded)):nullptr;if(!bytes||size==0||size>65536)return false;blob.assign(bytes,bytes+size);}
    else blob=read_file(std::filesystem::path(public_path));
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE key{};NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_RSA_ALGORITHM,nullptr,0);
    if(status>=0)status=BCryptImportKeyPair(algorithm,nullptr,BCRYPT_RSAPUBLIC_BLOB,&key,blob.data(),(ULONG)blob.size(),0);
    if(status<0){if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);return false;}auto digest=sha256(payload);BCRYPT_PKCS1_PADDING_INFO padding{BCRYPT_SHA256_ALGORITHM};status=BCryptVerifySignature(key,&padding,digest.data(),(ULONG)digest.size(),const_cast<PUCHAR>(signature.data()),(ULONG)signature.size(),BCRYPT_PAD_PKCS1);BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(algorithm,0);return status>=0;
}

std::string current_hwid(){
    auto value=read_hwid();if(!value)throw std::runtime_error("MachineGuid unavailable");int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value->c_str(),-1,nullptr,0,nullptr,nullptr);if(length<=1)throw std::runtime_error("MachineGuid UTF-8 conversion failed");std::string result((size_t)length,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value->c_str(),-1,result.data(),length,nullptr,nullptr))throw std::runtime_error("MachineGuid UTF-8 conversion failed");result.resize((size_t)length-1);return result;
}

std::string make_payload(std::string_view plan,unsigned days,std::string_view hwid){
    if(plan.empty()||plan.size()>40||!json::valid_utf8(plan)||days==0||days>3650)throw std::runtime_error("invalid plan or duration");
    if(hwid.size()>256||!json::valid_utf8(hwid))throw std::runtime_error("invalid target HWID");
    for(unsigned char c:hwid)if(c<0x20||c==0x7f)throw std::runtime_error("invalid target HWID");
    auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if(now<0||days>(std::numeric_limits<long long>::max()-now)/86400)throw std::runtime_error("license expiry overflow");
    json::Value::Object payload;payload["days"].data=(std::int64_t)days;payload["expires"].data=(std::int64_t)(now+(long long)days*86400);payload["hwid"].data=std::string(hwid);payload["plan"].data=std::string(plan);json::Value value;value.data=std::move(payload);return json::stringify(value);
}

struct TokenParts{std::string payload;std::vector<unsigned char> signature;};
TokenParts decode_token(std::string_view token){const size_t dot=token.find('.');if(dot==token.npos||dot==0||token.find('.',dot+1)!=token.npos)throw std::runtime_error("invalid token format");auto payload_bytes=unbase64url(token.substr(0,dot));TokenParts result;result.payload.assign(payload_bytes.begin(),payload_bytes.end());result.signature=unbase64url(token.substr(dot+1));if(result.signature.size()<128||result.signature.size()>512)throw std::runtime_error("invalid signature length");return result;}
void require_signature(std::string_view token,std::string_view public_path){auto parts=decode_token(token);if(!verify_signature(public_path,parts.payload,parts.signature))throw std::runtime_error("invalid license signature");}

bool is_revoked(std::string_view token,std::string_view revoked_path){
    std::ifstream file(std::filesystem::path(revoked_path),std::ios::binary);if(!file)return false;std::string line;size_t bytes=0;
    while(std::getline(file,line)){bytes+=line.size();if(bytes>16*1024*1024)throw std::runtime_error("revocation file limit");if(line.size()>65536)throw std::runtime_error("revocation record limit");if(line.find(token)!=std::string::npos)return true;}
    if(!file.eof())throw std::runtime_error("partial revocation file read");return false;
}

bool verify_for_hwid_unchecked(std::string_view token,std::string_view hwid,std::string_view public_path,std::string_view revoked_path){
    try{require_signature(token,public_path);if(is_revoked(token,revoked_path))return false;auto parts=decode_token(token);auto value=json::parse(parts.payload,4096,8);auto id=value.find("hwid"),expiry=value.find("expires"),days=value.find("days"),plan=value.find("plan");if(!id||!expiry||!days||!plan||id->string_or()!=hwid||plan->string_or().empty())return false;auto expiry_number=std::get_if<std::int64_t>(&expiry->data);auto day_number=std::get_if<std::int64_t>(&days->data);if(!expiry_number||!day_number||*expiry_number<0||*day_number<1||*day_number>3650)return false;auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();return *expiry_number>=now;}catch(...){return false;}
}

bool unbound_token_is_valid(std::string_view token,std::string_view public_path,std::string_view revoked_path){
    try{require_signature(token,public_path);if(is_revoked(token,revoked_path))return false;auto parts=decode_token(token);auto value=json::parse(parts.payload,4096,8);auto id=value.find("hwid"),expiry=value.find("expires"),days=value.find("days"),plan=value.find("plan");if(!id||!id->string_or().empty()||!expiry||!days||!plan||plan->string_or().empty())return false;auto expiry_number=std::get_if<std::int64_t>(&expiry->data);auto day_number=std::get_if<std::int64_t>(&days->data);if(!expiry_number||!day_number||*expiry_number<0||*day_number<1||*day_number>3650)return false;auto now=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();return *expiry_number>=now;}catch(...){return false;}
}

std::optional<std::pair<std::string,std::string>> read_activation(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)return std::nullopt;std::string contents((std::istreambuf_iterator<char>(file)),{});if(file.bad()||contents.size()>65536)throw std::runtime_error("activation record read failed");auto value=json::parse(contents,65536,8);auto token=value.find("token"),hwid=value.find("hwid");if(!token||!hwid||token->string_or().empty()||hwid->string_or().empty())throw std::runtime_error("invalid activation record");return std::pair<std::string,std::string>{token->string_or(),hwid->string_or()};
}

bool save_activation_exclusive(const std::filesystem::path& path,std::string_view token,std::string_view hwid){
    json::Value::Object record;record["hwid"].data=std::string(hwid);record["token"].data=std::string(token);json::Value value;value.data=std::move(record);const auto contents=json::stringify(value)+"\n";
    auto parent=path.parent_path();if(!parent.empty())std::filesystem::create_directories(parent);
    const auto temporary=path.wstring()+L".pending."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetTickCount64());HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)throw std::runtime_error("cannot create activation record");DWORD written=0;const BOOL ok=WriteFile(file,contents.data(),(DWORD)contents.size(),&written,nullptr)&&written==contents.size()&&FlushFileBuffers(file);CloseHandle(file);if(!ok){DeleteFileW(temporary.c_str());throw std::runtime_error("activation record write failed");}
    if(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH))return true;const DWORD error=GetLastError();DeleteFileW(temporary.c_str());if(error==ERROR_ALREADY_EXISTS||error==ERROR_FILE_EXISTS)return false;throw std::runtime_error("cannot commit activation record");
}
}

bool new_secret(std::string_view private_key_path,std::string_view public_key_path){
    const std::filesystem::path private_path(private_key_path),public_path(public_key_path);if(std::filesystem::exists(private_path)||std::filesystem::exists(public_path))return false;
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_KEY_HANDLE key{};NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_RSA_ALGORITHM,nullptr,0);if(status>=0)status=BCryptGenerateKeyPair(algorithm,&key,2048,0);if(status>=0)status=BCryptFinalizeKeyPair(key,0);if(status<0){if(key)BCryptDestroyKey(key);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);return false;}
    std::vector<unsigned char> private_blob,public_blob;for(auto [type,target]:{std::pair<LPCWSTR,std::vector<unsigned char>*>{BCRYPT_RSAPRIVATE_BLOB,&private_blob},{BCRYPT_RSAPUBLIC_BLOB,&public_blob}}){ULONG size=0;status=BCryptExportKey(key,nullptr,type,nullptr,0,&size,0);if(status<0||size==0||size>65536)break;target->resize(size);status=BCryptExportKey(key,nullptr,type,target->data(),size,&size,0);if(status<0)break;target->resize(size);}
    BCryptDestroyKey(key);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0||private_blob.empty()||public_blob.empty())return false;
    if(!write_exclusive(private_path,private_blob))return false;if(!write_exclusive(public_path,public_blob)){DeleteFileW(private_path.c_str());return false;}return true;
}

std::string issue_for_hwid(std::string_view plan,unsigned days,std::string_view hwid,std::string_view private_path){auto payload=make_payload(plan,days,hwid);auto signature=sign_payload(private_path,payload);return base64url((const unsigned char*)payload.data(),payload.size())+"."+base64url(signature.data(),signature.size());}
std::string issue(std::string_view plan,unsigned days,std::string_view private_path){return issue_unbound(plan,days,private_path);}
std::string issue_unbound(std::string_view plan,unsigned days,std::string_view private_path){auto payload=make_payload(plan,days,{});auto signature=sign_payload(private_path,payload);return base64url((const unsigned char*)payload.data(),payload.size())+"."+base64url(signature.data(),signature.size());}
bool verify_for_hwid(std::string_view token,std::string_view hwid,std::string_view public_path,std::string_view revoked_path){return verify_for_hwid_unchecked(token,hwid,public_path,revoked_path);}
bool activate_for_hwid(std::string_view token,std::string_view hwid,std::string_view activation_path,std::string_view public_path,std::string_view revoked_path){
    if(hwid.empty()||hwid.size()>256||!json::valid_utf8(hwid)||!unbound_token_is_valid(token,public_path,revoked_path))return false;for(unsigned char c:hwid)if(c<0x20||c==0x7f)return false;
    const std::filesystem::path path(activation_path);try{auto existing=read_activation(path);if(existing)return existing->first==token&&existing->second==hwid&&unbound_token_is_valid(token,public_path,revoked_path);if(save_activation_exclusive(path,token,hwid))return true;existing=read_activation(path);return existing&&existing->first==token&&existing->second==hwid&&unbound_token_is_valid(token,public_path,revoked_path);}catch(...){return false;}
}
bool verify_activation_for_hwid(std::string_view token,std::string_view hwid,std::string_view activation_path,std::string_view public_path,std::string_view revoked_path){try{auto value=read_activation(std::filesystem::path(activation_path));return value&&value->first==token&&value->second==hwid&&unbound_token_is_valid(token,public_path,revoked_path);}catch(...){return false;}}
bool verify_active_license(std::string_view activation_path,std::string_view public_path,std::string_view revoked_path){try{auto record=read_activation(std::filesystem::path(activation_path));return record&&verify_activation_for_hwid(record->first,current_hwid(),activation_path,public_path,revoked_path);}catch(...){return false;}}
bool verify(std::string_view token){try{const auto hwid=current_hwid();return verify_for_hwid_unchecked(token,hwid,"config/license_public_key.bin","state/revoked.jsonl")||verify_activation_for_hwid(token,hwid,"config/license_activation.json","config/license_public_key.bin","state/revoked.jsonl");}catch(...){return false;}}
bool verify_issued(std::string_view token){return unbound_token_is_valid(token,"config/license_public_key.bin","state/revoked.jsonl");}

bool revoke_any(std::string_view token,std::string_view public_path,std::string_view revoked_path){
    try{require_signature(token,public_path);if(is_revoked(token,revoked_path))return true;std::filesystem::create_directories(std::filesystem::path(revoked_path).parent_path());std::ofstream file(std::filesystem::path(revoked_path),std::ios::binary|std::ios::app);if(!file)return false;json::Value::Object record;record["token"].data=std::string(token);json::Value value;value.data=std::move(record);file<<json::stringify(value)<<'\n';file.flush();return(bool)file;}catch(...){return false;}
}
bool revoke(std::string_view token){if(!verify(token))return false;return revoke_any(token);}
std::string reissue_for_hwid(std::string_view old_token,std::string_view plan,unsigned days,std::string_view new_hwid,std::string_view private_path,std::string_view revoked_path,std::string_view public_path){auto replacement=issue_for_hwid(plan,days,new_hwid,private_path);if(!revoke_any(old_token,public_path,revoked_path))throw std::runtime_error("old license could not be revoked; replacement was not delivered");return replacement;}
std::string reissue_unbound(std::string_view old_token,std::string_view plan,unsigned days,std::string_view private_path,std::string_view revoked_path,std::string_view public_path){auto replacement=issue_unbound(plan,days,private_path);if(!revoke_any(old_token,public_path,revoked_path))throw std::runtime_error("old license could not be revoked; replacement was not delivered");return replacement;}
}
