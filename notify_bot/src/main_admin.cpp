#include "notify/admin_gui.hpp"
#include <filesystem>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    wchar_t path[32768]{};DWORD size=GetModuleFileNameW(nullptr,path,32768);if(size==0||size>=32768)return 2;
    std::filesystem::current_path(std::filesystem::path(path).parent_path().parent_path().parent_path());
    return notify::run_admin_gui(instance,show);
}
