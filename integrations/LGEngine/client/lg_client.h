#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
// C++17 client for LGEngine protocol 1.1 (major and frame version 1; falls back to 1.0 against a host that does not
// answer with a minor), evaluated at dabe4cb and extended for the 1.1 messages (types 10..17, event 0x24).
// Screen capture, glass rendering and glass windows belong exclusively to the host.
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace lghost {
struct Handle {
    HANDLE value{};Handle()=default;explicit Handle(HANDLE h):value(h==INVALID_HANDLE_VALUE?nullptr:h){}
    ~Handle(){reset();}void reset(HANDLE h=nullptr){if(value)CloseHandle(value);value=h==INVALID_HANDLE_VALUE?nullptr:h;}
    Handle(const Handle&)=delete;Handle&operator=(const Handle&)=delete;
};
struct Look {int dark=35,blur=30,refraction=80,rim=60,shadow=28,round=12,intensity=100,elevation=50;};
inline bool operator==(const Look&a,const Look&b){return a.dark==b.dark&&a.blur==b.blur&&a.refraction==b.refraction&&a.rim==b.rim&&a.shadow==b.shadow&&a.round==b.round&&a.intensity==b.intensity&&a.elevation==b.elevation;}
struct Spec {
    int width=420,height=200,x=100,y=100,kind=0;float scale=1;
    bool adapt=true,hover=true,savePower=true,inShots=false;
    int fps=15,motion=4,inMs=380,outMs=230;int zoom=10,liquid=60,bounce=45,travel=-1,indicator=-1;Look look;
    std::string label="LiquidGlassControl.Host",profile;
};
// A glass button for SetButtons: hit box, glow centre/half sizes/radius, style and mark, and an optional alpha mask
// (maskBox.w*maskBox.h bytes, or empty). Rects are left/top/right/bottom here and x,y,w,h on the wire.
struct Button {RECT hit{};float cx=0,cy=0,hx=0,hy=0,rad=0;int style=0,mark=0;RECT maskBox{};std::vector<uint8_t> mask;};
enum Event : unsigned {Connected=1,Lost=2,Restarted=3,Shown=4,Hidden=5,Appearance=6,Failed=7,LightMoved=8};
struct Options {std::wstring hostPath,suffix;HWND notify{};UINT message=WM_APP+60;};
// LGEngineHost-<sha256 of the file>.exe, as stage-host.ps1 names it.
inline bool stagedHostName(const std::wstring&file){
    bool safe=file.size()==81&&file.substr(0,13)==L"LGEngineHost-"&&file.substr(77)==L".exe";
    for(size_t i=13;safe&&i<77;++i)if(!((file[i]>=L'0'&&file[i]<=L'9')||(file[i]>=L'a'&&file[i]<=L'f')))safe=false;
    return safe;
}
inline std::wstring bundledHostPath(const std::wstring&directory){
    wchar_t name[128]{};GetPrivateProfileStringW(L"Host",L"Executable",L"",name,128,(directory+L"host-current.ini").c_str());
    std::wstring file=name;
    return directory+(stagedHostName(file)?file:L"LGEngineHost.exe");
}
// %LOCALAPPDATA%\LGEngine\host\, or empty when it cannot be made.
inline std::wstring hostHome(){
    wchar_t base[MAX_PATH]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);if(!n||n>=MAX_PATH)return {};
    std::wstring dir=std::wstring(base)+L"\\LGEngine\\";CreateDirectoryW(dir.c_str(),nullptr);dir+=L"host\\";
    if(!CreateDirectoryW(dir.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return {};return dir;
}
// The file at path hashes to the SHA-256 its staged name carries.
inline bool hostFileMatches(const std::wstring&path,const std::wstring&file){
    Handle f(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr));LARGE_INTEGER size{};
    if(!f.value||!GetFileSizeEx(f.value,&size)||size.QuadPart<=0||size.QuadPart>(64<<20))return false;
    std::vector<unsigned char> bytes(size_t(size.QuadPart));DWORD got=0;if(!ReadFile(f.value,bytes.data(),DWORD(bytes.size()),&got,nullptr)||got!=bytes.size())return false;
    unsigned char digest[32]{};if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,bytes.data(),ULONG(bytes.size()),digest,32)<0)return false;
    wchar_t hex[65]{};for(int i=0;i<32;++i)swprintf(hex+i*2,3,L"%02x",digest[i]);return file.compare(13,64,hex)==0;
}
// The shared host outlives the app that started it and serves every later version, so run from an app folder it kept that folder
// locked while any version ran. It runs from one verified copy per build in hostHome() instead (CopyFileW keeps the download's
// Mark of the Web, so Smart App Control judges the copy exactly as the original). Any failure runs the bundled file as before.
inline std::wstring hostCopy(const std::wstring&hostPath,const std::wstring&home){
    auto slash=hostPath.find_last_of(L"\\");if(home.empty()||slash==std::wstring::npos)return hostPath;
    auto file=hostPath.substr(slash+1);if(!stagedHostName(file))return hostPath;
    auto target=home+file;if(hostFileMatches(target,file))return target;
    auto temp=target+L"."+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetCurrentThreadId())+L".tmp";
    bool ok=CopyFileW(hostPath.c_str(),temp.c_str(),FALSE)&&hostFileMatches(temp,file)&&MoveFileExW(temp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING);
    if(!ok)DeleteFileW(temp.c_str());
    // Other builds' copies go once their hosts have exited (a running one refuses the delete and is tried again next time).
    WIN32_FIND_DATAW found{};HANDLE find=FindFirstFileW((home+L"LGEngineHost-*").c_str(),&found);
    if(find!=INVALID_HANDLE_VALUE){do{if(_wcsnicmp(found.cFileName,file.c_str(),file.size())!=0)DeleteFileW((home+found.cFileName).c_str());}while(FindNextFileW(find,&found));FindClose(find);}
    return ok||hostFileMatches(target,file)?target:hostPath;
}
namespace wire {
const unsigned kMinor=1; // the highest protocol minor this client speaks
enum Msg : unsigned {Hello=1,HelloAck=2,Create=3,CreateAck=4,Destroy=5,SetLook=6,Resize=7,SetMotionEx=10,SetSlosh=11,Kick=12,SetButtons=13,SetPointerArea=14,SetUpdates=15,SetBusy=16,Cut=17};
struct Writer {
    std::vector<unsigned char> bytes;
    void u8(unsigned n){bytes.push_back(static_cast<unsigned char>(n));}
    void u16(unsigned n){u8(n);u8(n>>8);}void u32(uint32_t n){for(int i=0;i<4;++i)u8(n>>(8*i));}
    void i32(int n){u32(static_cast<uint32_t>(n));}
    void f32(float f){uint32_t n;memcpy(&n,&f,4);u32(n);}
    void raw(const void*p,size_t n){auto c=static_cast<const unsigned char*>(p);bytes.insert(bytes.end(),c,c+n);}
    void str(const std::string&s){if(s.size()>60000)throw std::invalid_argument("string too long");u16(unsigned(s.size()));bytes.insert(bytes.end(),s.begin(),s.end());}
};
inline uint32_t get(const unsigned char*p){uint32_t n;memcpy(&n,p,4);return n;}
struct Reader {
    const std::vector<unsigned char>&b;size_t at=16;bool bad=false;
    bool need(size_t n){if(at>b.size()||n>b.size()-at){bad=true;return false;}return true;}
    unsigned u8(){return need(1)?b[at++]:0;}unsigned u16(){unsigned n=u8();return n|(u8()<<8);}
    uint32_t u32(){if(!need(4))return 0;auto n=get(b.data()+at);at+=4;return n;}
    float f32(){uint32_t n=u32();float f;memcpy(&f,&n,4);return f;}
    std::string str(size_t max){unsigned n=u16();if(n>max||!need(n)){bad=true;return {};}
        std::string s(reinterpret_cast<const char*>(b.data()+at),n);at+=n;return s;}
};
inline bool frame(const std::vector<unsigned char>&b){return b.size()>=16&&get(b.data())==0x3145474c&&b[4]==1&&b[5]==0&&get(b.data()+8)<=65536&&get(b.data()+8)==b.size()-16;}
inline std::vector<unsigned char> frame(unsigned type,unsigned seq,const Writer&p){Writer w;w.u32(0x3145474c);w.u16(1);w.u16(type);w.u32(unsigned(p.bytes.size()));w.u32(seq);w.bytes.insert(w.bytes.end(),p.bytes.begin(),p.bytes.end());return w.bytes;}
inline void look(Writer&w,Look l){for(int n:{l.dark,l.blur,l.refraction,l.rim,l.shadow,l.round,l.intensity,l.elevation})w.i32(n);}
inline void rect(Writer&w,const RECT&r){w.i32(int(r.left));w.i32(int(r.top));w.i32(int(r.right-r.left));w.i32(int(r.bottom-r.top));}
// Hello: u16 min, u16 max, str name, u16 clientMinor (a 1.0 host ignores the trailing bytes).
inline Writer hello(unsigned minor=kMinor){Writer w;w.u16(1);w.u16(1);w.str("LiquidGlassControl C++");w.u16(minor);return w;}
inline Writer create(unsigned cookie,const Spec&s){Writer w;w.u32(cookie);w.u8(s.kind);w.f32(s.scale);w.u32(s.width);w.u32(s.height);w.u32(s.x);w.u32(s.y);
    w.u8((s.inShots?1:0)|(s.savePower?2:0)|(s.adapt?4:0)|(s.hover?8:0));w.u32(s.fps);w.u32(s.motion);w.u32(s.inMs);w.u32(s.outMs);w.i32(s.zoom);w.i32(s.liquid);w.i32(s.bounce);look(w,s.look);w.str(s.label);w.str(s.profile);return w;}
inline Writer destroy(unsigned id){Writer w;w.u32(id);return w;}
inline Writer setLook(unsigned id,const Look&l,const std::string&profile){Writer w;w.u32(id);look(w,l);w.str(profile);return w;}
inline Writer resize(unsigned id,int width,int height,unsigned flags=0){Writer w;w.u32(id);w.i32(width);w.i32(height);w.u32(flags);return w;} // flags bit0: flow (minor >= 1)
inline Writer setMotionEx(unsigned id,int zoom,int liquid,int bounce,int travel,int indicator){Writer w;w.u32(id);for(int n:{zoom,liquid,bounce,travel,indicator})w.i32(n);return w;}
inline Writer setSlosh(unsigned id,int style,int amount){Writer w;w.u32(id);w.i32(style);w.i32(amount);return w;}
inline Writer kick(unsigned id,float push){Writer w;w.u32(id);w.f32(push);return w;}
// 0 = no kick (no slosh amount, or style 3 = none). A pulse (style 2) always pushes the same way.
inline float kickPush(int style,int amount,int way){if(amount<=0||style<0||style>=3)return 0;if(style==2)way=1;return float(way)*4.4f*float(amount)/60.f;}
inline bool buttonsValid(const std::vector<Button>&list){
    if(list.size()>32)return false;
    auto num=[](float v,bool nonNegative){return std::isfinite(v)&&std::fabs(v)<=16384.f&&(!nonNegative||v>=0);};
    auto box=[](const RECT&r,long long&area){long long w=(long long)r.right-r.left,h=(long long)r.bottom-r.top;area=w*h;
        return w>=0&&w<=4096&&h>=0&&h<=4096&&r.left>=-4096&&r.left<=8192&&r.top>=-4096&&r.top<=8192;};
    for(auto&b:list){long long a1=0,a2=0;
        if(!num(b.cx,false)||!num(b.cy,false)||!num(b.hx,true)||!num(b.hy,true)||!num(b.rad,true)||!box(b.hit,a1)||!box(b.maskBox,a2))return false;
        if(!b.mask.empty()&&(long long)b.mask.size()!=a2)return false;}
    return true;
}
// The whole frame must fit 64 KB: when the masks would not, every mask is dropped (the boxes stay).
inline Writer setButtons(unsigned id,const std::vector<Button>&list,bool carry){
    size_t size=7;for(auto&b:list)size+=64+b.mask.size();bool masks=size<=65536;
    Writer w;w.u32(id);w.u8(carry?1:0);w.u16(unsigned(list.size()));
    for(auto&b:list){rect(w,b.hit);w.f32(b.cx);w.f32(b.cy);w.f32(b.hx);w.f32(b.hy);w.f32(b.rad);w.i32(b.style);w.i32(b.mark);rect(w,b.maskBox);
        if(masks&&!b.mask.empty()){w.u32(uint32_t(b.mask.size()));w.raw(b.mask.data(),b.mask.size());}else w.u32(0);}
    return w;
}
inline Writer setPointerArea(unsigned id,const RECT&r){Writer w;w.u32(id);rect(w,r);return w;}
inline Writer setUpdates(unsigned id,int fps,bool adapt,bool savePower){Writer w;w.u32(id);w.i32(fps);w.u8(adapt?1:0);w.u8(savePower?1:0);return w;}
inline Writer setBusy(unsigned id,bool busy){Writer w;w.u32(id);w.u8(busy?1:0);return w;}
inline Writer cut(unsigned id){Writer w;w.u32(id);return w;}
// Cheap structural mirror of the host's profile check (the preset names live in the host, so a bare name is allowed): empty, a bare
// preset name, or "key=value;..." text that has a top= part. Never contains NUL; at most 2400 chars (bytes here, which is stricter).
inline bool profileOk(const std::string&p){
    if(p.size()>2400||p.find('\0')!=std::string::npos)return false;
    size_t a=p.find_first_not_of(" \t\r\n");if(a==std::string::npos)return true;
    if(p.find('=')==std::string::npos)return true;
    size_t at=0;while(at<=p.size()){size_t end=p.find(';',at);if(end==std::string::npos)end=p.size();size_t eq=p.find('=',at);
        if(eq!=std::string::npos&&eq<end){size_t k=p.find_first_not_of(" \t\r\n",at);size_t e=p.find_last_not_of(" \t\r\n",eq-1);
            if(k!=std::string::npos&&e!=std::string::npos&&e>=k&&p.compare(k,e-k+1,"top")==0)return true;}
        at=end+1;}
    return false;
}
inline bool valid(const Spec&s){auto range=[](int n,int lo,int hi){return n>=lo&&n<=hi;};
    return range(s.width,1,4096)&&range(s.height,1,4096)&&uint64_t(s.width)*s.height<=4000000&&
        range(s.x,-100000,100000)&&range(s.y,-100000,100000)&&s.scale>=.01f&&s.scale<=8&&
        range(s.kind,0,1)&&range(s.fps,0,60)&&range(s.motion,0,8)&&range(s.inMs,60,1200)&&range(s.outMs,60,1200)&&
        range(s.zoom,0,50)&&range(s.liquid,0,100)&&range(s.bounce,0,100)&&range(s.travel,-1,160)&&range(s.indicator,-1,8)&&
        !s.label.empty()&&s.label.size()<=256&&profileOk(s.profile)&&
        range(s.look.dark,0,100)&&range(s.look.blur,0,100)&&range(s.look.refraction,0,100)&&range(s.look.rim,0,100)&&range(s.look.shadow,0,100)&&range(s.look.round,0,28)&&range(s.look.intensity,0,150)&&range(s.look.elevation,0,100);}
// A change that needs destroy + create: kind, scale, label and inShots are fixed at Create; a host without 1.1 (minor 0) also
// takes fps/adapt/savePower and zoom/liquid/bounce only at Create. Everything else is sent in place.
inline bool needsCreate(const Spec&a,const Spec&b,unsigned hostMinor){
    if(a.kind!=b.kind||a.scale!=b.scale||a.label!=b.label||a.inShots!=b.inShots)return true;
    return hostMinor<1&&(a.fps!=b.fps||a.adapt!=b.adapt||a.savePower!=b.savePower||a.zoom!=b.zoom||a.liquid!=b.liquid||a.bounce!=b.bounce);
}
// Reconnect delay: 100, 400, 1000, 2000 ms, then doubling, capped at 60 s; rnd picks -25%..+25% (cap applied after the jitter).
inline unsigned backoffMs(unsigned attempt,uint32_t rnd){
    static const unsigned table[4]={100,400,1000,2000};
    uint64_t base=attempt<4?table[attempt]:std::min<uint64_t>(60000,uint64_t(2000)<<std::min(attempt-3,6u));
    return unsigned(std::min<uint64_t>(60000,base*(75+rnd%51)/100));
}
// Double-buffer dirty plan. The host copies only the descriptor's dirty rect when the publish seq steps by 1, so every published
// buffer must hold the whole current frame. A buffer lacks what changed since it was last written: the back buffer is behind by
// the previous publish's dirty rect plus this one's. next() returns what to copy into `back` (clipped to the card; empty = nothing).
inline bool rectEmpty(const RECT&r){return r.right<=r.left||r.bottom<=r.top;}
inline RECT rectUnion(const RECT&a,const RECT&b){if(rectEmpty(a))return b;if(rectEmpty(b))return a;
    return RECT{std::min(a.left,b.left),std::min(a.top,b.top),std::max(a.right,b.right),std::max(a.bottom,b.bottom)};}
inline RECT rectClip(const RECT&a,const RECT&b){RECT r{std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};return rectEmpty(r)?RECT{}:r;}
struct DirtyPlan {
    RECT lacks[2]{};
    void reset(int w,int h){lacks[0]=lacks[1]=RECT{0,0,w,h};} // both buffers blank (new section)
    RECT next(RECT dirty,int back,int w,int h){RECT card{0,0,w,h};dirty=rectClip(dirty,card);if(rectEmpty(dirty))return RECT{};
        lacks[0]=rectUnion(lacks[0],dirty);lacks[1]=rectUnion(lacks[1],dirty);
        RECT copy=rectClip(lacks[back&1],card);lacks[back&1]=RECT{};return copy;}
};
}
// One retained surface per connection. Multiple clients share the same host.
// All pipe I/O is on the worker. Setters coalesce under a short mutex.
class Client {
    enum Want : unsigned {WSlosh=1,WKick=2,WButtons=4,WPointer=8,WBusy=16,WCut=32};
    Options options;Handle stop{CreateEventW(nullptr,TRUE,FALSE,nullptr)},changed{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    Handle wrEv{CreateEventW(nullptr,TRUE,FALSE,nullptr)},rdEv{CreateEventW(nullptr,TRUE,FALSE,nullptr)}; // one reusable event per direction
    Handle pipe,wake,mapping;unsigned char* section{};std::thread worker;std::mutex gate;
    // guarded by gate: the retained state the setters change
    Spec spec;std::vector<uint32_t> contents;RECT pendingRect{};bool shown=false;uint64_t revision=1,specRevision=1;
    int sloshKind=0,sloshAmt=0;bool sloshSet=false,btnCarry=false,busyNow=false,pointerSet=false,flowNext=false;float kickAcc=0;
    std::vector<Button> btns;RECT pointerRect{};unsigned wantBits=0;
    // worker only
    unsigned seq=0,cookie=0,id=0,stateVersion=0,publishCount=0;int secW=0,secH=0;wire::DirtyPlan plan;uint32_t rngState=1;
    std::wstring sid,pipeName,wakeName;
    std::atomic<bool> online{false};std::atomic<unsigned> hostPid{0},minorHost{0},surfaceNow{0};std::atomic<int> errorFrames{0};std::atomic<DWORD> error{0};std::atomic<float> lightNow{0};
    std::string failure;
    void note(std::string text){std::lock_guard<std::mutex>lock(gate);failure=std::move(text);}
    void post(Event event,LPARAM data=0){if(options.notify)PostMessageW(options.notify,options.message,event,data);}
    uint32_t nextRandom(){rngState^=rngState<<13;rngState^=rngState>>17;rngState^=rngState<<5;return rngState;}
    static std::vector<unsigned char> tokenUser(HANDLE process){Handle token;HANDLE h{};if(!OpenProcessToken(process,TOKEN_QUERY,&h))throw std::runtime_error("token");token.reset(h);
        DWORD n=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&n);if(!n||n>4096)throw std::runtime_error("token size");
        std::vector<unsigned char>b(n);if(!GetTokenInformation(token.value,TokenUser,b.data(),n,&n))throw std::runtime_error("token user");return b;}
    void names(){auto user=tokenUser(GetCurrentProcess());LPWSTR str{};
        if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,&str))throw std::runtime_error("sid");sid=str;LocalFree(str);
        std::string text;for(wchar_t c:sid)text+=static_cast<char>(c);unsigned char digest[32]{};
        if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(text.data()),ULONG(text.size()),digest,32)<0)throw std::runtime_error("sha256");
        wchar_t hash[9]{};swprintf(hash,9,L"%02x%02x%02x%02x",digest[0],digest[1],digest[2],digest[3]);
        DWORD session=0;if(!ProcessIdToSessionId(GetCurrentProcessId(),&session))throw std::runtime_error("session");
        std::wstring suffix;for(auto c:options.suffix)if((c>=L'a'&&c<=L'z')||(c>=L'A'&&c<=L'Z')||(c>=L'0'&&c<=L'9')){if(suffix.size()<16)suffix+=c;}
        options.suffix=suffix;auto tag=std::wstring(hash)+L".1";if(!suffix.empty())suffix=L"."+suffix;
        pipeName=L"\\\\.\\pipe\\LGEngine."+tag+L".s"+std::to_wstring(session)+suffix;wakeName=L"Local\\LGEngine."+tag+suffix+L".wake";
    }
    bool peer(){ULONG pid{};DWORD mine{},theirs{};
        if(!GetNamedPipeServerProcessId(pipe.value,&pid)||!ProcessIdToSessionId(pid,&theirs)||!ProcessIdToSessionId(GetCurrentProcessId(),&mine)||mine!=theirs)return false;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));if(!process.value)return false;
        auto a=tokenUser(GetCurrentProcess()),b=tokenUser(process.value);
        if(!EqualSid(reinterpret_cast<TOKEN_USER*>(a.data())->User.Sid,reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid))return false;
        hostPid=pid;return true;
    }
    // A host that an older client started from its app folder serves every later version too, so that folder stays locked for as long
    // as any version runs. Retire it once (its clients reconnect by themselves); the retry starts this build's copy (hostCopy).
    // Only when that copy is in place (so the retry never runs from a folder again), and folders are compared as final paths
    // (a junction or short name in LOCALAPPDATA must not make the host in hostHome() look stray).
    bool retired=false,retiredNow=false;
    static std::wstring finalFolder(const std::wstring&path){Handle f(CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr));
        wchar_t out[32768]{};DWORD n=f.value?GetFinalPathNameByHandleW(f.value,out,32768,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS):0;return n&&n<32768?std::wstring(out,n):std::wstring();}
    bool retireStrayHost(){if(retired)return false;retired=true;auto home=hostHome();auto mine=options.hostPath.find_last_of(L"\\");if(home.empty()||mine==std::wstring::npos)return false;
        auto file=options.hostPath.substr(mine+1);if(!stagedHostName(file)||_wcsicmp(hostCopy(options.hostPath,home).c_str(),(home+file).c_str())!=0)return false;
        Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,hostPid));wchar_t image[32768]{};DWORD n=32768;
        if(!process.value||!QueryFullProcessImageNameW(process.value,0,image,&n))return false;std::wstring path(image,n);auto slash=path.find_last_of(L"\\");
        if(slash==std::wstring::npos||!stagedHostName(path.substr(slash+1)))return false;
        auto theirs=finalFolder(path.substr(0,slash)),ours=finalFolder(home.substr(0,home.size()-1));if(theirs.empty()||ours.empty()||_wcsicmp(theirs.c_str(),ours.c_str())==0)return false;
        pipe.reset();hostPid=0;if(!TerminateProcess(process.value,1))return false;WaitForSingleObject(process.value,2000);retiredNow=true;return true;}
    bool startHost(Handle&process){
        // The shared host must survive this app's job. Never fall back to an inherited job.
        if(options.hostPath.size()<4||!((options.hostPath[1]==L':'&&options.hostPath[2]==L'\\')||options.hostPath.substr(0,2)==L"\\\\"))return false;
        // Started from its own copy, with that folder (not this app's) as its working folder: neither locks an app folder.
        auto home=hostHome();auto path=hostCopy(options.hostPath,home);
        std::wstring cmd=L"\""+path+L"\"";if(!options.suffix.empty())cmd+=L" --suffix "+options.suffix;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION child{};
        if(!CreateProcessW(path.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB,nullptr,home.empty()?nullptr:home.c_str(),&startup,&child)){error=GetLastError();return false;}
        CloseHandle(child.hThread);process.reset(child.hProcess);return true;
    }
    // Overlapped write with the reusable event.
    bool io(void*data,DWORD size){ResetEvent(wrEv.value);OVERLAPPED op{};op.hEvent=wrEv.value;DWORD count{};
        BOOL ok=WriteFile(pipe.value,data,size,&count,&op);
        if(!ok&&GetLastError()==ERROR_IO_PENDING){HANDLE handles[]={stop.value,wrEv.value};DWORD wait=WaitForMultipleObjects(2,handles,FALSE,2000);
            if(wait!=WAIT_OBJECT_0+1){CancelIoEx(pipe.value,&op);GetOverlappedResult(pipe.value,&op,&count,TRUE);return false;}ok=GetOverlappedResult(pipe.value,&op,&count,FALSE);}
        return ok&&count==size;
    }
    bool send(unsigned type,const wire::Writer&p){auto b=wire::frame(type,++seq,p);return io(b.data(),DWORD(b.size()));}
    bool connect(){
        // Wait on the host process we started (and the stop event) instead of sleeping blind; still bounded at ~3.5 s (the host waits up to 3 s for its mutex).
        Handle process;ULONGLONG deadline=GetTickCount64()+3500;bool started=false;
        for(;;){
            pipe.reset(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            if(pipe.value)break;
            if(!started){started=true;if(!startHost(process))return false;}
            if(GetTickCount64()>=deadline)return false;
            HANDLE handles[]={stop.value,process.value};DWORD wait=WaitForMultipleObjects(process.value?2:1,handles,FALSE,50);
            if(wait==WAIT_OBJECT_0)return false;
            if(wait==WAIT_OBJECT_0+1){
                // The host exited. A failure exit will never open the pipe: give up now. A clean exit may be a hand-off to a
                // host that is already running, so keep polling until the deadline.
                DWORD code=1;GetExitCodeProcess(process.value,&code);process.reset();if(code!=0){
                    pipe.reset(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
                    if(pipe.value)break;return false;}}
        }
        if(!pipe.value||!peer()||(!started&&retireStrayHost()))return false;DWORD mode=PIPE_READMODE_MESSAGE;if(!SetNamedPipeHandleState(pipe.value,&mode,nullptr,nullptr))return false;
        if(!send(wire::Hello,wire::hello(wire::kMinor)))return false;
        std::vector<unsigned char>b(65552);ResetEvent(rdEv.value);OVERLAPPED op{};op.hEvent=rdEv.value;DWORD count{};
        BOOL ok=ReadFile(pipe.value,b.data(),DWORD(b.size()),&count,&op);
        if(!ok&&GetLastError()==ERROR_IO_PENDING){HANDLE handles[]={stop.value,rdEv.value};if(WaitForMultipleObjects(2,handles,FALSE,2000)!=WAIT_OBJECT_0+1){CancelIoEx(pipe.value,&op);GetOverlappedResult(pipe.value,&op,&count,TRUE);return false;}ok=GetOverlappedResult(pipe.value,&op,&count,FALSE);}
        if(!ok)return false;b.resize(count);if(!wire::frame(b)||b[6]!=2)return false;wire::Reader r{b};if(r.u16()!=1||r.bad)return false;
        // HelloAck: u16 version, str name, [u16 hostMinor]. No minor = a 1.0 host. Session minor = min(ours, theirs).
        r.str(256);unsigned theirs=0;if(!r.bad&&r.at+2<=b.size())theirs=r.u16();minorHost=std::min(theirs,wire::kMinor);
        wake.reset(OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,wakeName.c_str()));return wake.value!=nullptr;
    }
    void unmap(){if(section)UnmapViewOfFile(section);section=nullptr;mapping.reset();}
    void clearSection(){unmap();id=0;surfaceNow=0;publishCount=0;secW=secH=0;}
    // A CreateAck names the surface and its section. After a Resize it is the same surface (and cookie) with a NEW section.
    bool ack(const std::vector<unsigned char>&b,const Spec&sent,bool resized){wire::Reader r{b};unsigned incoming=r.u32(),surface=r.u32();auto name=r.str(256);unsigned bytes=r.u32(),w=r.u32(),h=r.u32();
        if(r.bad||incoming!=cookie||!surface||(resized&&surface!=id)||w!=unsigned(sent.width)||h!=unsigned(sent.height)||bytes!=320ull+8ull*w*h)return false;
        std::wstring wide(name.begin(),name.end());std::wstring expected=L"Local\\LGEngine.S.";
        if(wide.substr(0,expected.size())!=expected)return false;
        unmap();publishCount=0;mapping.reset(OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,wide.c_str()));if(!mapping.value)return false;
        section=static_cast<unsigned char*>(MapViewOfFile(mapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,bytes));if(!section)return false;
        if(wire::get(section)!=0x31534c47||wire::get(section+4)!=1||wire::get(section+8)!=bytes||wire::get(section+12)!=w||wire::get(section+16)!=h){clearSection();return false;}
        id=surface;surfaceNow=surface;secW=int(w);secH=int(h);return true;
    }
    void put(unsigned off,uint32_t n){memcpy(section+off,&n,4);}void putFloat(unsigned off,float f){memcpy(section+off,&f,4);}
    // State, then (if anything is pending) the contents. The pixels are copied straight from `contents` under the lock: only the
    // rect the back buffer lacks (this publish's dirty rect united with the previous one's), so no per-change vector copy.
    void publish(const Spec&s,bool visible){
        auto counter=reinterpret_cast<volatile LONG*>(section+64);InterlockedIncrement(counter);MemoryBarrier();
        put(68,1);put(72,++stateVersion);put(76,(visible?1:0)|(s.hover?4:0));put(80,s.x);put(84,s.y);putFloat(88,1);putFloat(92,1);putFloat(96,1);
        putFloat(100,0);putFloat(104,0);putFloat(108,0);put(112,s.motion);put(116,s.inMs);put(120,s.outMs);
        for(int i=0;i<4;++i)put(124+4*i,0);LARGE_INTEGER now;QueryPerformanceCounter(&now);memcpy(section+144,&now.QuadPart,8);
        MemoryBarrier();InterlockedIncrement(counter);
        {std::lock_guard<std::mutex>lock(gate);
            RECT dirty=wire::rectClip(pendingRect,RECT{0,0,secW,secH});
            if(!wire::rectEmpty(dirty)&&spec.width==secW&&spec.height==secH){ // (a size change in flight: leave it pending)
                unsigned front=wire::get(section+192)&1,back=front==0?1:0,off=200+back*24;
                RECT copy=plan.next(dirty,int(back),secW,secH);bool have=contents.size()==size_t(secW)*secH;
                InterlockedExchange(reinterpret_cast<volatile LONG*>(section+off),LONG(2*++publishCount-1));MemoryBarrier();
                size_t count=size_t(secW)*secH;auto dest=section+320+back*count*4;
                for(LONG y=copy.top;y<copy.bottom;++y){size_t at=size_t(y)*secW+copy.left,n=size_t(copy.right-copy.left)*4;
                    if(have)memcpy(dest+at*4,contents.data()+at,n);else memset(dest+at*4,0,n);}
                put(off+4,uint32_t(dirty.left));put(off+8,uint32_t(dirty.top));put(off+12,uint32_t(dirty.right-dirty.left));put(off+16,uint32_t(dirty.bottom-dirty.top));MemoryBarrier();
                InterlockedExchange(reinterpret_cast<volatile LONG*>(section+off),LONG(2*publishCount));MemoryBarrier();InterlockedExchange(reinterpret_cast<volatile LONG*>(section+192),LONG(back));
                pendingRect=RECT{};
            }}
        SetEvent(wake.value);
    }
    void runConnection(unsigned&attempt){
        uint64_t sentSpec=0,sentRevision=0;Spec sent;bool created=false,awaiting=false,awaitingCreate=false;unsigned createSeq=0,lookSeq=0,extraSeq=0;
        OVERLAPPED read{};std::vector<unsigned char> b(65552);DWORD count=0;bool reading=false;
        {std::lock_guard<std::mutex>lock(gate);wantBits&=~unsigned(WKick|WCut);kickAcc=0;} // momentary: never replayed on a new connection
        auto cancel=[&]{if(reading){CancelIoEx(pipe.value,&read);GetOverlappedResult(pipe.value,&read,&count,TRUE);reading=false;}};
        try{while(WaitForSingleObject(stop.value,0)!=WAIT_OBJECT_0){
            Spec current;bool visible;uint64_t sr,rev;unsigned minor=minorHost;
            {std::lock_guard<std::mutex>lock(gate);current=spec;visible=shown;sr=specRevision;rev=revision;}
            if(!awaiting&&sr!=sentSpec){
                if(!created||wire::needsCreate(sent,current,minor)){
                    if(id){if(!send(wire::Destroy,wire::destroy(id)))break;}clearSection();++cookie;sent=current;sentSpec=sr;lookSeq=extraSeq=0;{std::lock_guard<std::mutex>lock(gate);flowNext=false;}
                    if(!send(wire::Create,wire::create(cookie,current)))break;createSeq=seq;awaiting=awaitingCreate=created=true;sentRevision=0;
                }else{
                    bool flow=false;{std::lock_guard<std::mutex>lock(gate);flow=flowNext;flowNext=false;}
                    bool looked=!(sent.look==current.look)||sent.profile!=current.profile,motion=sent.zoom!=current.zoom||sent.liquid!=current.liquid||sent.bounce!=current.bounce||sent.travel!=current.travel||sent.indicator!=current.indicator;
                    bool updates=sent.fps!=current.fps||sent.adapt!=current.adapt||sent.savePower!=current.savePower,resized=sent.width!=current.width||sent.height!=current.height;
                    sent=current;sentSpec=sr;
                    if(looked){if(!send(wire::SetLook,wire::setLook(id,current.look,current.profile)))break;lookSeq=seq;}
                    if(minor>=1&&motion){if(!send(wire::SetMotionEx,wire::setMotionEx(id,current.zoom,current.liquid,current.bounce,current.travel,current.indicator)))break;extraSeq=seq;}
                    if(minor>=1&&updates){if(!send(wire::SetUpdates,wire::setUpdates(id,current.fps,current.adapt,current.savePower)))break;extraSeq=seq;}
                    if(resized){if(!send(wire::Resize,wire::resize(id,current.width,current.height,flow&&minor>=1?1u:0u)))break;createSeq=seq;awaiting=true;awaitingCreate=false;sentRevision=0;}
                }
            }
            if(section&&!awaiting){
                unsigned todo=0;{std::lock_guard<std::mutex>lock(gate);if(minor>=1)todo=wantBits;wantBits=0;} // (a 1.0 host: extras are dropped, hide(false) is the plain hide below)
                if(todo&WCut){if(!send(wire::Cut,wire::cut(id)))break;extraSeq=seq;} // before the visible=0 publish, or the host starts the animated hide first
                if(rev!=sentRevision){publish(current,visible);sentRevision=rev;}
                if(todo&WSlosh){int style,amount;{std::lock_guard<std::mutex>lock(gate);style=sloshKind;amount=sloshAmt;}if(!send(wire::SetSlosh,wire::setSlosh(id,style,amount)))break;extraSeq=seq;}
                if(todo&WButtons){wire::Writer w;{std::lock_guard<std::mutex>lock(gate);w=wire::setButtons(id,btns,btnCarry);}if(!send(wire::SetButtons,w))break;extraSeq=seq;}
                if(todo&WPointer){RECT area;{std::lock_guard<std::mutex>lock(gate);area=pointerRect;}if(!send(wire::SetPointerArea,wire::setPointerArea(id,area)))break;extraSeq=seq;}
                if(todo&WBusy){bool on;{std::lock_guard<std::mutex>lock(gate);on=busyNow;}if(!send(wire::SetBusy,wire::setBusy(id,on)))break;extraSeq=seq;}
                if(todo&WKick){float push;{std::lock_guard<std::mutex>lock(gate);push=kickAcc;kickAcc=0;}if(push!=0.f){if(!send(wire::Kick,wire::kick(id,push)))break;extraSeq=seq;}}
            }
            if(!reading){ResetEvent(rdEv.value);memset(&read,0,sizeof(read));read.hEvent=rdEv.value;
                BOOL ok=ReadFile(pipe.value,b.data(),DWORD(b.size()),&count,&read);
                if(!ok&&GetLastError()!=ERROR_IO_PENDING)break;reading=true;if(ok)SetEvent(rdEv.value);}
            HANDLE handles[]={stop.value,changed.value,rdEv.value};DWORD result=WaitForMultipleObjects(3,handles,FALSE,awaiting?5000:INFINITE);
            if(result==WAIT_OBJECT_0||result==WAIT_TIMEOUT)break;if(result==WAIT_OBJECT_0+1)continue;if(result!=WAIT_OBJECT_0+2)break;
            if(!GetOverlappedResult(pipe.value,&read,&count,FALSE)){reading=false;break;}reading=false;
            std::vector<unsigned char> message(b.begin(),b.begin()+count);if(!wire::frame(message))break;
            unsigned type=message[6]|(unsigned(message[7])<<8);wire::Reader r{message};
            if(type==wire::CreateAck){if(!awaiting||!ack(message,sent,!awaitingCreate))break;
                bool fresh=awaitingCreate;awaiting=awaitingCreate=false;plan.reset(secW,secH);sentRevision=0;if(fresh)attempt=0; // a CreateAck ends the backoff
                {std::lock_guard<std::mutex>lock(gate);pendingRect=RECT{0,0,secW,secH}; // the new section is blank: send the whole frame
                    if(fresh){ // a new surface (also after a host restart): re-send every extra
                        if(sloshSet)wantBits|=WSlosh;if(!btns.empty())wantBits|=WButtons;if(pointerSet)wantBits|=WPointer;if(busyNow)wantBits|=WBusy;}}
                if(fresh&&minorHost>=1&&(sent.travel!=-1||sent.indicator!=-1)&&!send(wire::SetMotionEx,wire::setMotionEx(id,sent.zoom,sent.liquid,sent.bounce,sent.travel,sent.indicator)))break;}
            else if(type==0xff||type==0x23){
                // Late events for retired surfaces and replies to their Destroy
                // requests must not fail a replacement surface.
                ++errorFrames;unsigned inSeq=0;
                if(type==0x23){auto target=r.u32();if(target!=id)continue;}
                else{inSeq=wire::get(message.data()+12);if(inSeq!=createSeq&&inSeq!=lookSeq&&inSeq!=extraSeq)continue;}
                auto code=r.u32();auto text=r.str(1024);note("host error "+std::to_string(code)+": "+text);error=ERROR_INVALID_DATA;
                if(type==0xff&&awaiting&&awaitingCreate&&inSeq==createSeq){ // Create rejected: report once, keep the pipe, wait for a changed spec
                    created=awaiting=awaitingCreate=false;post(Failed,LPARAM(code));continue;}
                if(type==0xff&&inSeq!=createSeq){ // SetLook or an extra rejected: report, no disconnect (an Unsupported extra on a 1.0 host is expected)
                    if(code==4&&minorHost<1)continue;post(Failed,LPARAM(code));continue;}
                post(Failed);break;}
            else if(type>=0x20&&type<=0x22){unsigned target=r.u32();if(target!=id)continue;
                if(type==0x20)post(Shown);else if(type==0x21)post(Hidden);else {auto light=r.u8();if(!r.bad)post(Appearance,light);}}
            else if(type==0x24&&minorHost>=1){unsigned target=r.u32();float level=r.f32();if(target!=id||r.bad||!std::isfinite(level))continue;
                lightNow=std::min(1.f,std::max(0.f,level));post(LightMoved,LPARAM(std::lround(lightNow.load()*1000.f)));}
        }}catch(...){cancel();throw;}cancel();
    }
    // 4551 = Smart App Control / App Control for Business, 1260 = blocked by group policy, 577 = invalid image signature.
    static bool policyBlocked(DWORD e){return e==4551||e==1260||e==577;}
    void run(){bool ever=false;unsigned attempt=0,initialAttempts=0;
        while(WaitForSingleObject(stop.value,0)!=WAIT_OBJECT_0){
            try{if(connect()){online=true;post(ever?Restarted:Connected);ever=true;ULONGLONG up=GetTickCount64();runConnection(attempt);if(GetTickCount64()-up>=10000)attempt=0;}}
            catch(const std::exception&e){error=ERROR_INVALID_DATA;note(e.what());}
            catch(...){error=ERROR_INVALID_DATA;note("unexpected client exception");}
            bool was=online.exchange(false);clearSection();wake.reset();pipe.reset();hostPid=0;minorHost=0;if(was)post(Lost);
            if(retiredNow){retiredNow=false;continue;} // retiring a stray host is not a failed attempt: start this build's copy at once
            // A host may close its pipe while exiting after idle. Retry that initial handoff before declaring the service
            // unavailable. Deliberately NOT retried forever: if Smart App Control blocks the host exe, every new attempt would
            // pop a Smart App Control toast, so after 3 attempts that never connected we post Failed and stop. A launch the
            // policy refused will be refused again: stop after the first one, also once a host has run (lastError() keeps the code for the owner).
            if((!ever&&++initialAttempts>=3)||policyBlocked(error)){post(Failed);return;}
            if(WaitForSingleObject(stop.value,wire::backoffMs(attempt++,nextRandom()))==WAIT_OBJECT_0)return;
        }
    }
public:
    explicit Client(Options opts,Spec initial):options(std::move(opts)),spec(std::move(initial)){
        if(!wire::valid(spec)||!stop.value||!changed.value||!wrEv.value||!rdEv.value)throw std::invalid_argument("invalid LGEngine client options");
        if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&rngState),4,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0||!rngState)rngState=GetTickCount()*2654435761u+GetCurrentProcessId()+1;
        names();worker=std::thread([this]{run();});
    }
    ~Client(){SetEvent(stop.value);if(worker.joinable())worker.join();clearSection();}
    Client(const Client&)=delete;Client&operator=(const Client&)=delete;
    bool connected()const{return online;}unsigned hostProcessId()const{return hostPid;}DWORD lastError()const{return error;}
    // 0 on a 1.0 host (or while not connected), 1 on a 1.1 host. light(): last LightMoved value 0..1. surfaceId(): the host's id of
    // the surface (0 = none); it stays the same over in-place changes and changes only when the surface is re-created.
    int hostErrors()const{return errorFrames;} // Error frames received from the host so far (any seq), for tests and diagnostics
    unsigned hostMinor()const{return minorHost;}float light()const{return lightNow;}unsigned surfaceId()const{return surfaceNow;}
    std::string failureText(){std::lock_guard<std::mutex>lock(gate);return failure;}
    // Look, profile, size, place, motion and (1.1) zoom/liquid/bounce/travel/indicator/fps/adapt/savePower change in place; kind,
    // scale, label and inShots (and, on a 1.0 host, the Create-only numbers) re-create the surface. A size change clears the
    // contents: submit the new sized bitmap (or use resize()).
    void setSpec(Spec value){if(!wire::valid(value))throw std::invalid_argument("invalid glass spec");{std::lock_guard<std::mutex>lock(gate);
        if(value.width!=spec.width||value.height!=spec.height){contents.clear();pendingRect=RECT{0,0,value.width,value.height};}spec=std::move(value);++specRevision;++revision;}SetEvent(changed.value);}
    void setPlace(int x,int y){if(x< -100000||x>100000||y< -100000||y>100000)throw std::invalid_argument("place");
        {std::lock_guard<std::mutex>lock(gate);spec.x=x;spec.y=y;++revision;}SetEvent(changed.value);}
    void show(bool visible=true){{std::lock_guard<std::mutex>lock(gate);shown=visible;if(visible)wantBits&=~unsigned(WCut);++revision;}SetEvent(changed.value);}
    // animate=false: Cut (no out animation) on a 1.1 host, the plain hide on a 1.0 host.
    void hide(bool animate=true){{std::lock_guard<std::mutex>lock(gate);shown=false;if(!animate)wantBits|=WCut;++revision;}SetEvent(changed.value);}
    // Premultiplied ARGB, exactly width*height. `dirty` (card coordinates) is what changed since the last call; an empty rect means
    // the whole frame. Dirty rects accumulate until the worker publishes. The vector replaces the contents, so everything outside
    // `dirty` must be unchanged.
    void setContents(std::vector<uint32_t> pixels,RECT dirty={}){{std::lock_guard<std::mutex>lock(gate);if(pixels.size()!=size_t(spec.width)*spec.height)throw std::invalid_argument("contents size");
        RECT card{0,0,spec.width,spec.height};contents=std::move(pixels);pendingRect=wire::rectUnion(pendingRect,wire::rectEmpty(dirty)?card:wire::rectClip(dirty,card));++revision;}SetEvent(changed.value);}
    // New size and contents in one step (an in-place Resize; flow = the host eases the old picture into the new size, 1.1 only).
    void resize(int w,int h,std::vector<uint32_t> pixels,bool flow=false){
        if(w<1||h<1||w>4096||h>4096||uint64_t(w)*h>4000000||pixels.size()!=size_t(w)*h)throw std::invalid_argument("resize");
        {std::lock_guard<std::mutex>lock(gate);bool grew=w!=spec.width||h!=spec.height;spec.width=w;spec.height=h;contents=std::move(pixels);pendingRect=RECT{0,0,w,h};
            if(grew){flowNext=flow;++specRevision;}++revision;}SetEvent(changed.value);}
    // 1.1 extras (ignored by a 1.0 host). style 0..3 = lean, bounce, pulse, none; amount 0..100. Nothing is sent by kick() until set.
    void setSlosh(int style,int amount){if(style<0||style>3||amount<0||amount>100)throw std::invalid_argument("slosh");
        {std::lock_guard<std::mutex>lock(gate);sloshKind=style;sloshAmt=amount;sloshSet=true;wantBits|=WSlosh;}SetEvent(changed.value);}
    // push = way*4.4*amount/60 (a pulse always uses way=1); nothing if amount<=0 or style>=3. Pushes before the next send add up (|sum| <= 8).
    void kick(int way){{std::lock_guard<std::mutex>lock(gate);float push=wire::kickPush(sloshKind,sloshAmt,way);if(push==0.f)return;
        kickAcc=std::min(8.f,std::max(-8.f,kickAcc+push));wantBits|=WKick;}SetEvent(changed.value);}
    void setButtons(std::vector<Button> list,bool carry){if(!wire::buttonsValid(list))throw std::invalid_argument("buttons");
        {std::lock_guard<std::mutex>lock(gate);btns=std::move(list);btnCarry=carry;wantBits|=WButtons;}SetEvent(changed.value);}
    void setPointerArea(RECT area){if(area.left<-100000||area.left>100000||area.top<-100000||area.top>100000)throw std::invalid_argument("pointer area");
        {std::lock_guard<std::mutex>lock(gate);pointerRect=area;pointerSet=true;wantBits|=WPointer;}SetEvent(changed.value);}
    void setBusy(bool busy){{std::lock_guard<std::mutex>lock(gate);busyNow=busy;wantBits|=WBusy;}SetEvent(changed.value);}
};
}
