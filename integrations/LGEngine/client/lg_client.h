#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
// C++17 client for LGEngine protocol v1, evaluated at b6e93053.
// Screen capture, glass rendering and glass windows belong exclusively to the host.
#include <windows.h>
#include <sddl.h>
#include <bcrypt.h>
#include <algorithm>
#include <atomic>
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
struct Spec {
    int width=420,height=200,x=100,y=100,kind=0;float scale=1;
    bool adapt=true,hover=true,savePower=true,inShots=false;
    int fps=15,motion=4,inMs=380,outMs=230;Look look;
    std::string label="LiquidGlassControl.Host",profile;
};
enum Event : unsigned {Connected=1,Lost=2,Restarted=3,Shown=4,Hidden=5,Appearance=6,Failed=7};
struct Options {std::wstring hostPath,suffix;HWND notify{};UINT message=WM_APP+60;};
inline std::wstring bundledHostPath(const std::wstring&directory){
    wchar_t name[128]{};GetPrivateProfileStringW(L"Host",L"Executable",L"",name,128,(directory+L"host-current.ini").c_str());
    std::wstring file=name;
    bool safe=file.size()==81&&file.substr(0,13)==L"LGEngineHost-"&&file.substr(77)==L".exe";
    for(size_t i=13;safe&&i<77;++i)if(!((file[i]>=L'0'&&file[i]<=L'9')||(file[i]>=L'a'&&file[i]<=L'f')))safe=false;
    return directory+(safe?file:L"LGEngineHost.exe");
}
namespace wire {
struct Writer {
    std::vector<unsigned char> bytes;
    void u8(unsigned n){bytes.push_back(static_cast<unsigned char>(n));}
    void u16(unsigned n){u8(n);u8(n>>8);}void u32(uint32_t n){for(int i=0;i<4;++i)u8(n>>(8*i));}
    void f32(float f){uint32_t n;memcpy(&n,&f,4);u32(n);}
    void str(const std::string&s){if(s.size()>60000)throw std::invalid_argument("string too long");u16(unsigned(s.size()));bytes.insert(bytes.end(),s.begin(),s.end());}
};
inline uint32_t get(const unsigned char*p){uint32_t n;memcpy(&n,p,4);return n;}
struct Reader {
    const std::vector<unsigned char>&b;size_t at=16;bool bad=false;
    bool need(size_t n){if(at>b.size()||n>b.size()-at){bad=true;return false;}return true;}
    unsigned u8(){return need(1)?b[at++]:0;}unsigned u16(){unsigned n=u8();return n|(u8()<<8);}
    uint32_t u32(){if(!need(4))return 0;auto n=get(b.data()+at);at+=4;return n;}
    std::string str(size_t max){unsigned n=u16();if(n>max||!need(n)){bad=true;return {};}
        std::string s(reinterpret_cast<const char*>(b.data()+at),n);at+=n;return s;}
};
inline bool frame(const std::vector<unsigned char>&b){return b.size()>=16&&get(b.data())==0x3145474c&&b[4]==1&&b[5]==0&&get(b.data()+8)<=65536&&get(b.data()+8)==b.size()-16;}
inline std::vector<unsigned char> frame(unsigned type,unsigned seq,const Writer&p){Writer w;w.u32(0x3145474c);w.u16(1);w.u16(type);w.u32(unsigned(p.bytes.size()));w.u32(seq);w.bytes.insert(w.bytes.end(),p.bytes.begin(),p.bytes.end());return w.bytes;}
inline void look(Writer&w,Look l){for(int n:{l.dark,l.blur,l.refraction,l.rim,l.shadow,l.round,l.intensity,l.elevation})w.u32(n);}
inline Writer create(unsigned cookie,const Spec&s){Writer w;w.u32(cookie);w.u8(s.kind);w.f32(s.scale);w.u32(s.width);w.u32(s.height);w.u32(s.x);w.u32(s.y);
    w.u8((s.inShots?1:0)|(s.savePower?2:0)|(s.adapt?4:0)|(s.hover?8:0));w.u32(s.fps);w.u32(s.motion);w.u32(s.inMs);w.u32(s.outMs);w.u32(10);w.u32(60);w.u32(45);look(w,s.look);w.str(s.label);w.str(s.profile);return w;}
inline bool valid(const Spec&s){auto range=[](int n,int lo,int hi){return n>=lo&&n<=hi;};
    return range(s.width,1,4096)&&range(s.height,1,4096)&&uint64_t(s.width)*s.height<=4000000&&
        range(s.x,-100000,100000)&&range(s.y,-100000,100000)&&s.scale>=.01f&&s.scale<=8&&
        range(s.kind,0,1)&&range(s.fps,0,60)&&range(s.motion,0,8)&&range(s.inMs,60,1200)&&range(s.outMs,60,1200)&&
        !s.label.empty()&&s.label.size()<=256&&s.profile.size()<=2400&&
        range(s.look.dark,0,100)&&range(s.look.blur,0,100)&&range(s.look.refraction,0,100)&&range(s.look.rim,0,100)&&range(s.look.shadow,0,100)&&range(s.look.round,0,28)&&range(s.look.intensity,0,150)&&range(s.look.elevation,0,100);}
}
// One retained surface per connection. Multiple clients share the same host.
// All pipe I/O is on the worker. Setters coalesce under a short mutex.
class Client {
    Options options;Handle stop{CreateEventW(nullptr,TRUE,FALSE,nullptr)},changed{CreateEventW(nullptr,FALSE,FALSE,nullptr)};
    Handle pipe,wake,mapping;unsigned char* section{};std::thread worker;std::mutex gate;
    Spec spec;std::vector<uint32_t> contents;bool shown=false;uint64_t revision=1,contentRevision=1,specRevision=1;
    unsigned seq=0,cookie=0,id=0,stateVersion=0,publishCount=0;std::wstring sid,pipeName,wakeName;
    std::atomic<bool> online{false};std::atomic<unsigned> hostPid{0};std::atomic<DWORD> error{0};
    std::string failure;
    void note(std::string text){std::lock_guard<std::mutex>lock(gate);failure=std::move(text);}
    void post(Event event,LPARAM data=0){if(options.notify)PostMessageW(options.notify,options.message,event,data);}
    static std::vector<unsigned char> tokenUser(HANDLE process){Handle token;HANDLE h{};if(!OpenProcessToken(process,TOKEN_QUERY,&h))throw std::runtime_error("token");token.reset(h);
        DWORD n=0;GetTokenInformation(token.value,TokenUser,nullptr,0,&n);if(!n||n>4096)throw std::runtime_error("token size");
        std::vector<unsigned char>b(n);if(!GetTokenInformation(token.value,TokenUser,b.data(),n,&n))throw std::runtime_error("token user");return b;}
    void names(){auto user=tokenUser(GetCurrentProcess());LPWSTR str{};
        if(!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,&str))throw std::runtime_error("sid");sid=str;LocalFree(str);
        std::string text(sid.begin(),sid.end());unsigned char digest[32]{};
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
    bool startHost(){
        // The shared host must survive this app's job. Never fall back to an inherited job.
        if(options.hostPath.size()<4||!((options.hostPath[1]==L':'&&options.hostPath[2]==L'\\')||options.hostPath.substr(0,2)==L"\\\\"))return false;
        std::wstring cmd=L"\""+options.hostPath+L"\"";if(!options.suffix.empty())cmd+=L" --suffix "+options.suffix;
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;PROCESS_INFORMATION child{};
        if(!CreateProcessW(options.hostPath.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_BREAKAWAY_FROM_JOB,nullptr,nullptr,&startup,&child)){error=GetLastError();return false;}
        CloseHandle(child.hThread);CloseHandle(child.hProcess);return true;
    }
    bool io(void*data,DWORD size,bool write){Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED op{};op.hEvent=event.value;DWORD count{};
        BOOL ok=write?WriteFile(pipe.value,data,size,&count,&op):ReadFile(pipe.value,data,size,&count,&op);
        if(!ok&&GetLastError()==ERROR_IO_PENDING){HANDLE handles[]={stop.value,event.value};DWORD wait=WaitForMultipleObjects(2,handles,FALSE,2000);
            if(wait!=WAIT_OBJECT_0+1){CancelIoEx(pipe.value,&op);GetOverlappedResult(pipe.value,&op,&count,TRUE);return false;}ok=GetOverlappedResult(pipe.value,&op,&count,FALSE);}
        return ok&&(write?count==size:count>=16);
    }
    bool send(unsigned type,const wire::Writer&p){auto b=wire::frame(type,++seq,p);return io(b.data(),DWORD(b.size()),true);}
    bool connect(){
        for(int attempt=0;attempt<25;++attempt){
            pipe.reset(CreateFileW(pipeName.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr));
            if(pipe.value)break;if(attempt==0&&!startHost())return false;if(WaitForSingleObject(stop.value,100)==WAIT_OBJECT_0)return false;
        }
        if(!pipe.value||!peer())return false;DWORD mode=PIPE_READMODE_MESSAGE;if(!SetNamedPipeHandleState(pipe.value,&mode,nullptr,nullptr))return false;
        wire::Writer hello;hello.u16(1);hello.u16(1);hello.str("LiquidGlassControl C++");if(!send(1,hello))return false;
        std::vector<unsigned char>b(65552);Handle event(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED op{};op.hEvent=event.value;DWORD count{};
        BOOL ok=ReadFile(pipe.value,b.data(),DWORD(b.size()),&count,&op);
        if(!ok&&GetLastError()==ERROR_IO_PENDING){HANDLE handles[]={stop.value,event.value};if(WaitForMultipleObjects(2,handles,FALSE,2000)!=WAIT_OBJECT_0+1){CancelIoEx(pipe.value,&op);GetOverlappedResult(pipe.value,&op,&count,TRUE);return false;}ok=GetOverlappedResult(pipe.value,&op,&count,FALSE);}
        if(!ok)return false;b.resize(count);if(!wire::frame(b)||b[6]!=2)return false;wire::Reader r{b};if(r.u16()!=1||r.bad)return false;
        wake.reset(OpenEventW(EVENT_MODIFY_STATE|SYNCHRONIZE,FALSE,wakeName.c_str()));return wake.value!=nullptr;
    }
    void clearSection(){if(section)UnmapViewOfFile(section);section=nullptr;mapping.reset();id=0;publishCount=0;}
    bool ack(const std::vector<unsigned char>&b,const Spec&sent){wire::Reader r{b};unsigned incoming=r.u32(),surface=r.u32();auto name=r.str(256);unsigned bytes=r.u32(),w=r.u32(),h=r.u32();
        if(r.bad||incoming!=cookie||!surface||w!=unsigned(sent.width)||h!=unsigned(sent.height)||bytes!=320ull+8ull*w*h)return false;
        std::wstring wide(name.begin(),name.end());std::wstring expected=L"Local\\LGEngine.S.";
        if(wide.substr(0,expected.size())!=expected)return false;
        clearSection();mapping.reset(OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,wide.c_str()));if(!mapping.value)return false;
        section=static_cast<unsigned char*>(MapViewOfFile(mapping.value,FILE_MAP_READ|FILE_MAP_WRITE,0,0,bytes));if(!section)return false;
        if(wire::get(section)!=0x31534c47||wire::get(section+4)!=1||wire::get(section+8)!=bytes||wire::get(section+12)!=w||wire::get(section+16)!=h){clearSection();return false;}
        id=surface;return true;
    }
    void put(unsigned off,uint32_t n){memcpy(section+off,&n,4);}void putFloat(unsigned off,float f){memcpy(section+off,&f,4);}
    void publish(const Spec&s,const std::vector<uint32_t>&pixels,bool visible,bool content){
        auto counter=reinterpret_cast<volatile LONG*>(section+64);InterlockedIncrement(counter);MemoryBarrier();
        put(68,1);put(72,++stateVersion);put(76,(visible?1:0)|(s.hover?4:0));put(80,s.x);put(84,s.y);putFloat(88,1);putFloat(92,1);putFloat(96,1);
        putFloat(100,0);putFloat(104,0);putFloat(108,0);put(112,s.motion);put(116,s.inMs);put(120,s.outMs);
        for(int i=0;i<4;++i)put(124+4*i,0);LARGE_INTEGER now;QueryPerformanceCounter(&now);memcpy(section+144,&now.QuadPart,8);
        MemoryBarrier();InterlockedIncrement(counter);
        if(content){unsigned front=wire::get(section+192),back=front==0?1:0,off=200+back*24;
            InterlockedExchange(reinterpret_cast<volatile LONG*>(section+off),LONG(2*++publishCount-1));MemoryBarrier();
            size_t count=size_t(s.width)*s.height;auto dest=section+320+back*count*4;
            if(pixels.size()==count)memcpy(dest,pixels.data(),count*4);else memset(dest,0,count*4);
            put(off+4,0);put(off+8,0);put(off+12,s.width);put(off+16,s.height);MemoryBarrier();
            InterlockedExchange(reinterpret_cast<volatile LONG*>(section+off),LONG(2*publishCount));MemoryBarrier();InterlockedExchange(reinterpret_cast<volatile LONG*>(section+192),back);
        }SetEvent(wake.value);
    }
    void runConnection(){
        uint64_t sentSpec=0,sentRevision=0,sentContent=0;Spec sent;bool awaiting=false;unsigned createSeq=0;
        Handle readEvent(CreateEventW(nullptr,TRUE,FALSE,nullptr));OVERLAPPED read{};read.hEvent=readEvent.value;std::vector<unsigned char> b(65552);DWORD count=0;bool reading=false;
        auto cancel=[&]{if(reading){CancelIoEx(pipe.value,&read);GetOverlappedResult(pipe.value,&read,&count,TRUE);reading=false;}};
        try{while(WaitForSingleObject(stop.value,0)!=WAIT_OBJECT_0){
            Spec current;std::vector<uint32_t> image;bool visible;uint64_t sr,rev,cr;
            {std::lock_guard<std::mutex>lock(gate);current=spec;visible=shown;sr=specRevision;rev=revision;cr=contentRevision;if(cr!=sentContent)image=contents;}
            if(!awaiting&&sr!=sentSpec){if(id){wire::Writer destroy;destroy.u32(id);if(!send(5,destroy))break;}clearSection();++cookie;sent=current;sentSpec=sr;
                if(!send(3,wire::create(cookie,current)))break;createSeq=seq;awaiting=true;sentRevision=sentContent=0;}
            if(section&&!awaiting&&rev!=sentRevision){publish(current,image,visible,cr!=sentContent);sentRevision=rev;sentContent=cr;}
            if(!reading){ResetEvent(readEvent.value);memset(&read,0,sizeof(read));read.hEvent=readEvent.value;
                BOOL ok=ReadFile(pipe.value,b.data(),DWORD(b.size()),&count,&read);
                if(!ok&&GetLastError()!=ERROR_IO_PENDING)break;reading=true;if(ok)SetEvent(readEvent.value);}
            HANDLE handles[]={stop.value,changed.value,readEvent.value};DWORD result=WaitForMultipleObjects(3,handles,FALSE,awaiting?5000:INFINITE);
            if(result==WAIT_OBJECT_0||result==WAIT_TIMEOUT)break;if(result==WAIT_OBJECT_0+1)continue;if(result!=WAIT_OBJECT_0+2)break;
            if(!GetOverlappedResult(pipe.value,&read,&count,FALSE)){reading=false;break;}reading=false;
            std::vector<unsigned char> message(b.begin(),b.begin()+count);if(!wire::frame(message))break;
            unsigned type=message[6]|(unsigned(message[7])<<8);wire::Reader r{message};
            if(type==4){if(!awaiting||!ack(message,sent))break;awaiting=false;}
            else if(type==0xff||type==0x23){
                // Late events for retired surfaces and replies to their Destroy
                // requests must not fail a replacement surface.
                if(type==0x23){auto target=r.u32();if(target!=id)continue;}
                else if(wire::get(message.data()+12)!=createSeq)continue;
                auto code=r.u32();auto text=r.str(1024);note("host error "+std::to_string(code)+": "+text);error=ERROR_INVALID_DATA;post(Failed);break;}
            else if(type>=0x20&&type<=0x22){unsigned target=r.u32();if(target!=id)continue;
                if(type==0x20)post(Shown);else if(type==0x21)post(Hidden);else {auto light=r.u8();if(!r.bad)post(Appearance,light);}}
        }}catch(...){cancel();throw;}cancel();
    }
    void run(){bool ever=false;unsigned backoff=0,initialAttempts=0;
        while(WaitForSingleObject(stop.value,0)!=WAIT_OBJECT_0){
            try{if(connect()){online=true;post(ever?Restarted:Connected);ever=true;backoff=0;runConnection();}}
            catch(const std::exception&e){error=ERROR_INVALID_DATA;note(e.what());}
            catch(...){error=ERROR_INVALID_DATA;note("unexpected client exception");}
            bool was=online.exchange(false);clearSection();wake.reset();pipe.reset();hostPid=0;if(was)post(Lost);
            // A host may close its pipe while exiting after idle. Retry that
            // initial handoff before declaring the service unavailable.
            if(!ever&&++initialAttempts>=3){post(Failed);return;}DWORD delays[]={100,400,1000,2000};if(WaitForSingleObject(stop.value,delays[std::min(backoff++,3u)])==WAIT_OBJECT_0)return;
        }
    }
public:
    explicit Client(Options opts,Spec initial):options(std::move(opts)),spec(std::move(initial)){
        if(!wire::valid(spec)||!stop.value||!changed.value)throw std::invalid_argument("invalid LGEngine client options");names();worker=std::thread([this]{run();});
    }
    ~Client(){SetEvent(stop.value);if(worker.joinable())worker.join();clearSection();}
    Client(const Client&)=delete;Client&operator=(const Client&)=delete;
    bool connected()const{return online;}unsigned hostProcessId()const{return hostPid;}DWORD lastError()const{return error;}
    std::string failureText(){std::lock_guard<std::mutex>lock(gate);return failure;}
    void setSpec(Spec value){if(!wire::valid(value))throw std::invalid_argument("invalid glass spec");{std::lock_guard<std::mutex>lock(gate);
        if(value.width!=spec.width||value.height!=spec.height){contents.clear();++contentRevision;}spec=std::move(value);++specRevision;++revision;}SetEvent(changed.value);}
    void setPlace(int x,int y){if(x< -100000||x>100000||y< -100000||y>100000)throw std::invalid_argument("place");
        {std::lock_guard<std::mutex>lock(gate);spec.x=x;spec.y=y;++revision;}SetEvent(changed.value);}
    void show(bool visible=true){{std::lock_guard<std::mutex>lock(gate);shown=visible;++revision;}SetEvent(changed.value);}
    void setContents(std::vector<uint32_t> pixels){{std::lock_guard<std::mutex>lock(gate);if(pixels.size()!=size_t(spec.width)*spec.height)throw std::invalid_argument("contents size");contents=std::move(pixels);++contentRevision;++revision;}SetEvent(changed.value);}
};
}
