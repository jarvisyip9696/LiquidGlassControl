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
    HANDLE statusMapping{};Status*status{};const wchar_t* endpoint=L"LiquidGlass.Control";
    LONG observedConnected=0,observedSubscribed=0; StatusCursor statusCursor;bool statusDirty=true;
    unsigned packageRetryRounds=0;
    // init: "<owner pid> classic|modern <slot> [<config> <status> <process>]"; slot is the host's glass::HostSlot, the hex handles are duplicated into the host (see HostHandles).
    struct PackageAttach {DWORD pid{},owner{};std::wstring path,init;std::atomic<HRESULT> result{E_PENDING};bool reported=false;};
    // Per host pid, duplicated once and passed unchanged on every retry. The controller never closes anything inside a host and the engine never closes them either: the leak is bounded at 3 handles per host process per controller instance, and they die with the host.
    // created: the host's creation time, so a reused pid never inherits another process's handle values; tries: duplication attempts (given up after 5).
    struct HostHandles {HANDLE cfg{},st{},proc{};bool ok=false;int slot=0,tries=0;ULONGLONG created=0;};
    static ULONGLONG createdAt(DWORD pid){ULONGLONG born=0;if(HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid)){FILETIME c{},e{},k{},u{};if(GetProcessTimes(p,&c,&e,&k,&u))born=((ULONGLONG)c.dwHighDateTime<<32)|c.dwLowDateTime;CloseHandle(p);}return born;}
    std::map<DWORD,HostHandles> hostHandles;std::map<DWORD,ULONGLONG> prepareFailedAt;std::set<DWORD> prepareLogged;
    std::map<DWORD,std::shared_ptr<PackageAttach>> packageAttachments;
    static DWORD WINAPI attachPackage(void*context){std::unique_ptr<std::shared_ptr<PackageAttach>> holder(static_cast<std::shared_ptr<PackageAttach>*>(context));auto state=*holder;
        HRESULT apartment=CoInitializeEx(nullptr,COINIT_MULTITHREADED);HMODULE xaml=LoadLibraryExW(L"Windows.UI.Xaml.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        using Initialize=HRESULT(WINAPI*)(LPCWSTR,DWORD,LPCWSTR,LPCWSTR,CLSID,LPCWSTR);auto initialize=xaml?(Initialize)GetProcAddress(xaml,"InitializeXamlDiagnosticsEx"):nullptr;HRESULT hr=HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);
        if(initialize){auto init=state->init;for(int i=1;i<=10000;++i){auto connection=L"VisualDiagConnection"+std::to_wstring(i);hr=initialize(connection.c_str(),state->pid,L"",state->path.c_str(),runtimeTapId(state->path),init.c_str());if(hr!=HRESULT_FROM_WIN32(ERROR_NOT_FOUND))break;}}
        state->result=hr;if(xaml)FreeLibrary(xaml);if(SUCCEEDED(apartment))CoUninitialize();return 0;
    }
    // Grant only this exact package read/execute on our extracted DLL and
    // read-only configuration access. No directory/system/process ACL changes.
    bool preparePackage(DWORD pid){HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!p)return false;wchar_t image[MAX_PATH]{};DWORD chars=MAX_PATH;bool expected=QueryFullProcessImageNameW(p,0,image,&chars)&&(_wcsicmp(runtimeName(image).c_str(),L"ShellExperienceHost.exe")==0||_wcsicmp(runtimeName(image).c_str(),L"ShellHost.exe")==0||startHost(runtimeName(image)));HANDLE token{};bool opened=expected&&OpenProcessToken(p,TOKEN_QUERY,&token);CloseHandle(p);if(!opened)return false;
        DWORD app=0,bytes=0;GetTokenInformation(token,TokenIsAppContainer,&app,sizeof(app),&bytes);if(!app){CloseHandle(token);return false;}GetTokenInformation(token,TokenAppContainerSid,nullptr,0,&bytes);std::vector<BYTE> buffer(bytes);bool got=bytes&&GetTokenInformation(token,TokenAppContainerSid,buffer.data(),bytes,&bytes);CloseHandle(token);if(!got)return false;auto sid=((TOKEN_APPCONTAINER_INFORMATION*)buffer.data())->TokenAppContainer;
        auto grant=[&](HANDLE object,SE_OBJECT_TYPE type,DWORD access){PSECURITY_DESCRIPTOR sd{};PACL old{},updated{};DWORD error=GetSecurityInfo(object,type,DACL_SECURITY_INFORMATION,nullptr,nullptr,&old,nullptr,&sd);if(error)return error;EXPLICIT_ACCESSW entry{};entry.grfAccessPermissions=access;entry.grfAccessMode=GRANT_ACCESS;entry.Trustee.TrusteeForm=TRUSTEE_IS_SID;entry.Trustee.TrusteeType=TRUSTEE_IS_USER;entry.Trustee.ptstrName=(LPWSTR)sid;error=SetEntriesInAclW(1,&entry,old,&updated);if(!error)error=SetSecurityInfo(object,type,DACL_SECURITY_INFORMATION,nullptr,nullptr,updated,nullptr);if(updated)LocalFree(updated);LocalFree(sd);return error;};
        HANDLE file=CreateFileW(dllPath.c_str(),READ_CONTROL|WRITE_DAC,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);DWORD error=file==INVALID_HANDLE_VALUE?GetLastError():grant(file,SE_FILE_OBJECT,FILE_GENERIC_READ|FILE_GENERIC_EXECUTE);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(!error)error=grant(mapping,SE_KERNEL_OBJECT,FILE_MAP_READ);if(!error)error=grant(statusMapping,SE_KERNEL_OBJECT,FILE_MAP_READ|FILE_MAP_WRITE);if(error){trace("notification package access error",error);return false;}return true;
    }
    void clearSlot(int slot){if(status&&slot>=0&&slot<SlotCount){InterlockedExchange(&status->hostStage[slot],0);InterlockedExchange(&status->hostError[slot],0);}}
    bool moduleLoaded(DWORD pid,const wchar_t*name){bool found=false;HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);if(s==INVALID_HANDLE_VALUE)return false;MODULEENTRY32W e{};e.dwSize=sizeof(e);if(Module32FirstW(s,&e))do{if(_wcsicmp(e.szModule,name)==0){found=true;break;}}while(Module32NextW(s,&e));CloseHandle(s);return found;}
    // An AppContainer host resolves "Local\" names in its own namespace, so the sections and the owner process are handed over as duplicated handles in the init string; the name path stays the fallback.
    void discoverPackage(DWORD pid){if(packageAttachments.count(pid))return;
        // A failed DACL preparation (also the normal "not an AppContainer" answer) is logged once per pid and redone at most once per minute.
        auto lastFail=prepareFailedAt.find(pid);if(lastFail!=prepareFailedAt.end()&&GetTickCount64()-lastFail->second<60000)return;
        if(!preparePackage(pid)){prepareFailedAt[pid]=GetTickCount64();if(prepareLogged.insert(pid).second)trace("package host not prepared",pid);return;}prepareFailedAt.erase(pid);
        auto state=std::make_shared<PackageAttach>();state->pid=pid;state->owner=GetCurrentProcessId();state->path=dllPath;
        // Reuse the cached values only for the same process (creation time) and only while they work or the retries are used up.
        ULONGLONG born=createdAt(pid);int tries=0;auto found=hostHandles.find(pid);
        if(found!=hostHandles.end()){if(found->second.created==born&&(found->second.ok||found->second.tries>=5))tries=-1;else{if(found->second.created==born)tries=found->second.tries;hostHandles.erase(found);found=hostHandles.end();}}
        if(found==hostHandles.end()){HostHandles hh;hh.slot=hostSlot(process(pid),false);hh.created=born;hh.tries=tries+1;
            HANDLE host=OpenProcess(PROCESS_DUP_HANDLE,FALSE,pid);if(host){HANDLE self=GetCurrentProcess();hh.ok=DuplicateHandle(self,mapping,host,&hh.cfg,FILE_MAP_READ,FALSE,0)&&DuplicateHandle(self,statusMapping,host,&hh.st,FILE_MAP_READ|FILE_MAP_WRITE,FALSE,0)&&DuplicateHandle(self,self,host,&hh.proc,SYNCHRONIZE,FALSE,0);if(!hh.ok)trace(hh.tries>=5?"package handle transport gave up":"package handle transport error",GetLastError());CloseHandle(host);}else trace("package host open error",GetLastError());
            found=hostHandles.emplace(pid,hh).first;}
        clearSlot(found->second.slot);
        // Only the classic Windows.UI.Xaml session can be attached through this path (the modern one is created by the engine from inside the host), so the framework is explicit.
        state->init=std::to_wstring(state->owner)+L" classic "+std::to_wstring(found->second.slot);if(moduleLoaded(pid,L"Microsoft.UI.Xaml.dll"))trace("package host has Microsoft.UI.Xaml (modern not attachable here)",pid);
        if(found->second.ok){wchar_t hex[96]{};swprintf(hex,96,L" %llx %llx %llx",(unsigned long long)(ULONG_PTR)found->second.cfg,(unsigned long long)(ULONG_PTR)found->second.st,(unsigned long long)(ULONG_PTR)found->second.proc);state->init+=hex;}
        auto holder=new std::shared_ptr<PackageAttach>(state);HANDLE worker=CreateThread(nullptr,0,attachPackage,holder,0,nullptr);if(worker){CloseHandle(worker);packageAttachments.emplace(pid,state);}else delete holder;}
    // Panel window of a notification host appeared: forget a failed attach of that process and try again, also past the timed retry cap.
    void rediscoverPackage(DWORD pid){auto i=packageAttachments.find(pid);if(i!=packageAttachments.end()){auto hr=i->second->result.load();if(hr!=E_PENDING&&FAILED(hr))packageAttachments.erase(i);}packageRetryRounds=0;discoverPackage(pid);}
    // Progress the engines report per host (see hostStageSet in the engine), logged on every change.
    LONG seenStage[SlotCount]{-1,-1,-1,-1,-1,-1},seenError[SlotCount]{};
    void logHosts(){if(!status)return;for(int h=0;h<SlotCount;++h){LONG stage=status->hostStage[h],error=status->hostError[h];if(stage==seenStage[h]&&error==seenError[h])continue;seenStage[h]=stage;seenError[h]=error;
        auto at=dllPath.find_last_of(L"\\");auto file=dllPath.substr(0,at+1)+L"glass-controller.log";HANDLE f=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);if(f!=INVALID_HANDLE_VALUE){char line[96]{};int n=snprintf(line,sizeof(line),"host %d stage %ld error 0x%08lx\r\n",h,(long)stage,(unsigned long)error);DWORD written=0;WriteFile(f,line,n,&written,nullptr);CloseHandle(f);}}}
    std::set<DWORD> hookErrorPids;
    std::set<DWORD> threads;std::map<DWORD,std::wstring> processes;
    std::set<DWORD> attemptedThreads;unsigned packagedRounds=0;
    std::wstring dllPath,windowClass;bool failed=false;DWORD loadError=0;
    struct HostWatch {HANDLE process{},wait{};DWORD pid{};HWND window{};int slot=0;~HostWatch(){if(wait)UnregisterWaitEx(wait,INVALID_HANDLE_VALUE);if(process)CloseHandle(process);}};
    std::map<DWORD,std::unique_ptr<HostWatch>> hostWatches;unsigned recoveryRounds=0;
    static void CALLBACK hostEnded(void*context,BOOLEAN){auto watch=static_cast<HostWatch*>(context);PostMessageW(watch->window,HostExitedMessage,watch->pid,0);}
    void watchHosts(std::set<DWORD>const&ids){
        for(auto i=hostWatches.begin();i!=hostWatches.end();)if(!ids.count(i->first))i=hostWatches.erase(i);else ++i;
        for(auto pid:ids)if(!hostWatches.count(pid)&&hostWatches.size()<8){auto watch=std::make_unique<HostWatch>();watch->pid=pid;watch->slot=hostSlot(process(pid),false);watch->window=FindWindowW(endpoint,nullptr);watch->process=OpenProcess(SYNCHRONIZE,FALSE,pid);
            if(watch->process&&watch->window&&RegisterWaitForSingleObject(&watch->wait,watch->process,hostEnded,watch.get(),INFINITE,WT_EXECUTEONLYONCE))hostWatches.emplace(pid,std::move(watch));}
    }
    void trace(const char*stage,DWORD value){if(strcmp(stage,"packaged shell processes")==0)return;auto at=dllPath.find_last_of(L"\\");auto file=dllPath.substr(0,at+1)+L"glass-controller.log";HANDLE h=CreateFileW(file.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);if(h!=INVALID_HANDLE_VALUE){char line[120]{};int n=snprintf(line,sizeof(line),"%s: %lu\r\n",stage,(unsigned long)value);DWORD written=0;WriteFile(h,line,n,&written,nullptr);CloseHandle(h);}}
    std::wstring process(DWORD id){auto found=processes.find(id);if(found!=processes.end())return found->second;if(processes.size()>=128)processes.clear();wchar_t path[32768]{};DWORD size=32768;HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,id);if(p){QueryFullProcessImageNameW(p,0,path,&size);CloseHandle(p);}std::wstring value=path;auto at=value.find_last_of(L"\\");if(at!=std::wstring::npos)value.erase(0,at+1);return processes[id]=value;}
    bool target(HWND h){DWORD pid=0;GetWindowThreadProcessId(h,&pid);if(pid==GetCurrentProcessId())return false;auto exe=process(pid);wchar_t cls[128]{};GetClassNameW(h,cls,128);DWORD f=data?data->features:0;
        if((f&Keyboard)&&wcscmp(cls,L"Shell_InputSwitchTopLevelWindow")==0&&(!_wcsicmp(exe.c_str(),L"explorer.exe")||!_wcsicmp(exe.c_str(),L"ShellHost.exe")||!_wcsicmp(exe.c_str(),L"ShellExperienceHost.exe")))return true;
        if((f&Keyboard)&&!_wcsicmp(exe.c_str(),L"explorer.exe")&&(wcscmp(cls,L"XamlExplorerHostIslandWindow")==0||wcscmp(cls,L"XamlExplorerHostIslandWindow_WASDK")==0))return true;
        if(_wcsicmp(exe.c_str(),L"explorer.exe")==0){if((f&(Volume|Brightness|Taskbar|Dock|Capsule))&&(wcscmp(cls,L"Shell_TrayWnd")==0||wcscmp(cls,L"Shell_SecondaryTrayWnd")==0||wcscmp(cls,L"XamlExplorerHostIslandWindow")==0))return true;return (f&Explorer)&&(wcscmp(cls,L"CabinetWClass")==0||wcscmp(cls,L"ExploreWClass")==0);}
        return ((f&Start)&&startHost(exe))||((f&NotificationHostFeatures)&&notificationHostWindow(exe,cls));
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
    void attachThread(DWORD tid,HWND targetWindow=nullptr){if(threads.count(tid)||pendingHooks.count(tid)||attemptedThreads.count(tid)||pendingHooks.size()>=32)return;
        // Worker threads without a message queue make the hook fail with 87; skip them (not blacklisted, a queue can appear later).
        GUITHREADINFO gui{};gui.cbSize=sizeof(gui);if(!GetGUIThreadInfo(tid,&gui))return;
        attemptedThreads.insert(tid);HHOOK h=SetWindowsHookExW(WH_GETMESSAGE,messageHook,module,tid);if(!h){DWORD error=GetLastError(),pid=0;if(targetWindow)GetWindowThreadProcessId(targetWindow,&pid);else if(HANDLE t=OpenThread(THREAD_QUERY_LIMITED_INFORMATION,FALSE,tid)){pid=GetProcessIdOfThread(t);CloseHandle(t);}if(error!=ERROR_INVALID_PARAMETER&&hookErrorPids.insert(pid).second)trace("message hook error",error);return;}
        // As in the Styler's deferred ShellHost initialization, enqueue on the
        // real window so modal/window-filtered message loops also receive it.
        BOOL posted=targetWindow?PostMessageW(targetWindow,attach,GetCurrentProcessId(),0):PostThreadMessageW(tid,attach,GetCurrentProcessId(),0);
        if(!posted){trace("thread queue unavailable",GetLastError());UnhookWindowsHookEx(h);return;}pendingHooks[tid]=h;auto control=FindWindowW(endpoint,nullptr);if(control)SetTimer(control,5,2200,nullptr);}
    void discoverPackagedShell(){
        std::set<DWORD> ids;HANDLE p=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(p!=INVALID_HANDLE_VALUE){PROCESSENTRY32W e{};e.dwSize=sizeof(e);if(Process32FirstW(p,&e))do{bool start=(data->features&Start)&&startHost(e.szExeFile);bool notifications=(data->features&NotificationHostFeatures)&&(_wcsicmp(e.szExeFile,L"ShellExperienceHost.exe")==0||_wcsicmp(e.szExeFile,L"ShellHost.exe")==0);if(start||notifications)ids.insert(e.th32ProcessID);}while(Process32NextW(p,&e));CloseHandle(p);}
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
    // Shell processes pin the injected engine until they restart, so an older copy
    // can stay locked; delete what is free now and retry the rest on a later start.
    static void sweepEngines(const std::wstring&dir,const std::wstring&keep){WIN32_FIND_DATAW found{};HANDLE h=FindFirstFileW((dir+L"LiquidGlass.Engine-*.dll").c_str(),&found);if(h==INVALID_HANDLE_VALUE)return;do{auto path=dir+found.cFileName;if(_wcsicmp(path.c_str(),keep.c_str())!=0)DeleteFileW(path.c_str());}while(FindNextFileW(h,&found));FindClose(h);}
    bool initialize(const std::wstring&dir,bool nativeProbe=false){if(data)return true;endpoint=nativeProbe?L"LiquidGlass.Control.NativeProbe":L"LiquidGlass.Control";

        // Extract the exact engine embedded in this EXE. A versioned filename
        // allows an old shell session to release its DLL naturally on sign-out.
        auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(201),RT_RCDATA);if(!resource)return false;DWORD size=SizeofResource(nullptr,resource);auto bytes=LockResource(LoadResource(nullptr,resource));
        unsigned long long hash=14695981039346656037ull;for(DWORD i=0;i<size;++i){hash^=((const BYTE*)bytes)[i];hash*=1099511628211ull;}dllPath=dir+L"LiquidGlass.Engine-"+std::to_wstring(hash)+L".dll";
        sweepEngines(dir,dllPath);
        windowClass=threadClassName(dllPath);bool identical=false;HANDLE existing=CreateFileW(dllPath.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);if(existing!=INVALID_HANDLE_VALUE){LARGE_INTEGER length{};GetFileSizeEx(existing,&length);if(length.QuadPart==size){std::vector<BYTE> content(size);DWORD n=0;if(ReadFile(existing,content.data(),size,&n,nullptr)&&n==size)identical=memcmp(content.data(),bytes,size)==0;}CloseHandle(existing);}
        if(!identical){HANDLE file=CreateFileW(dllPath.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;DWORD n=0;BOOL ok=WriteFile(file,bytes,size,&n,nullptr);CloseHandle(file);if(!ok||n!=size)return false;}
        DWORD previousMode=0;BOOL changedMode=SetThreadErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX,&previousMode);
        module=LoadLibraryExW(dllPath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);loadError=module?0:GetLastError();
        if(changedMode)SetThreadErrorMode(previousMode,nullptr);
        if(!module){trace("engine load error",loadError);return false;}hook=(HOOKPROC)GetProcAddress(module,"GlassHook");messageHook=(HOOKPROC)GetProcAddress(module,"GlassMessageHook");if(!hook||!messageHook){trace("engine export error",GetLastError());return false;}
        auto name=mappingName(GetCurrentProcessId())+(nativeProbe?L".Probe":L"");mapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Config),name.c_str());if(!mapping)return false;data=(Config*)MapViewOfFile(mapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Config));if(!data)return false;Config initial;initial.owner=GetCurrentProcessId();initial.heartbeat=(LONG)GetTickCount();*data=initial;
        name=statusName(GetCurrentProcessId());statusMapping=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,0,sizeof(Status),name.c_str());if(!statusMapping)return false;status=(Status*)MapViewOfFile(statusMapping,FILE_MAP_ALL_ACCESS,0,0,sizeof(Status));if(!status)return false;Status initialStatus;initialStatus.owner=GetCurrentProcessId();*status=initialStatus;
        // Only the disposable status page is writable by a low-integrity host;
        // configuration remains read-only and at the controller's integrity.
        PSECURITY_DESCRIPTOR label{};if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"S:(ML;;NW;;;LW)",SDDL_REVISION_1,&label,nullptr))return false;PACL sacl{};BOOL present{},defaulted{};GetSecurityDescriptorSacl(label,&present,&sacl,&defaulted);DWORD labelError=SetSecurityInfo(statusMapping,SE_KERNEL_OBJECT,LABEL_SECURITY_INFORMATION,nullptr,nullptr,nullptr,sacl);LocalFree(label);if(labelError){trace("status page label error",labelError);return false;}
        attach=RegisterWindowMessageW(AttachMessage);refresh=RegisterWindowMessageW(RefreshMessage);return true;
    }
    bool available()const{return data&&hook;}
    bool policyBlocked()const{return loadError==4551||loadError==577||loadError==1260;}
    DWORD initializationError()const{return loadError;}
    bool hostExited(DWORD pid){auto watched=hostWatches.find(pid);if(watched==hostWatches.end())return false;int slot=watched->second->slot;hostWatches.erase(watched);packageAttachments.erase(pid);clearSlot(slot);hostHandles.erase(pid);prepareFailedAt.erase(pid);prepareLogged.erase(pid);hookErrorPids.erase(pid);threads.clear();attemptedThreads.clear();processes.erase(pid);recoveryRounds=0;packageRetryRounds=0;trace("shell host exited",pid);return true;}
    DWORD lastExplorer=0;
    // A new Explorer process starts with a clean progress slot, so a failed re-attach cannot keep showing the old "ok".
    void watchExplorer(){DWORD pid=0;if(HWND tray=FindWindowW(L"Shell_TrayWnd",nullptr))GetWindowThreadProcessId(tray,&pid);if(pid&&pid!=lastExplorer){if(lastExplorer){clearSlot(SlotExplorer);clearSlot(SlotModernExplorer);}lastExplorer=pid;}}
    void tick(){watchExplorer();logHosts();if(data)InterlockedExchange(&data->heartbeat,(LONG)GetTickCount());if(status&&data){DWORD success=(DWORD)status->success&data->features,fallback=(DWORD)status->fallback&success,failedBits=(DWORD)status->failed&data->features;LONG error=status->error;auto changes=statusCursor.take(success,fallback,failedBits,error,statusDirty);statusDirty=false;if(changes.success||changes.failed){auto control=FindWindowW(endpoint,nullptr);auto message=RegisterWindowMessageW(StatusMessage);for(DWORD bits=changes.success;bits;){DWORD bit=bits&(0u-bits);bits&=~bit;PostMessageW(control,message,bit,(fallback&bit)?S_FALSE:S_OK);}if(changes.failed)PostMessageW(control,message,changes.failed,error);}}
        if(status&&status->connected!=observedConnected){observedConnected=status->connected;trace("connected host mask",observedConnected);}if(status&&status->subscribed!=observedSubscribed){observedSubscribed=status->subscribed;trace("subscribed host mask",observedSubscribed);}
        for(auto&entry:packageAttachments){auto hr=entry.second->result.load();if(hr!=E_PENDING&&!entry.second->reported){entry.second->reported=true;trace("notification diagnostics result",(DWORD)hr);if(hr==HRESULT_FROM_WIN32(ERROR_NOT_FOUND)&&packageRetryRounds<15)SetTimer(FindWindowW(endpoint,nullptr),21,2000,nullptr);}}}
    void retryPackages(){if(!data||!(data->features&(Start|NotificationHostFeatures))||packageRetryRounds>=15)return;++packageRetryRounds;std::vector<DWORD> retry;for(auto const&entry:packageAttachments)if(entry.second->result==HRESULT_FROM_WIN32(ERROR_NOT_FOUND))retry.push_back(entry.first);for(auto pid:retry){packageAttachments.erase(pid);discoverPackage(pid);}}
    bool missingHost(){bool start=!(data->features&Start),notifications=!(data->features&NotificationHostFeatures);for(auto const&watch:hostWatches){auto exe=process(watch.first);if(startHost(exe))start=true;if(_wcsicmp(exe.c_str(),L"ShellExperienceHost.exe")==0||_wcsicmp(exe.c_str(),L"ShellHost.exe")==0)notifications=true;}return !start||!notifications;}
    // After the fast rounds, keep probing every 5 s (up to ~3 min) only while a wanted shell host is still not watched: a slow ShellExperienceHost or Start restart must be re-attached too.
    unsigned recoverHosts(){if(!data||!(data->features&(Start|NotificationHostFeatures)))return 0;discoverPackagedShell();++recoveryRounds;if(recoveryRounds<6)return 500;return recoveryRounds<132&&missingHost()?5000:0;}
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
    void publish(Config c){if(!data)return;statusDirty=true;InterlockedExchange(&data->heartbeat,(LONG)GetTickCount());c=normalize(c);if(!(c.features&(Start|NotificationHostFeatures|Keyboard)))hostWatches.clear();if(status){InterlockedAnd(&status->success,c.features);InterlockedAnd(&status->fallback,c.features);InterlockedAnd(&status->failed,c.features);}c.owner=GetCurrentProcessId();InterlockedIncrement(&data->sequence);MemoryBarrier();data->features=c.features;data->opacity=c.opacity;data->radius=c.radius;data->tint=c.tint;data->blur=c.blur;data->economy=c.economy;for(int f=0;f<FamilyCount;++f){data->looks[f].opacity=c.looks[f].opacity;data->looks[f].radius=c.looks[f].radius;data->looks[f].tint=c.looks[f].tint;data->looks[f].blur=c.looks[f].blur;}data->dockScale=c.dockScale;data->dockSpread=c.dockSpread;data->dockSpeed=c.dockSpeed;data->dockBounce=c.dockBounce;MemoryBarrier();InterlockedIncrement(&data->sequence);}
    // Ordinary updates never synchronously block the controller on shell threads. Shutdown still waits for restoration.
    void broadcast(bool wait=false){if(!data)return;HWND h=nullptr;while((h=FindWindowExW(HWND_MESSAGE,h,windowClass.c_str(),nullptr))){if(wait){DWORD_PTR value=0;SendMessageTimeoutW(h,refresh,0,0,SMTO_ABORTIFHUNG|SMTO_BLOCK,200,&value);}else PostMessageW(h,refresh,0,0);}}
    bool discover(){if(!data)return false;statusDirty=true;failed=false;threads.clear();processes.clear();attemptedThreads.clear();packagedRounds=0;packageRetryRounds=0;for(auto i=packageAttachments.begin();i!=packageAttachments.end();)if(i->second->result!=E_PENDING&&FAILED(i->second->result.load()))i=packageAttachments.erase(i);else ++i;if(data->features){EnumWindows(window,reinterpret_cast<LPARAM>(this));discoverPackagedShell();}broadcast();return !failed;}
    bool wantsWindow(HWND h){return data&&data->features&&target(h);}
    void discoverWindow(HWND h){if(!wantsWindow(h))return;DWORD pid=0;GetWindowThreadProcessId(h,&pid);wchar_t cls[128]{};GetClassNameW(h,cls,128);if(notificationHostWindow(process(pid),cls))rediscoverPackage(pid);attachWindow(h,true);EnumChildWindows(h,observedChild,reinterpret_cast<LPARAM>(this));}
    // Short per-host state for the UI status line, e.g. "Start ok · Notification center: no connection (5)".
    std::wstring hostSummary()const{if(!status)return L"";static const wchar_t*names[SlotCount]={L"Taskbar",L"Start",L"Search",L"Notification center",L"Shell host",L"File Explorer"};std::wstring out;
        for(int h=0;h<SlotCount;++h){LONG stage=status->hostStage[h],error=status->hostError[h];if(stage>=2)error=0;if(!stage&&!error&&h!=SlotStart&&h!=SlotShellExperience)continue;std::wstring text;wchar_t code[24]{};
            if(HRESULT_FACILITY(error)==FACILITY_WIN32||(error>=0&&error<65536))swprintf(code,24,L"%lu",(unsigned long)(error&0xffff));else swprintf(code,24,L"0x%08lx",(unsigned long)error);
            if(stage>=4)text=std::wstring(names[h])+L" ok";else if(error)text=std::wstring(names[h])+L": no connection ("+code+L")";else text=std::wstring(names[h])+(stage==3?L": matched":stage==2?L": connected":stage==1?L": attaching":L": waiting");
            if(!out.empty())out+=L" · ";out+=text;}return out;}
    LONG dockIcons()const{return status?status->dockIcons:0;}
    LONG dockBound()const{return status?status->dockBound:0;}
    LONG dockPointer()const{return status?status->dockPointer:0;}
    unsigned sessions()const{unsigned count=0;HWND h=nullptr;while((h=FindWindowExW(HWND_MESSAGE,h,windowClass.c_str(),nullptr)))++count;return count;}
    void shutdown(){hostWatches.clear();packageAttachments.clear();expireHooks();if(data){Config c=*data;c.features=0;publish(c);broadcast(true);InterlockedExchange(&data->heartbeat,(LONG)(GetTickCount()-5000));UnmapViewOfFile(data);data=nullptr;}if(mapping){CloseHandle(mapping);mapping=nullptr;}if(status){UnmapViewOfFile(status);status=nullptr;}if(statusMapping){CloseHandle(statusMapping);statusMapping=nullptr;}if(module){FreeLibrary(module);module=nullptr;}threads.clear();processes.clear();}
};
}
