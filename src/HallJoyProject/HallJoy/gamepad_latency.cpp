#include "gamepad_latency.h"
#if HALLJOY_CAMERA_LATENCY_TEST_ENABLED
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <process.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include "gamepad_latency.h"
#include "gamepad_latency_model.h"
#include "worker_exception_barrier.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <memory>
#include <stdexcept>
#include <vector>
#pragma comment(lib,"windowsapp.lib")
#pragma comment(lib,"comdlg32.lib")
namespace {
using namespace winrt::Windows::Gaming::Input;
constexpr wchar_t ClassName[]=L"HallJoyLatencyCamera";
constexpr UINT Activate=WM_APP+201;
constexpr int DeviceId=101,SignalId=102,ResetId=103,SaveId=104;
std::mutex lifecycle;
HANDLE worker=nullptr,stop=nullptr;
std::atomic<HWND> window{nullptr};
int64_t Now(){LARGE_INTEGER n{};QueryPerformanceCounter(&n);return n.QuadPart;}
struct Event {int64_t begin=0,end=0,submit=0;uint64_t os=0;double gapMs=0,value=0;bool connected=false,active=false;};
struct State {
    HWND hwnd=nullptr,devices=nullptr,signals=nullptr;
    HDC canvas=nullptr;HBITMAP canvasBitmap=nullptr;HGDIOBJ canvasOriginal=nullptr;
    int canvasWidth=0,canvasHeight=0;
    ~State(){if(canvas){SelectObject(canvas,canvasOriginal);DeleteObject(canvasBitmap);DeleteDC(canvas);}}
    HDC Canvas(HDC target,int width,int height){
        if(!canvas){canvas=CreateCompatibleDC(target);if(!canvas)return target;}
        if(width!=canvasWidth || height!=canvasHeight){
            auto bitmap=CreateCompatibleBitmap(target,std::max(width,1),std::max(height,1));
            if(!bitmap)return target;
            auto previous=SelectObject(canvas,bitmap);
            if(canvasBitmap)DeleteObject(canvasBitmap);else canvasOriginal=previous;
            canvasBitmap=bitmap;canvasWidth=width;canvasHeight=height;
        }
        return canvas;
    }
    std::vector<Gamepad> pads;
    Gamepad selected{nullptr};
    std::wstring deviceName;
    int signal=0;
    halljoy::latency::Edges edges;
    std::array<Event,8192> events{};
    size_t count=0,next=0;uint64_t overwritten=0;
    int64_t frequency=1,lastRead=0,lastInventory=0,lastPaint=0;
    double maxGapMs=0,lastGapMs=0,lastReadMs=0,lastSubmitMs=0;
    Event current{};
    bool haveReading=false,dirty=true,resetTiming=true,everSelected=false;
    State(){LARGE_INTEGER f{};QueryPerformanceFrequency(&f);frequency=f.QuadPart;}
    double Ms(int64_t ticks)const{return double(ticks)*1000./double(frequency);}
    void Reset(){edges={};count=next=0;overwritten=0;maxGapMs=lastGapMs=lastReadMs=lastSubmitMs=0;lastRead=0;haveReading=false;dirty=true;resetTiming=true;}
    void Inventory(){
        std::vector<Gamepad> found;
        for(auto p:Gamepad::Gamepads())found.push_back(p);
        if(found==pads)return;
        const bool initial=!everSelected;
        pads=std::move(found);
        SendMessageW(devices,CB_RESETCONTENT,0,0);
        SendMessageW(devices,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Select a Windows controller"));
        int choice=0;
        for(size_t i=0;i<pads.size();++i){
            auto raw=RawGameController::FromGameController(pads[i]);
            wchar_t label[128]{};
            swprintf_s(label,L"%u: %.110s",unsigned(i+1),raw.DisplayName().c_str());
            SendMessageW(devices,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
            if(selected && selected==pads[i])choice=int(i+1);
        }
        if(initial && pads.size()==1)choice=1;
        SendMessageW(devices,CB_SETCURSEL,choice,0);
        Select(choice,false);
    }
    void Select(int choice,bool explicitChoice=true){
        const Gamepad replacement=choice>0 && size_t(choice)<=pads.size()?pads[choice-1]:Gamepad{nullptr};
        if(replacement!=selected){selected=replacement;if(explicitChoice || replacement)Reset();else {edges.Disconnect();haveReading=false;dirty=true;} }
        if(selected)everSelected=true;
        wchar_t label[128]{};
        if(choice>0)SendMessageW(devices,CB_GETLBTEXT,choice,reinterpret_cast<LPARAM>(label));
        if(choice>0)deviceName=label;dirty=true;
    }
    void Sample(){
        Event e{};e.begin=Now();
        try {
            if(selected){
                auto r=selected.GetCurrentReading();e.os=r.Timestamp;e.connected=true;
                switch(signal){
                case 0:e.value=r.LeftThumbstickY;break;
                case 1:e.value=r.LeftThumbstickX;break;
                case 2:e.value=r.RightThumbstickY;break;
                case 3:e.value=r.RightThumbstickX;break;
                case 4:e.value=r.LeftTrigger;break;
                case 5:e.value=r.RightTrigger;break;
                default:e.value=static_cast<uint32_t>(r.Buttons)!=0?1.:0.;break;
                }
                if(!std::isfinite(e.value))e.connected=false;
            }
        }catch(...){e.connected=false;}
        e.end=Now();lastReadMs=Ms(e.end-e.begin);
        if(lastRead && !resetTiming){lastGapMs=Ms(e.begin-lastRead);maxGapMs=std::max(maxGapMs,lastGapMs);}
        e.gapMs=resetTiming?0:lastGapMs;resetTiming=false;lastRead=e.begin;
        const bool edge=edges.Observe(e.connected,e.value);e.active=edges.active;
        const bool changed=!haveReading || edge || e.value!=current.value || e.connected!=current.connected;
        if(changed){
            current=e;haveReading=true;dirty=true;
            events[next]=e;next=(next+1)%events.size();
            if(count<events.size())++count;else ++overwritten;
        }
    }
    void Paint(){
        PAINTSTRUCT ps{};HDC target=BeginPaint(hwnd,&ps);RECT rc{};GetClientRect(hwnd,&rc);
        HDC dc=Canvas(target,rc.right,rc.bottom);
        const int scale=GetDpiForWindow(hwnd);auto unit=[&](int x){return MulDiv(x,scale,96);};
        auto fill=[&](RECT rect,COLORREF color){HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush);};
        fill(rc,RGB(22,27,37));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(225,233,245));
        auto oldFont=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
        const bool axis=signal<4;
        const double value=edges.known?std::clamp(current.value,axis?-1.:0.,1.):0.;
        const int left=unit(32),right=std::max(left+1,int(rc.right)-unit(32));
        const int footerTop=std::max(unit(190),int(rc.bottom)-unit(100));
        const int barTop=unit(110),barBottom=std::max(barTop+unit(28),footerTop-unit(40));
        const int zero=axis?(left+right)/2:left;
        const int tip=axis?zero+int(std::lround(value*(right-left)/2.)):left+int(std::lround(value*(right-left)));
        RECT bar{left,barTop,right,barBottom};fill(bar,RGB(39,49,65));
        if(edges.known && tip!=zero)fill(RECT{std::min(zero,tip),barTop,std::max(zero,tip),barBottom},value<0?RGB(77,154,255):RGB(47,205,173));
        for(int i=0;i<=10;++i){
            const int x=left+(right-left)*i/10;
            fill(RECT{x,barTop,x+unit(i==5 && axis?2:1),barBottom},i==5 && axis?RGB(194,207,228):RGB(64,79,99));
            if(i%2==0 || (axis && i==5)){
                wchar_t tick[20]{};swprintf_s(tick,L"%d%%",axis?i*20-100:i*10);
                RECT label{x-unit(24),barBottom+unit(8),x+unit(24),barBottom+unit(28)};
                DrawTextW(dc,tick,-1,&label,DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);
            }
        }
        if(edges.known)fill(RECT{std::max(left,tip-unit(1)),barTop-unit(4),std::min(right,tip+unit(2)),barBottom+unit(4)},RGB(237,244,255));
        wchar_t heading[160]{};
        const wchar_t* names[]={L"Left stick Y",L"Left stick X",L"Right stick Y",L"Right stick X",L"Left trigger",L"Right trigger",L"Any button"};
        if(edges.known)swprintf_s(heading,L"%s    %+.3f%%",names[std::clamp(signal,0,6)],value*100.);
        else swprintf_s(heading,L"Select a connected controller");
        HFONT large=CreateFontW(-unit(30),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if(large)SelectObject(dc,large);
        RECT title{left,unit(54),right,unit(102)};DrawTextW(dc,heading,-1,&title,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
        SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));if(large)DeleteObject(large);
        wchar_t text[768]{};
        swprintf_s(text,L"Raw value: %+.8f | press %llu / release %llu\nPoll gap %.3f ms (max %.3f) | read %.3f ms | last change to GDI %.3f ms\nLive controller output. No extra deadzone, smoothing or animation.\nHost timings are not physical key latency; camera includes display delay. CSV changes: %u (overwritten %llu).",
            current.value,static_cast<unsigned long long>(edges.presses),static_cast<unsigned long long>(edges.releases),
            lastGapMs,maxGapMs,lastReadMs,lastSubmitMs,unsigned(count),static_cast<unsigned long long>(overwritten));
        RECT footer{left,footerTop,right,rc.bottom};DrawTextW(dc,text,-1,&footer,DT_LEFT|DT_TOP|DT_NOPREFIX);
        SelectObject(dc,oldFont);
        if(dc!=target)BitBlt(target,0,0,rc.right,rc.bottom,dc,0,0,SRCCOPY);
        // Submission timestamp is not monitor scanout or photons.
        GdiFlush();const auto submitted=Now();
        if(haveReading && current.submit==0){current.submit=submitted;lastSubmitMs=Ms(submitted-current.end);if(count)events[(next+events.size()-1)%events.size()].submit=submitted;}
        EndPaint(hwnd,&ps);lastPaint=Now();dirty=false;
    }
    void Save(){
        wchar_t path[MAX_PATH]=L"HallJoy-gamepad-timing.csv";
        OPENFILENAMEW dialog{sizeof(dialog)};dialog.hwndOwner=hwnd;dialog.lpstrFile=path;dialog.nMaxFile=MAX_PATH;
        dialog.lpstrFilter=L"CSV (*.csv)\0*.csv\0\0";dialog.lpstrDefExt=L"csv";
        dialog.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
        if(GetSaveFileNameW(&dialog)){
            std::ofstream out{std::filesystem::path(path)};
            out<<"# QPC frequency,"<<frequency<<"\n# signal,"<<signal<<"\n# max_poll_gap_ms,"<<maxGapMs<<"\n# overwritten,"<<overwritten
               <<"\n# Times are host observation/GDI submission, not physical actuation or photons.\n"
               <<"read_begin_qpc,read_end_qpc,gdi_submit_qpc,os_timestamp_raw,poll_gap_ms,connected,active,value\n"<<std::setprecision(17);
            const size_t first=(next+events.size()-count)%events.size();
            for(size_t i=0;i<count;++i){const auto& e=events[(first+i)%events.size()];out<<e.begin<<','<<e.end<<','<<e.submit<<','<<e.os<<','<<e.gapMs<<','<<e.connected<<','<<e.active<<','<<e.value<<'\n';}
            out.close();if(!out)MessageBoxW(hwnd,L"Could not save the timing file.",L"HallJoy",MB_OK|MB_ICONERROR);
        }
        // A modal dialog is a sampling pause, not a giant input latency sample.
        resetTiming=true;edges.Disconnect();haveReading=false;dirty=true;
    }
};
LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto* p=reinterpret_cast<State*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){p=static_cast<State*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);p->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(p));}
    if(!p)return DefWindowProcW(hwnd,msg,wp,lp);
    try {
        switch(msg){
        case WM_CREATE:{
            auto control=[&](const wchar_t* type,const wchar_t* name,DWORD style,int x,int w,int id){
                const auto scale=GetDpiForWindow(hwnd);auto s=[&](int v){return MulDiv(v,scale,96);};
                HWND c=CreateWindowExW(0,type,name,WS_CHILD|WS_VISIBLE|WS_TABSTOP|style,s(x),s(8),s(w),s(type[0]==L'C'?240:28),hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
                SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);return c;
            };
            p->devices=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL,8,265,DeviceId);
            p->signals=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST,280,150,SignalId);
            for(const auto* label:{L"Left stick Y",L"Left stick X",L"Right stick Y",L"Right stick X",L"Left trigger",L"Right trigger",L"Any button"})SendMessageW(p->signals,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
            SendMessageW(p->signals,CB_SETCURSEL,0,0);
            control(L"BUTTON",L"Reset",BS_PUSHBUTTON,438,65,ResetId);control(L"BUTTON",L"Save CSV",BS_PUSHBUTTON,511,95,SaveId);
            return 0;}
        case WM_COMMAND:
            if(LOWORD(wp)==DeviceId && HIWORD(wp)==CBN_SELCHANGE)p->Select(int(SendMessageW(p->devices,CB_GETCURSEL,0,0)));
            if(LOWORD(wp)==SignalId && HIWORD(wp)==CBN_SELCHANGE){p->signal=int(SendMessageW(p->signals,CB_GETCURSEL,0,0));p->Reset();}
            if(LOWORD(wp)==ResetId)p->Reset();
            if(LOWORD(wp)==SaveId)p->Save();return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:p->Paint();return 0;
        case WM_SIZE:p->resetTiming=true;p->edges.Disconnect();p->haveReading=false;p->dirty=true;return 0;
        case WM_GETMINMAXINFO:reinterpret_cast<MINMAXINFO*>(lp)->ptMinTrackSize={MulDiv(660,GetDpiForWindow(hwnd),96),MulDiv(360,GetDpiForWindow(hwnd),96)};return 0;
        case WM_DPICHANGED:{const auto* r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
            const int dpi=GetDpiForWindow(hwnd);const int ids[]={DeviceId,SignalId,ResetId,SaveId},xs[]={8,280,438,511},ws[]={265,150,65,95};
            for(int i=0;i<4;++i)MoveWindow(GetDlgItem(hwnd,ids[i]),MulDiv(xs[i],dpi,96),MulDiv(8,dpi,96),MulDiv(ws[i],dpi,96),MulDiv(i<2?240:28,dpi,96),TRUE);
            return 0;}
        case Activate:ShowWindow(hwnd,SW_RESTORE);SetForegroundWindow(hwnd);return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
        }
    }catch(...){PostMessageW(hwnd,WM_CLOSE,0,0);}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
struct Resources {
    HANDLE timer=nullptr;HWND hwnd=nullptr;
    ~Resources(){if(hwnd && IsWindow(hwnd))DestroyWindow(hwnd);if(timer)CloseHandle(timer);window.store(nullptr);}
};
unsigned __stdcall Run(void*) noexcept {
    return halljoy::worker::RunWorkerEntryBarrier([]()->unsigned{
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        struct Apartment{~Apartment(){winrt::uninit_apartment();}} apartment;
        auto owned=std::make_unique<State>();State& state=*owned;Resources resources;
        WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=ClassName;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        RegisterClassW(&wc);
        resources.timer=CreateWaitableTimerExW(nullptr,nullptr,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,TIMER_ALL_ACCESS);
        if(!resources.timer)throw std::runtime_error("high resolution timer unavailable");
        resources.hwnd=CreateWindowExW(0,ClassName,L"HallJoy - Camera latency test",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,920,600,nullptr,nullptr,wc.hInstance,&state);
        if(!resources.hwnd)throw std::runtime_error("latency window unavailable");
        window.store(resources.hwnd);ShowWindow(resources.hwnd,SW_SHOW);
        bool quit=false;
        while(!quit && WaitForSingleObject(stop,0)!=WAIT_OBJECT_0){
            MSG msg{};unsigned budget=0;
            while(budget++<32 && PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                if(msg.message==WM_QUIT){quit=true;break;}
                if(!IsDialogMessageW(resources.hwnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
            }
            if(quit)break;
            const auto now=Now();const bool visible=!IsIconic(resources.hwnd) && IsWindowVisible(resources.hwnd);
            if(visible){
                if(state.Ms(now-state.lastInventory)>=1000){state.Inventory();state.lastInventory=now;}
                state.Sample();
                if(state.dirty || state.Ms(now-state.lastPaint)>=250){InvalidateRect(resources.hwnd,nullptr,FALSE);UpdateWindow(resources.hwnd);}
            }
            LARGE_INTEGER due{};due.QuadPart=visible?-5000:-500000; // 0.5ms requested wait; actual gaps are measured, no busy spin
            if(!SetWaitableTimer(resources.timer,&due,0,nullptr,nullptr,FALSE))throw std::runtime_error("timer arm failed");
            HANDLE handles[]={stop,resources.timer};
            if(MsgWaitForMultipleObjectsEx(2,handles,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE)==WAIT_FAILED)throw std::runtime_error("latency wait failed");
        }
        return 0;
    },[](const halljoy::worker::WorkerExceptionRecord&) noexcept {
        MessageBoxW(nullptr,L"The camera latency tester could not run. Normal gamepad output is unchanged.",L"HallJoy",MB_OK|MB_ICONERROR);
    },[](const halljoy::worker::WorkerExceptionRecord&) noexcept {},1);
}
}
void GamepadLatency_Open(HWND owner) noexcept {
    try {
        std::lock_guard<std::mutex> lock(lifecycle);
        if(worker && WaitForSingleObject(worker,0)==WAIT_OBJECT_0){CloseHandle(worker);worker=nullptr;CloseHandle(stop);stop=nullptr;}
        if(worker){if(auto hwnd=window.load())PostMessageW(hwnd,Activate,0,0);return;}
        stop=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(stop)worker=reinterpret_cast<HANDLE>(_beginthreadex(nullptr,0,Run,nullptr,0,nullptr));
        if(!worker){if(stop)CloseHandle(stop);stop=nullptr;MessageBoxW(owner,L"Could not start the camera latency tester.",L"HallJoy",MB_OK|MB_ICONERROR);}
    }catch(...){}
}
void GamepadLatency_Stop() noexcept {
    std::lock_guard<std::mutex> lock(lifecycle);
    if(!worker)return;SetEvent(stop);
    if(auto hwnd=window.load())PostMessageW(hwnd,WM_CLOSE,0,0);
    // Retain handles if a system API stalls. Never free state under a live thread.
    if(WaitForSingleObject(worker,3000)==WAIT_OBJECT_0){CloseHandle(worker);CloseHandle(stop);worker=nullptr;stop=nullptr;}
}

#else
void GamepadLatency_Open(HWND) noexcept {}
void GamepadLatency_Stop() noexcept {}
#endif
