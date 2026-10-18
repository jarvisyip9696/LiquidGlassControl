#pragma once
#include <set>
#include <ocidl.h>
#include <tlhelp32.h>
#include <cstdio>
#include <memory>
#include <atomic>
#include <aclapi.h>
#include <sddl.h>
#include "glass_protocol.h"
namespace glass {
constexpr UINT HostExitedMessage=WM_APP+18;
class Controller {
    HANDLE mapping{};Config*data{};HMODULE module{};HOOKPROC hook{},messageHook{};UINT attach{},refresh{};std::map<DWORD,HHOOK> pendingHooks;
    HANDLE statusMapping{};Status*status{};
    LONG observedConnected=0,observedSubscribed=0;
    unsigned packageRetryRounds=0;
    struct PackageAttach {DWORD pid{},owner{};std::wstring path;std::atomic<HRESULT> result{E_PENDING};bool reported=false;};
    std::map<DWORD,std::shared_ptr<PackageAttach>> packageAttachments;
    static DWORD WINAPI attachPackage(void*context){std::unique_ptr<std::shared_ptr<PackageAttach>> holder(static_cast<std::shared_ptr<PackageAttach>*>(context));auto state=*holder;
        HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);HMODULE xaml=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Initialize=HRESULT(WINAPI*)(LPCWSTR,DWORD,LPCWSTR,LPCWSTR,CLSID,LPCWSTR);auto initialize=xaml?(Initialize)GetProcAddress(xaml,"InitializeXamlDiagnosticsEx"):nullptr;HRESULT hr=HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        if(initialize){auto init=std::to_wstring(state->owner)+L" classic";for(int i=1;i<=10000;++i){auto endpoint=L"VisualDiagConnection"+std::to_wstring(i);hr=initialize(endpoint.c_str(),state->pid,L"",state->path.c_str(),runtimeTapId(state->path),init.c_str());if(hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;}}
        state->result=hr;if(xaml)FreeLibrary(xaml);if(SUCCEEDED(apartment))CoUninitialize();return 0;
    }
    // Grant only this exact package read/execute on our extracted DLL and
    // read-only configuration access. No directory/system/process ACL changes.
    bool preparePackage(DWORD pid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;wchar_t image[MAX_PATH]{};DWORD chars=MAX_PATH;bool expected=QueryFullProcessImageNameW(p,0,image,&chars)&&(_wcsicmp(runtimeName(image).c_str(),L"ShellExperienceHost.exe")==0||startHost(runtimeName(image)));HANDLE token{};bool opened=expected&&OpenProcessToken(p,TOKEN_QUERY,&token);CloseHandle(p);if(!opened)return false;
        DWORD app=0,bytes=0;GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&bytes);if(!app){CloseHandle(token);return false;}GetTokenInformation(token,TokenAppContainerSid,nullptr,0,&bytes);std::vector<BYTE> buffer(bytes);bool got=bytes&&GetTokenInformation(token,TokenAppContainerSid,buffer.data(),bytes,&bytes);CloseHandle(token);if(!got)return false;auto sid=((TOKEN_APPCONTAINER_INFORMATION*)buffer.data())->TokenAppContainer;
        auto grant=[&](HANDLE object,SE_OBJECT_TYPE type,DWORD access){PSECURITY_DESCRIPTOR sd{};PACL old{},updated{};DWORD error=GetSecurityInfo(object,type,DACL_SECURITY_INFORMATION,nullptr,nullptr,&old,nullptr,&sd);if(error)return error;EXPLICIT_ACCESSW entry{};entry.grfAccessPermissions=access;entry.grfAccessMode=GRANT_ACCESS;entry.Trustee.TrusteeForm=TRUSTEE_IS_SID;entry.Trustee.TrusteeType=TRUSTEE_IS_USER;entry.Trustee.ptstrName=(LPWSTR)sid;error=SetEntriesInAclW(1,&entry,old,&updated);if(!error)error=SetSecurityInfo(object,type,DACL_SECURITY_INFORMATION,nullptr,nullptr,updated,nullptr);if(updated)LocalFree(updated);LocalFree(sd);return error;};
        HANDLE file=CreateFileW(dllPath.c_str(),READ_CONTROL|WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);DWORD error=file==INVALID_HANDLE_VALUE?GetLastError():grant(file,SE_FILE_OBJECT,FILE_GENERIC_READ|FILE_GENERIC_EXECUTE);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(!error)error=grant(mapping,SE_KERNEL_OBJECT,FILE_MAP_READ);if(!error)error=grant(statusMapping,SE_KERNEL_OBJECT,FILE_MAP_READ|FILE_MAP_WRITE);if(error){trace("notification package access error",error);return false;}return true;
    }
    void discoverPackage(DWORD pid){if(packageAttachments.count(pid)||!preparePackage(pid))return;auto state=std::make_shared<PackageAttach>();state->pid=pid;state->owner=GetCurrentProcessId();state->path=dllPath;auto holder=new std::shared_ptr<PackageAttach>(state);HANDLE worker=CreateThread(nullptr,0,attachPackage,holder,0,nullptr);if(worker){CloseHandle(worker);packageAttachments.emplace(pid,state);}else delete holder;}
    std::set<DWORD> threads;std::map<DWORD,std::wstring> processes;
    std::set<DWORD> attemptedThreads;unsigned packagedRounds=0;
    std::wstring dllPath,windowClass;bool failed=false;DWORD loadError=0;
    struct HostWatch {HANDLE process{},wait{};DWORD pid{};HWND window{};~HostWatch(){if(wait)UnregisterWaitEx(wait,INVALID_HANDLE_VALUE);if(process)CloseHandle(process);}};
    std::map<DWORD,std::unique_ptr<HostWatch>> hostWatches;unsigned recoveryRounds=0;
    static void CALLBACK hostEnded(void*context,BOOLEAN){auto watch=static_cast<HostWatch*>(context);PostMessageW(watch->window,HostExitedMessage,watch->pid,0);}
    void watchHosts(std::set<DWORD>const&ids){
        for(auto i=hostWatches.begin();i!=hostWatches.end();)if(!ids.count(i->first))i=hostWatches.erase(i);else ++i;
        for(auto pid:ids)if(!hostWatches.count(pid)&&hostWatches.size()<8){auto watch=std::make_unique<HostWatch>();watch->pid=pid;watch->window=FindWindowW(L"LiquidGlass.Control",nullptr);watch->process=OpenProcess(SYNCHRONIZE,FALSE,pid);
            if(watch->process&&watch->window&&RegisterWaitForSingleObject(&watch->wait,watch->process,hostEnded,watch.get(),INFINITE,WT_EXECUTEONLYONCE))hostWatches.emplace(pid,std::move(watch));}
    }
    void trace(const char*stage,DWORD value){if(strcmp(stage,"packaged shell processes")==0)return;auto at=dllPath.find_last_of(L"\\");auto file=dllPath.substr(0,at+1)+L"glass-controller.log";HANDLE h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);if(h!=INVALID_HANDLE_VALUE){char line[120]{};int n=snprintf(line,sizeof(line),"%s: %lu\r\n",stage,(unsigned long)value);DWORD written=0;WriteFile(h,line,n,&written,nullptr);CloseHandle(h);}}
    std::wstring process(DWORD id){auto found=processes.find(id);if(found!=processes.end())return found->second;if(processes.size()>=128)processes.clear();wchar_t path[32768]{};DWORD size=32768;HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,id);if(p){QueryFullProcessImageNameW(p,0,path,&size);CloseHandle(p);}std::wstring value=path;auto at=value.find_last_of(L"\\");if(at!=std::wstring::npos)value.erase(0,at+1);return processes[id]=value;}
    bool target(HWND h){DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())return false;auto exe=process(pid);wchar_t cls[128]{};GetClassNameW(h,cls,128);DWORD f=data?data->features:0;
        if((f&Keyboard)&&wcscmp(cls,L"Shell_InputSwitchTopLevelWindow")==0&&(!_wcsicmp(exe.c_str(),L"explorer.exe")||!_wcsicmp(exe.c_str(),L"ShellHost.exe")||!_wcsicmp(exe.c_str(),L"ShellExperienceHost.exe")))return true;
        if((f&Keyboard)&&!_wcsicmp(exe.c_str(),L"explorer.exe")&&(wcscmp(cls,L"XamlExplorerHostIslandWindow")==0||wcscmp(cls,L"XamlExplorerHostIslandWindow_WASDK")==0))return true;
        if(_wcsicmp(exe.c_str(),L"explorer.exe")==0){if((f&(Volume|Brightness|Taskbar|Dock|Capsule))&&(wcscmp(cls,L"Shell_TrayWnd")==0||wcscmp(cls,L"Shell_SecondaryTrayWnd")==0||wcscmp(cls,L"XamlExplorerHostIslandWindow")==0))return true;return (f&Explorer)&&(wcscmp(cls,L"CabinetWClass")==0||wcscmp(cls,L"ExploreWClass")==0);}
        return ((f&Start)&&startHost(exe))||((f&Notifications)&&notificationHostWindow(exe,cls));
    }
    // Queue attachment through WH_GETMESSAGE; never wait for a host window on the UI thread.
    void attachWindow(HWND h,bool newlyObserved=false){DWORD tid=GetWindowThreadProcessId(h,nullptr);if(!tid)return;
        // A real window can appear after an earlier worker-thread bootstrap
        // found no queue/runtime. That failed probe must not blacklist its UI.
        if(!pendingHooks.count(tid)){if(newlyObserved)threads.erase(tid);if(!threads.count(tid))attemptedThreads.erase(tid);}
        attachThread(tid,h);}
    static BOOL CALLBACK child(HWND h,LPARAM param){auto self=(Controller*)param;wchar_t name[128]{};GetClassNameW(h,name,128);if(wcsstr(name,L"Xaml")||wcsstr(name,L"DesktopWindow"))self->attachWindow(h);return TRUE;}
    static BOOL CALLBACK observedChild(HWND h,LPARAM param){auto self=(Controller*)param;wchar_t name[128]{};GetClassNameW(h,name,128);if(wcsstr(name,L"Xaml")||wcsstr(name,L"DesktopWindow"))self->attachWindow(h,true);return TRUE;}
    static BOOL CALLBACK window(HWND h,LPARAM param){auto self=(Controller*)param;if(self->target(h)){self->attachWindow(h);EnumChildWindows(h,child,param);}return TRUE;}
    static BOOL CALLBACK threadWindow(HWND h,LPARAM param){auto self=(Controller*)param;if(self->target(h))self->attachWindow(h);return TRUE;}
    void attachThread(DWORD tid,HWND targetWindow=nullptr){if(threads.count(tid)||pendingHooks.count(tid)||attemptedThreads.count(tid)||pendingHooks.size()>=32)return;attemptedThreads.insert(tid);HHOOK h=SetWindowsHookExW(WH_GETMESSAGE,messageHook,module,tid);if(!h){trace("message hook error",GetLastError());return;}
        // As in the Styler's deferred ShellHost initialization, enqueue on the
        // real window so modal/window-filtered message loops also receive it.
        BOOL posted=targetWindow?PostMessageW(targetWindow,attach,GetCurrentProcessId(),0):PostThreadMessageW(tid,attach,GetCurrentProcessId(),0);
        if(!posted){trace("thread queue unavailable",GetLastError());UnhookWindowsHookEx(h);return;}pendingHooks[tid]=h;auto window=FindWindowW(L"LiquidGlass.Control",nullptr);if(window)SetTimer(window,5,2200,nullptr);}
    void discoverPackagedShell(){
        std::set<DWORD> ids;HANDLE p=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(p!=INVALID_HANDLE_VALUE){PROCESSENTRY32W e{};e.dwSize=sizeof(e);if(Process32FirstW(p,&e))do{bool start=(data->features&Start)&&startHost(e.szExeFile);bool notifications=(data->features&Notifications)&&(_wcsicmp(e.szExeFile,L"ShellExperienceHost.exe")==0||_wcsicmp(e.szExeFile,L"ShellHost.exe")==0);if(start||notifications)ids.insert(e.th32ProcessID);}while(Process32NextW(p,&e));CloseHandle(p);}
        DWORD session=0;ProcessIdToSessionId(GetCurrentProcessId(),&session);for(auto i=ids.begin();i!=ids.end();){DWORD hostSession=0;if(!ProcessIdToSessionId(*i,&hostSession)||hostSession!=session)i=ids.erase(i);else ++i;}
        watchHosts(ids);for(auto pid:ids)discoverPackage(pid);trace("packaged shell processes",(DWORD)ids.size());if(ids.empty())return;HANDLE t=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(t==INVALID_HANDLE_VALUE)return;
        std::map<DWORD,std::vector<DWORD>> candidates;size_t longest=0;THREADENTRY32 e{};e.dwSize=sizeof(e);
        if(Thread32First(t,&e))do{if(ids.count(e.th32OwnerProcessID)){EnumThreadWindows(e.th32ThreadID,threadWindow,reinterpret_cast<LPARAM>(this));
            // Notification hosts must wait for the panel class, rather than
            // probing arbitrary worker queues before their XAML is ready.
            if(startHost(process(e.th32OwnerProcessID))){auto&list=candidates[e.th32OwnerProcessID];list.push_back(e.th32ThreadID);longest=std::max(longest,list.size());}
        }}while(Thread32Next(t,&e));
        CloseHandle(t);
        // Share each bounded batch between processes. A host with many idle
        // worker queues must not consume every slot before the panel's host.
        for(size_t index=0;index<longest&&pendingHooks.size()<32;++index)for(auto&candidate:candidates)if(index<candidate.second.size())attachThread(candidate.second[index]);
    }
public:
    ~Controller(){shutdown();}
    bool initialize(const std::wstring&dir){if(data)return true;
        // Extract the exact engine embedded in this EXE. A versioned filename
        // allows an old shell session to release its DLL naturally on sign-out.
        auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(201),RT_RCDATA);if(!resource)return false;DWORD size=SizeofResource(nullptr,resource);auto bytes=LockResource(LoadResource(nullptr,resource));
        unsigned long long hash=14695981039346656037ull;for(DWORD i=0;i<size;++i){hash^=((const BYTE*)bytes)[i];hash*=1099511628211ull;}dllPath=dir+L"LiquidGlass.Engine-"+std::to_wstring(hash)+L".dll";
        windowClass=threadClassName(dllPath);bool identical=false;HANDLE existing=CreateFileW(dllPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(existing!=INVALID_HANDLE_VALUE){LARGE_INTEGER length{};GetFileSizeEx(existing,&length);if(length.QuadPart==size){std::vector<BYTE> content(size);DWORD n=0;if(ReadFile(existing,content.data(),size,&n,nullptr)&&n==size)identical=memcmp(content.data(),bytes,size)==0;}CloseHandle(existing);}
        if(!identical){HANDLE file=CreateFileW(dllPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;DWORD n=0;BOOL ok=WriteFile(file,bytes,size,&n,nullptr);CloseHandle(file);if(!ok||n!=size)return false;}
        DWORD previousMode=0;BOOL changedMode=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&previousMode);
        module=LoadLibraryExW(dllPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);loadError=module?0:GetLastError();
        if(changedMode)SetThreadErrorMode(previousMode,nullptr);
        if(!module){trace("engine load error",loadError);return false;}hook=(HOOKPROC)GetProcAddress(module,"GlassHook");messageHook=(HOOKPROC)GetProcAddress(module,"GlassMessageHook");if(!hook||!messageHook){trace("engine export error",GetLastError());return false;}
        auto name=mappingName(GetCurrentProcessId());mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Config),name.c_str());if(!mapping)return false;data=(Config*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Config));if(!data)return false;Config initial;initial.owner=GetCurrentProcessId();initial.heartbeat=(LONG)GetTickCount();*data=initial;
        name=statusName(GetCurrentProcessId());statusMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Status),name.c_str());if(!statusMapping)return false;status=(Status*)MapViewOfFile(statusMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Status));if(!status)return false;Status initialStatus;initialStatus.owner=GetCurrentProcessId();*status=initialStatus;
        // Only the disposable status page is writable by a low-integrity host;
        // configuration remains read-only and at the controller's integrity.
        PSECURITY_DESCRIPTOR label{};if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"S:(ML;;NW;;;LW)",SDDL_REVISION_1,&label,nullptr))return false;PACL sacl{};BOOL present{},defaulted{};GetSecurityDescriptorSacl(label,&present,&sacl,&defaulted);DWORD labelError=SetSecurityInfo(statusMapping,SE_KERNEL_OBJECT,LABEL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,sacl);LocalFree(label);if(labelError){trace("status page label error",labelError);return false;}
        attach=RegisterWindowMessageW(AttachMessage);refresh=RegisterWindowMessageW(RefreshMessage);return true;
    }
    bool available()const{return data&&hook;}
    bool policyBlocked()const{return loadError==4551||loadError==577||loadError==1260;}
    DWORD initializationError()const{return loadError;}
    bool hostExited(DWORD pid){if(!hostWatches.count(pid))return false;hostWatches.erase(pid);packageAttachments.erase(pid);threads.clear();attemptedThreads.clear();processes.erase(pid);recoveryRounds=0;trace("shell host exited",pid);return true;}
    void tick(){if(data)InterlockedExchange(&data->heartbeat,(LONG)GetTickCount());if(status&&data){auto window=FindWindowW(L"LiquidGlass.Control",nullptr);for(DWORD bits=(DWORD)status->success&data->features;bits;){DWORD bit=bits&-bits;bits&=~bit;PostMessageW(window,RegisterWindowMessageW(StatusMessage),bit,(status->fallback&bit)?S_FALSE:S_OK);}if(status->failed&data->features)PostMessageW(window,RegisterWindowMessageW(StatusMessage),(DWORD)status->failed&data->features,status->error);}
        if(status&&status->connected!=observedConnected){observedConnected=status->connected;trace("connected host mask",observedConnected);}if(status&&status->subscribed!=observedSubscribed){observedSubscribed=status->subscribed;trace("subscribed host mask",observedSubscribed);}
        for(auto&entry:packageAttachments){auto hr=entry.second->result.load();if(hr!=E_PENDING&&!entry.second->reported){entry.second->reported=true;trace("notification diagnostics result",(DWORD)hr);if(hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)&&packageRetryRounds<15)SetTimer(FindWindowW(L"LiquidGlass.Control",nullptr),21,2000,nullptr);}}}
    void retryPackages(){if(!data||!(data->features&(Start|Notifications))||packageRetryRounds>=15)return;++packageRetryRounds;std::vector<DWORD> retry;for(auto const&entry:packageAttachments)if(entry.second->result==HRESULT_FROM_WIN32(ERROR_NOT_FOUND))retry.push_back(entry.first);for(auto pid:retry){packageAttachments.erase(pid);discoverPackage(pid);}}
    bool recoverHosts(){if(!data||!(data->features&(Start|Notifications)))return false;discoverPackagedShell();return ++recoveryRounds<6;}
    void acknowledge(DWORD tid){auto i=pendingHooks.find(tid);if(i!=pendingHooks.end()){UnhookWindowsHookEx(i->second);pendingHooks.erase(i);threads.insert(tid);}}
    void expireHooks(bool retry=false){for(auto&i:pendingHooks)UnhookWindowsHookEx(i.second);pendingHooks.clear();if(retry&&data&&data->features&&++packagedRounds<5)discoverPackagedShell();}
    const std::wstring&runtimePath()const{return dllPath;}
    bool factoryTest(){
        using GetFactory=HRESULT(WINAPI*)(REFCLSID,REFIID,void**);
        auto get=module?(GetFactory)GetProcAddress(module,"DllGetClassObject"):nullptr;if(!get)return false;
        IClassFactory*f=nullptr;IObjectWithSite*s=nullptr;
        HRESULT hr=get(runtimeTapId(dllPath),IID_IClassFactory,reinterpret_cast<void**>(&f));if(FAILED(hr)||!f)return false;
        hr=f->CreateInstance(nullptr,IID_IObjectWithSite,reinterpret_cast<void**>(&s));f->Release();if(FAILED(hr)||!s)return false;
        IUnknown*site=nullptr;bool ok=s->GetSite(IID_IUnknown,reinterpret_cast<void**>(&site))==E_FAIL;if(site)site->Release();s->Release();return ok;
    }
    HRESULT connectTest(){using Test=HRESULT(WINAPI*)();auto test=module?(Test)GetProcAddress(module,"GlassConnectTest"):nullptr;return test?test():E_NOINTERFACE;}
    HRESULT graphicsTest(){using Test=HRESULT(WINAPI*)();auto test=module?(Test)GetProcAddress(module,"GlassGraphicsTest"):nullptr;return test?test():E_NOINTERFACE;}
    void publish(Config c){if(!data)return;c=normalize(c);if(!(c.features&(Start|Notifications|Keyboard)))hostWatches.clear();if(status){InterlockedAnd(&status->success,c.features);InterlockedAnd(&status->fallback,c.features);InterlockedAnd(&status->failed,c.features);}c.owner=GetCurrentProcessId();InterlockedIncrement(&data->sequence);MemoryBarrier();data->features=c.features;data->opacity=c.opacity;data->radius=c.radius;data->tint=c.tint;data->blur=c.blur;data->economy=c.economy;MemoryBarrier();InterlockedIncrement(&data->sequence);}
    // Ordinary updates never synchronously block the controller on shell threads. Shutdown still waits for restoration.
    void broadcast(bool wait=false){if(!data)return;HWND h=nullptr;while((h=FindWindowExW(HWND_MESSAGE,h,windowClass.c_str(),nullptr))){if(wait){DWORD_PTR value=0;SendMessageTimeoutW(h,refresh,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&value);}else PostMessageW(h,refresh,0,0);}}
    bool discover(){if(!data)return false;failed=false;threads.clear();processes.clear();attemptedThreads.clear();packagedRounds=0;packageRetryRounds=0;for(auto i=packageAttachments.begin();i!=packageAttachments.end();)if(i->second->result!=E_PENDING&&FAILED(i->second->result.load()))i=packageAttachments.erase(i);else ++i;if(data->features){EnumWindows(window,reinterpret_cast<LPARAM>(this));discoverPackagedShell();}broadcast();return !failed;}
    bool wantsWindow(HWND h){return data&&data->features&&target(h);}
    void discoverWindow(HWND h){if(!wantsWindow(h))return;attachWindow(h,true);EnumChildWindows(h,observedChild,reinterpret_cast<LPARAM>(this));}
    unsigned sessions()const{unsigned count=0;HWND h=nullptr;while((h=FindWindowExW(HWND_MESSAGE,h,windowClass.c_str(),nullptr)))++count;return count;}
    void shutdown(){hostWatches.clear();packageAttachments.clear();expireHooks();if(data){Config c=*data;c.features=0;publish(c);broadcast(true);InterlockedExchange(&data->heartbeat,(LONG)(GetTickCount()-5000));UnmapViewOfFile(data);data=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}if(status){UnmapViewOfFile(status);status=nullptr;}if(statusMapping){CloseHandle(statusMapping);statusMapping=nullptr;}if(module){FreeLibrary(module);module=nullptr;}threads.clear();processes.clear();}
};
}
