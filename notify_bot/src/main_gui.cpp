#include "notify/gui.hpp"
#include <windows.h>
#include <filesystem>
#include <vector>
#include <stdexcept>
int WINAPI wWinMain(HINSTANCE h,HINSTANCE, PWSTR,int n){try{return notify::run_gui(h,n);}catch(const std::exception& error){const int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,error.what(),-1,nullptr,0);std::vector<wchar_t> message(count>0?(size_t)count:64,L'\0');if(count>0)MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,error.what(),-1,message.data(),count);MessageBoxW(nullptr,count>0?message.data():L"Application startup failed.",L"Notify Bot",MB_ICONERROR|MB_OK);return 1;}}
