#include "notify/hwid.hpp"
#include <windows.h>
#include <vector>
#include <stdexcept>
namespace notify {
std::optional<std::wstring> read_hwid(){HKEY key{};if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Cryptography",0,KEY_READ|KEY_WOW64_64KEY,&key)!=ERROR_SUCCESS)return{};DWORD type=0,size=0;auto rc=RegQueryValueExW(key,L"MachineGuid",nullptr,&type,nullptr,&size);if(rc!=ERROR_SUCCESS||type!=REG_SZ||size<sizeof(wchar_t)||size>65536){RegCloseKey(key);return{};}std::vector<wchar_t>b(size/sizeof(wchar_t)+1);rc=RegQueryValueExW(key,L"MachineGuid",nullptr,&type,(BYTE*)b.data(),&size);RegCloseKey(key);if(rc!=ERROR_SUCCESS||type!=REG_SZ)return{};std::wstring s(b.data());if(s.empty())return{};return s;}
}
