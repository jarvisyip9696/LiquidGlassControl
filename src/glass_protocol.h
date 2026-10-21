#pragma once
#include <windows.h>
#include <string>
namespace glass {
constexpr DWORD Magic=0x4c474531, Version=4;
inline std::wstring runtimeName(const std::wstring&path){auto at=path.find_last_of(L"\\/");return path.substr(at==std::wstring::npos?0:at+1);}
inline std::wstring threadClassName(const std::wstring&path){return L"LiquidGlass.Engine.Thread.v2."+runtimeName(path);}
constexpr wchar_t AttachMessage[]=L"LiquidGlass.Engine.Attach.v2";
constexpr wchar_t RefreshMessage[]=L"LiquidGlass.Engine.Refresh.v2";
constexpr wchar_t StatusMessage[]=L"LiquidGlass.Engine.Status.v2";
constexpr wchar_t AttachAckMessage[]=L"LiquidGlass.Engine.AttachAck.v2";
constexpr CLSID TapId={0x973361c7,0xcbe9,0x4da4,{0x9c,0x96,0x3b,0x70,0x18,0x3b,0xa1,0x62}};
inline CLSID runtimeTapId(const std::wstring&path){DWORD hash=2166136261u;for(auto c:runtimeName(path)){hash^=(DWORD)c;hash*=16777619u;}auto id=TapId;id.Data1^=hash;return id;}
// Capsule reshapes the taskbar's visible background into a centered pill; unlike the material features it changes the taskbar's visual layout.
enum Feature:DWORD {Volume=1,Brightness=2,Taskbar=4,Start=8,Notifications=16,Explorer=32,Dock=64,Capsule=128,Keyboard=256,Calendar=512,Toasts=1024};
// ShellExperienceHost serves the notification center, the calendar and the toasts: any of these bits wants it.
constexpr DWORD NotificationHostFeatures=Notifications|Calendar|Toasts;
// Each shell effect family has its own look; the shared Config fields stay as the legacy default the per-family values fall back to.
enum Family {TaskbarLook,StartLook,NotificationLook,CalendarLook,ExplorerLook,FamilyCount};
struct Look {DWORD opacity=35,radius=12,tint=0,blur=12;};
// Notification Center Styler 1.7 initializes these hosts only once their
// actual panel window exists. ShellHost's hidden CoreWindow is not ready.
inline bool notificationHostWindow(const std::wstring&exe,const std::wstring&cls){
    return (!_wcsicmp(exe.c_str(),L"ShellExperienceHost.exe")&&!_wcsicmp(cls.c_str(),L"Windows.UI.Core.CoreWindow"))||
        (!_wcsicmp(exe.c_str(),L"ShellHost.exe")&&!_wcsicmp(cls.c_str(),L"ControlCenterWindow"));
}
// One taskbar switch owns both material and geometry; accept either legacy bit on load.
inline bool startHost(const std::wstring&exe){return !_wcsicmp(exe.c_str(),L"StartMenuExperienceHost.exe")||!_wcsicmp(exe.c_str(),L"SearchHost.exe")||!_wcsicmp(exe.c_str(),L"SearchApp.exe");}
inline DWORD unifiedTaskbar(DWORD features){return (features&(Taskbar|Capsule))?features|Taskbar|Capsule:features;}
struct Config {
    DWORD magic=Magic,version=Version,owner=0;volatile LONG sequence=0;
    DWORD features=0,opacity=35,radius=12,tint=0,blur=12;BOOL economy=FALSE;
    volatile LONG heartbeat=0;
    // Dock: scale is the peak magnification in percent (126 = +26%), spread the hover radius as a percent of the icon width (135 = 1.35 x width), speed 0..100 (50 = 150 ms ease; each point shifts 3 ms), bounce 0..100 (30 and below = no overshoot; above adds an overshoot on the ease).
    Look looks[FamilyCount];DWORD dockScale=126,dockSpread=135,dockSpeed=50,dockBounce=30;
};
inline std::wstring mappingName(DWORD owner){return L"Local\\LiquidGlass.Engine.Config.v4."+std::to_wstring(owner);}
inline std::wstring statusName(DWORD owner){return L"Local\\LiquidGlass.Engine.Status.v4."+std::to_wstring(owner);}
struct RootStatus {volatile LONG width=0,height=0,visible=0,brush=0,error=0,brushWhenVisible=0,visibleSamples=0;};
// One progress slot per attach target, so hosts never share (or inherit) each other's stage and error.
enum HostSlot {SlotExplorer,SlotStart,SlotSearch,SlotShellExperience,SlotShellHost,SlotModernExplorer,SlotCount};
inline int hostSlot(const std::wstring&exe,bool modern){if(!_wcsicmp(exe.c_str(),L"StartMenuExperienceHost.exe"))return SlotStart;if(!_wcsicmp(exe.c_str(),L"SearchHost.exe")||!_wcsicmp(exe.c_str(),L"SearchApp.exe"))return SlotSearch;if(!_wcsicmp(exe.c_str(),L"ShellExperienceHost.exe"))return SlotShellExperience;if(!_wcsicmp(exe.c_str(),L"ShellHost.exe"))return SlotShellHost;return modern?SlotModernExplorer:SlotExplorer;}
struct Status {DWORD magic=Magic,version=Version,owner=0;volatile LONG success=0,fallback=0,failed=0,error=0,connected=0,subscribed=0;RootStatus roots[2];volatile LONG hostStage[SlotCount]{},hostError[SlotCount]{},dockIcons=0,dockBound=0,dockPointer=0,dockLastX=0;};
struct StatusChanges {DWORD success=0,failed=0;};
struct StatusCursor {
    DWORD success=0,fallback=0,failed=0;LONG error=0;
    StatusChanges take(DWORD nextSuccess,DWORD nextFallback,DWORD nextFailed,LONG nextError,bool force=false){
        StatusChanges out{force?nextSuccess:nextSuccess&((nextSuccess^success)|(nextFallback^fallback)),nextFailed&&(force||nextFailed!=failed||nextError!=error)?nextFailed:0};
        success=nextSuccess;fallback=nextFallback;failed=nextFailed;error=nextError;return out;
    }
};
inline bool alive(volatile Config*config,DWORD now=GetTickCount()){return config&&DWORD(now-DWORD(config->heartbeat))<5000;}
inline DWORD bounded(DWORD n,DWORD lo,DWORD hi){return n<lo?lo:n>hi?hi:n;}
// Seqlock reader for the config the controller publishes. The writer keeps the sequence odd while publishing. Ok: consistent copy. Transient: still publishing or torn after the bounded attempts. Invalid: consistent copy with a foreign header.
enum class ReadState {Ok,Transient,Invalid};
inline ReadState readSnapshot(volatile Config*shared,Config&c,int attempts=16){
    for(int i=0;i<attempts;++i){
        LONG s=shared->sequence;if(s&1){YieldProcessor();continue;}
        // Copy field by field: a volatile source cannot be assigned to a plain Config.
        Config copy{};MemoryBarrier();copy.magic=shared->magic;copy.version=shared->version;copy.owner=shared->owner;copy.features=shared->features;copy.opacity=shared->opacity;copy.radius=shared->radius;copy.tint=shared->tint;copy.blur=shared->blur;copy.economy=shared->economy;
        for(int f=0;f<FamilyCount;++f){copy.looks[f].opacity=shared->looks[f].opacity;copy.looks[f].radius=shared->looks[f].radius;copy.looks[f].tint=shared->looks[f].tint;copy.looks[f].blur=shared->looks[f].blur;}
        copy.dockScale=shared->dockScale;copy.dockSpread=shared->dockSpread;copy.dockSpeed=shared->dockSpeed;copy.dockBounce=shared->dockBounce;MemoryBarrier();
        if(s==shared->sequence){copy.sequence=s;c=copy;return c.magic==Magic&&c.version==Version?ReadState::Ok:ReadState::Invalid;}
    }
    return ReadState::Transient;
}
// Regression check for the reader above, run by --self-test: a consistent config is accepted, a writer that never finishes is Transient (bounded), and a foreign header is Invalid.
inline bool selfTestSnapshot(){
    Config source;source.features=Taskbar|Dock;source.blur=20;source.looks[CalendarLook].opacity=61;source.dockSpread=170;Config out;
    source.heartbeat=100;if(!alive(&source,5099)||alive(&source,5100))return false;
    source.heartbeat=(LONG)0xfffffff0u;if(!alive(&source,16)||alive(&source,6000))return false;
    if(readSnapshot((volatile Config*)&source,out)!=ReadState::Ok||out.features!=(Taskbar|Dock)||out.blur!=20||out.looks[CalendarLook].opacity!=61||out.looks[TaskbarLook].opacity!=35||out.dockSpread!=170||out.dockScale!=126)return false;
    source.sequence=6;if(readSnapshot((volatile Config*)&source,out)!=ReadState::Ok||out.sequence!=6)return false;
    source.sequence=1;if(readSnapshot((volatile Config*)&source,out,4)!=ReadState::Transient)return false;
    source.sequence=0;source.magic=0;return readSnapshot((volatile Config*)&source,out)==ReadState::Invalid;
}
inline Look normalize(Look l){l.blur=bounded(l.blur,0,40);l.opacity=bounded(l.opacity,5,90);l.radius=bounded(l.radius,0,28);l.tint=bounded(l.tint,0,3);return l;}
inline Config normalize(Config c){c.features&=2047;c.blur=bounded(c.blur,0,40);c.opacity=bounded(c.opacity,5,90);c.radius=bounded(c.radius,0,28);c.tint=bounded(c.tint,0,3);for(auto&l:c.looks)l=normalize(l);c.dockScale=bounded(c.dockScale,100,200);c.dockSpread=bounded(c.dockSpread,50,300);c.dockSpeed=bounded(c.dockSpeed,0,100);c.dockBounce=bounded(c.dockBounce,0,100);return c;}
// Names only identify material surfaces. No width, height, padding, alignment,
// taskbar position or system work-area property is ever written by this engine.
enum Surface {NoSurface,TaskbarFill,OsdPlate,VolumePlate,BrightnessPlate,StartPlate,StartOverlay,NotificationPlate,ExplorerPlate,TaskbarRoot,TaskbarOverlay,StartClear,NotificationClear,ExplorerClear,DockRoot,DockIcon,TaskbarAuxiliary,NotificationAuxiliary,KeyboardPlate,CalendarPlate,CalendarAuxiliary,ToastPlate};
inline Family family(Surface s){switch(s){case StartClear:case StartOverlay:case StartPlate:return StartLook;case NotificationClear:case NotificationPlate:case NotificationAuxiliary:case ToastPlate:return NotificationLook;case CalendarPlate:case CalendarAuxiliary:return CalendarLook;case ExplorerClear:case ExplorerPlate:return ExplorerLook;default:return TaskbarLook;}}
inline DWORD feature(Surface s){switch(s){case CalendarPlate:case CalendarAuxiliary:return Calendar;case ToastPlate:return Toasts;case KeyboardPlate:return Keyboard;case TaskbarRoot:case TaskbarOverlay:case TaskbarFill:case TaskbarAuxiliary:return Taskbar;case VolumePlate:return Volume;case BrightnessPlate:return Brightness;case StartClear:case StartOverlay:case StartPlate:return Start;case NotificationClear:case NotificationPlate:return Notifications;case ExplorerClear:case ExplorerPlate:return Explorer;case DockRoot:case DockIcon:return Dock;default:return 0;}}
inline Surface identify(const std::wstring&name,const std::wstring&type,int host){
    if((host==0||host==3)&&name==L"FlatContent"&&type.find(L"ContentControl")!=std::wstring::npos)return KeyboardPlate;
    if(host==0){if(type==L"Taskbar.TaskbarFrame")return DockRoot;if(type==L"Taskbar.TaskListButton"||type==L"Taskbar.TaskListLabeledButton")return DockIcon;if(name==L"SystemTrayFrameGrid")return TaskbarRoot;if(name==L"BackgroundStroke"&&type.find(L"Rectangle")!=std::wstring::npos)return TaskbarOverlay;
        if(name==L"BackgroundFill"&&type.find(L"Rectangle")!=std::wstring::npos)return TaskbarFill;
        if(name==L"ConfirmatorMainGrid")return OsdPlate;
        if(name==L"VolumeConfirmator")return VolumePlate;
        if(name==L"BrightnessConfirmator")return BrightnessPlate;
    }
    if(host==1){if(name==L"RootGrid"&&type.find(L"Grid")!=std::wstring::npos)return StartClear;if(name==L"AcrylicOverlay")return StartOverlay;if(name==L"AcrylicBorder"||name==L"AppBorder")return StartPlate;}
    if(host==2&&(name==L"PageContent"||name==L"FullScreenPageRoot"))return NotificationClear;
    if(host==2&&(name==L"NotificationCenterGrid"))return NotificationPlate;
    if(host==2&&name==L"CalendarCenterGrid")return CalendarPlate;
    if(host==2&&(name==L"CalendarControlScrollViewer"||name==L"CalendarHeaderMinimizedOverlay"))return CalendarAuxiliary;
    if(host==2&&(name==L"ToastBackgroundBorder"||name==L"ToastBackgroundBorder2"))return ToastPlate;
    if(host==2&&(name==L"ControlCenterGrid"||name==L"ControlCenterRegion"||name==L"NotificationCenterTopBanner"||name==L"ListContent"))return NotificationAuxiliary;
    // PART_LayoutRoot is a common template part name; like Explorer Styler, clear only the Grid one.
    if(host==3&&((name==L"PART_LayoutRoot"&&type==L"Microsoft.UI.Xaml.Controls.Grid")||name==L"RootContainer"||name==L"NavigationBarControlGrid"||name==L"DetailsViewControlRootGrid"||name==L"GalleryRootGrid"))return ExplorerClear;
    if(host==3&&(name==L"FileExplorerAddressBarGrid"||name==L"FileExplorerSearchBox"||name==L"CommandBarControlRootGrid"||name==L"FileExplorerCommandBar"||name==L"NavigationBarControlGrid"||name==L"HomeViewRootGrid"||name==L"GalleryRootGrid"||name==L"DetailsViewControlRootGrid"||name==L"GalleryViewControl"))return ExplorerPlate;
    return NoSurface;
}
inline bool active(Surface s,DWORD f){switch(s){case CalendarPlate:case CalendarAuxiliary:return (f&Calendar)!=0;case ToastPlate:return (f&Toasts)!=0;case KeyboardPlate:return (f&Keyboard)!=0;case TaskbarAuxiliary:return (f&Taskbar)!=0;case TaskbarRoot:case TaskbarOverlay:return (f&(Taskbar|Capsule))!=0;case TaskbarFill:return (f&(Taskbar|Capsule))!=0;case OsdPlate:return (f&(Volume|Brightness))!=0;case VolumePlate:return (f&Volume)!=0;case BrightnessPlate:return (f&Brightness)!=0;case StartClear:case StartPlate:case StartOverlay:return (f&Start)!=0;case NotificationClear:case NotificationPlate:case NotificationAuxiliary:return (f&Notifications)!=0;case ExplorerClear:case ExplorerPlate:return (f&Explorer)!=0;case DockRoot:case DockIcon:return (f&Dock)!=0;default:return false;}}
}
