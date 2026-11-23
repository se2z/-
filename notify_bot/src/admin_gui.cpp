#include "notify/admin_gui.hpp"
#include "notify/license.hpp"
#include "notify/config.hpp"
#include <windows.h>
#include <wingdi.h>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <cstring>

namespace notify {
namespace {
enum : int { ID_PLAN=202,ID_DAYS=203,ID_ISSUE=204,ID_COPY=205,ID_OLD=206,ID_REVOKE=207,ID_REISSUE=208,ID_SECRET=209,ID_STATUS=210 };
struct AdminScreen { HWND plan{},days{},output{},old_key{},status{},secret_button{},issue{},copy{},revoke{},reissue{};HBRUSH edit_brush{},static_brush{};HFONT font{},small{},title{}; };

std::wstring read_control(HWND control){int length=GetWindowTextLengthW(control);std::wstring value((size_t)length+1,L'\0');GetWindowTextW(control,value.data(),length+1);value.resize((size_t)length);return value;}
std::string to_utf8(const std::wstring& value){if(value.empty())return{};int length=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),(int)value.size(),nullptr,0,nullptr,nullptr);if(length<=0)throw std::runtime_error("invalid UTF-16 input");std::string result((size_t)length,'\0');if(!WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),(int)value.size(),result.data(),length,nullptr,nullptr))throw std::runtime_error("UTF-8 conversion failed");return result;}
std::wstring to_wide(const std::string& value){if(value.empty())return{};int length=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),nullptr,0);if(length<=0)throw std::runtime_error("invalid UTF-8 output");std::wstring result((size_t)length,L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),(int)value.size(),result.data(),length))throw std::runtime_error("UTF-8 conversion failed");return result;}
void set_status(AdminScreen* screen,const std::wstring& value){SetWindowTextW(screen->status,value.c_str());}
unsigned days_value(AdminScreen* screen){auto text=read_control(screen->days);size_t used=0;unsigned long value=std::stoul(text,&used);if(used!=text.size()||value==0||value>3650)throw std::runtime_error("Срок должен быть от 1 до 3650 дней.");return(unsigned)value;}
std::string selected_plan(AdminScreen* screen){auto plan=to_utf8(read_control(screen->plan));if(plan.empty())throw std::runtime_error("Введите план лицензии.");return plan;}
void copy_text(HWND owner,const std::wstring& value){if(!OpenClipboard(owner))throw std::runtime_error("Не удалось открыть буфер обмена.");EmptyClipboard();const SIZE_T bytes=(value.size()+1)*sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!memory){CloseClipboard();throw std::runtime_error("Недостаточно памяти для копирования.");}void* target=GlobalLock(memory);if(!target){GlobalFree(memory);CloseClipboard();throw std::runtime_error("Буфер обмена недоступен.");}memcpy(target,value.c_str(),bytes);GlobalUnlock(memory);if(!SetClipboardData(CF_UNICODETEXT,memory)){GlobalFree(memory);CloseClipboard();throw std::runtime_error("Не удалось записать буфер обмена.");}CloseClipboard();}
void create_control(HWND parent,HWND& target,DWORD ex,const wchar_t* cls,const wchar_t* caption,DWORD style,int x,int y,int width,int height,int id){target=CreateWindowExW(ex,cls,caption,WS_CHILD|WS_VISIBLE|style,x,y,width,height,parent,(HMENU)(INT_PTR)id,nullptr,nullptr);}
void label(HWND parent,const wchar_t* text,int x,int y,int width,int height){CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT,x,y,width,height,parent,nullptr,nullptr,nullptr);}
void rounded_panel(HDC dc,RECT rect,COLORREF color){HBRUSH brush=CreateSolidBrush(color);HPEN pen=(HPEN)GetStockObject(NULL_PEN);auto old_brush=SelectObject(dc,brush);auto old_pen=SelectObject(dc,pen);RoundRect(dc,rect.left,rect.top,rect.right,rect.bottom,18,18);SelectObject(dc,old_brush);SelectObject(dc,old_pen);DeleteObject(brush);}
HFONT make_font(int height,int weight){return CreateFontW(-height,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");}
void paint_button(const DRAWITEMSTRUCT* item,AdminScreen* screen){HBRUSH surface=CreateSolidBrush(RGB(21,29,48));FillRect(item->hDC,&item->rcItem,surface);DeleteObject(surface);RECT rect=item->rcItem;InflateRect(&rect,-1,-1);const bool pressed=(item->itemState&ODS_SELECTED)!=0;rounded_panel(item->hDC,rect,pressed?RGB(84,97,215):RGB(101,115,241));wchar_t caption[128]{};GetWindowTextW(item->hwndItem,caption,128);SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,RGB(255,255,255));auto old=(HFONT)SelectObject(item->hDC,screen->font);RECT text_rect=item->rcItem;DrawTextW(item->hDC,caption,-1,&text_rect,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(item->hDC,old);if(item->itemState&ODS_FOCUS){RECT focus=item->rcItem;InflateRect(&focus,-7,-7);DrawFocusRect(item->hDC,&focus);}}
void paint(HWND window,AdminScreen* screen){PAINTSTRUCT ps{};HDC target=BeginPaint(window,&ps);RECT client{};GetClientRect(window,&client);HDC dc=CreateCompatibleDC(target);HBITMAP bitmap=CreateCompatibleBitmap(target,client.right,client.bottom);auto old_bitmap=SelectObject(dc,bitmap);TRIVERTEX vertices[2]={{0,0,0x0808,0x0d0d,0x1919,0},{client.right,client.bottom,0x1616,0x1420,0x3131,0}};GRADIENT_RECT gradient{0,1};GradientFill(dc,vertices,2,&gradient,1,GRADIENT_FILL_RECT_V);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(145,162,191));auto old_font=SelectObject(dc,screen->small);RECT eyebrow{32,25,900,45};DrawTextW(dc,L"NOTIFY  /  ADMINISTRATION",-1,&eyebrow,DT_LEFT|DT_TOP|DT_SINGLELINE);SelectObject(dc,screen->title);SetTextColor(dc,RGB(241,245,255));RECT heading{32,52,900,94};DrawTextW(dc,L"Управление лицензиями",-1,&heading,DT_LEFT|DT_TOP|DT_SINGLELINE);SelectObject(dc,screen->font);SetTextColor(dc,RGB(147,164,192));RECT sub{34,98,1100,120};DrawTextW(dc,L"Ключ выдаётся без HWID и привязывается при первой активации",-1,&sub,DT_LEFT|DT_TOP|DT_SINGLELINE);
    rounded_panel(dc,{24,140,548,600},RGB(21,29,48));rounded_panel(dc,{570,140,1156,600},RGB(21,29,48));SelectObject(dc,screen->title);SetTextColor(dc,RGB(235,241,255));RECT a{48,160,520,194};DrawTextW(dc,L"Новый ключ",-1,&a,DT_LEFT|DT_TOP|DT_SINGLELINE);RECT b{596,160,1130,194};DrawTextW(dc,L"Отвязка / отзыв",-1,&b,DT_LEFT|DT_TOP|DT_SINGLELINE);
    SelectObject(dc,old_font);
    BitBlt(target,0,0,client.right,client.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old_bitmap);SelectObject(dc,old_font);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window,&ps);}

void issue_key(AdminScreen* screen){try{auto key=issue_unbound(selected_plan(screen),days_value(screen));SetWindowTextW(screen->output,to_wide(key).c_str());set_status(screen,L"Ключ создан без HWID; привязка произойдёт при первой активации.");}catch(const std::exception& error){set_status(screen,to_wide(error.what()));}}
void revoke_key(AdminScreen* screen){try{auto token=to_utf8(read_control(screen->old_key));if(token.empty())throw std::runtime_error("Вставьте ключ для отзыва.");if(!revoke_any(token))throw std::runtime_error("Ключ не прошёл проверку подписи или уже отозван.");set_status(screen,L"Ключ отозван в локальном state/revoked.jsonl.");}catch(const std::exception& error){set_status(screen,to_wide(error.what()));}}
void reissue_key(AdminScreen* screen){try{auto token=to_utf8(read_control(screen->old_key));if(token.empty())throw std::runtime_error("Вставьте старый ключ.");auto key=reissue_unbound(token,selected_plan(screen),days_value(screen));SetWindowTextW(screen->output,to_wide(key).c_str());set_status(screen,L"Старый ключ отозван локально; новый ключ привяжется при первой активации.");}catch(const std::exception& error){set_status(screen,to_wide(error.what()));}}
void ensure_secret(AdminScreen* screen){try{std::filesystem::create_directories(L"config");if(std::filesystem::exists(L"config/license_private_key.bin")||std::filesystem::exists(L"config/license_public_key.bin")){EnableWindow(screen->secret_button,FALSE);set_status(screen,L"Пара ключей уже существует; повторная генерация запрещена.");return;}if(!new_secret()){set_status(screen,L"Не удалось создать пару config/license_private_key.bin и config/license_public_key.bin.");return;}EnableWindow(screen->secret_button,FALSE);set_status(screen,L"Пара RSA-ключей создана; закрытый ключ храните только у администратора.");}catch(const std::exception& error){set_status(screen,to_wide(error.what()));}}

LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){auto* screen=(AdminScreen*)GetWindowLongPtrW(window,GWLP_USERDATA);if(message==WM_NCCREATE){screen=(AdminScreen*)((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)screen);}
    if(message==WM_CREATE){screen->font=make_font(14,FW_SEMIBOLD);screen->small=make_font(11,FW_BOLD);screen->title=make_font(24,FW_SEMIBOLD);screen->edit_brush=CreateSolidBrush(RGB(13,19,33));screen->static_brush=CreateSolidBrush(RGB(21,29,48));
        label(window,L"План",48,207,200,20);create_control(window,screen->plan,WS_EX_CLIENTEDGE,L"EDIT",L"premium",WS_TABSTOP|ES_AUTOHSCROLL,48,232,250,38,ID_PLAN);
        label(window,L"Срок (дней)",320,207,190,20);create_control(window,screen->days,WS_EX_CLIENTEDGE,L"EDIT",L"30",WS_TABSTOP|ES_NUMBER,320,232,204,38,ID_DAYS);
        create_control(window,screen->issue,0,L"BUTTON",L"Создать ключ",WS_TABSTOP|BS_OWNERDRAW,48,286,228,42,ID_ISSUE);create_control(window,screen->copy,0,L"BUTTON",L"Копировать ключ",WS_TABSTOP|BS_OWNERDRAW,296,286,228,42,ID_COPY);
        label(window,L"Новый ключ",48,346,400,20);create_control(window,screen->output,WS_EX_CLIENTEDGE,L"EDIT",L"",ES_READONLY|ES_AUTOHSCROLL,48,370,476,102,ID_COPY+20);
        label(window,L"Старый ключ",596,207,500,22);create_control(window,screen->old_key,WS_EX_CLIENTEDGE,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,596,232,534,170,ID_OLD);
        create_control(window,screen->revoke,0,L"BUTTON",L"Отозвать ключ",WS_TABSTOP|BS_OWNERDRAW,596,424,242,44,ID_REVOKE);create_control(window,screen->reissue,0,L"BUTTON",L"Отвязать и перевыпустить",WS_TABSTOP|BS_OWNERDRAW,858,424,272,44,ID_REISSUE);
        create_control(window,screen->secret_button,0,L"BUTTON",L"Создать секрет, если отсутствует",WS_TABSTOP|BS_OWNERDRAW,596,516,534,42,ID_SECRET);
        create_control(window,screen->status,0,L"STATIC",L"Готово к операциям.",SS_LEFT,32,690,1100,28,ID_STATUS);
        for(HWND control:{screen->plan,screen->days,screen->output,screen->old_key,screen->issue,screen->copy,screen->revoke,screen->reissue,screen->secret_button,screen->status})if(control)SendMessageW(control,WM_SETFONT,(WPARAM)screen->font,TRUE);
        if(std::filesystem::exists(L"config/license_private_key.bin")||std::filesystem::exists(L"config/license_public_key.bin"))EnableWindow(screen->secret_button,FALSE);return 0;}
    if(message==WM_COMMAND&&HIWORD(wp)==BN_CLICKED){switch(LOWORD(wp)){case ID_ISSUE:issue_key(screen);return 0;case ID_COPY:try{auto value=read_control(screen->output);if(value.empty())throw std::runtime_error("Сначала создайте ключ.");copy_text(window,value);set_status(screen,L"Ключ скопирован в буфер обмена.");}catch(const std::exception& error){set_status(screen,to_wide(error.what()));}return 0;case ID_REVOKE:revoke_key(screen);return 0;case ID_REISSUE:reissue_key(screen);return 0;case ID_SECRET:ensure_secret(screen);return 0;}}
    if(message==WM_CTLCOLOREDIT&&screen){SetTextColor((HDC)wp,RGB(235,241,255));SetBkColor((HDC)wp,RGB(13,19,33));return(LRESULT)screen->edit_brush;}
    if(message==WM_CTLCOLORSTATIC&&screen){SetTextColor((HDC)wp,RGB(165,180,205));SetBkColor((HDC)wp,RGB(21,29,48));return(LRESULT)screen->static_brush;}
    if(message==WM_DRAWITEM&&screen){auto* item=(DRAWITEMSTRUCT*)lp;if(item->CtlID==ID_ISSUE||item->CtlID==ID_COPY||item->CtlID==ID_REVOKE||item->CtlID==ID_REISSUE||item->CtlID==ID_SECRET){paint_button(item,screen);return TRUE;}}
    if(message==WM_PAINT&&screen){paint(window,screen);return 0;}
    if(message==WM_DESTROY){PostQuitMessage(0);return 0;}return DefWindowProcW(window,message,wp,lp);
}
}

int run_admin_gui(HINSTANCE instance,int show){
    BuildConfig::load(L"config/build_config.json");auto* screen=new AdminScreen;WNDCLASSEXW wc{sizeof wc};wc.lpfnWndProc=window_proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);wc.lpszClassName=L"NotifyBotAdminLicenses";RegisterClassExW(&wc);
    HWND window=CreateWindowExW(0,wc.lpszClassName,L"Notify Admin | Лицензии",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1200,780,nullptr,nullptr,instance,screen);if(!window){delete screen;return 1;}ShowWindow(window,show);UpdateWindow(window);MSG message;while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
    if(screen->font)DeleteObject(screen->font);if(screen->small)DeleteObject(screen->small);if(screen->title)DeleteObject(screen->title);if(screen->edit_brush)DeleteObject(screen->edit_brush);if(screen->static_brush)DeleteObject(screen->static_brush);delete screen;return(int)message.wParam;
}
}
