#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WIN32_WINNT 0x0A00
#include <windows.h>
#undef GetCurrentTime
#include <windows.ui.xaml.hosting.desktopwindowxamlsource.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Media.Imaging.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Composition.h>
#include <cstdio>
#include <vector>
#include "../src/lgengine_native_content.h"
#include "../src/lgengine_native_place.h"
#include "../src/lgengine_native_inspection.h"
#include "../src/native_capture_lane.h"

// Establish whether the supported XAML snapshot API can preserve native content
// while only its composition visual is hidden. Never touches a system window.
template<class Operation>void awaitUi(Operation const&op){
    auto deadline=GetTickCount64()+10000;
    while(op.Status()==winrt::Windows::Foundation::AsyncStatus::Started){
        MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
        if(GetTickCount64()>=deadline){op.Cancel();throw winrt::hresult_error(HRESULT_FROM_WIN32(WAIT_TIMEOUT));}
        MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);
    }
}
std::vector<uint8_t> capture(winrt::Windows::UI::Xaml::UIElement const&e){
    winrt::Windows::UI::Xaml::Media::Imaging::RenderTargetBitmap image;
    auto render=image.RenderAsync(e,320,120);awaitUi(render);render.GetResults();
    auto pixels=image.GetPixelsAsync();awaitUi(pixels);auto buffer=pixels.GetResults();
    auto reader=winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
    std::vector<uint8_t> result(buffer.Length());reader.ReadBytes(result);return result;
}
int wmain(int argc,wchar_t**argv){
    setvbuf(stdout,nullptr,_IONBF,0);
    if(!lghost::testNativeCaptureLane()){puts("FAIL: capture timeout ownership");return 1;}
    puts("PASS: capture timeout ownership rejects late and duplicate completions");
    try{
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        auto manager=winrt::Windows::UI::Xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"LGEngine own-content probe",WS_POPUP,-10000,-10000,320,120,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        if(!window)winrt::throw_last_error();
        winrt::Windows::UI::Xaml::Hosting::DesktopWindowXamlSource source;
        auto native=source.as<IDesktopWindowXamlSourceNative>();winrt::check_hresult(native->AttachToWindow(window));
        HWND island{};winrt::check_hresult(native->get_WindowHandle(&island));SetWindowPos(island,nullptr,0,0,320,120,SWP_SHOWWINDOW);
        using namespace winrt::Windows::UI::Xaml;
        Controls::Grid root;root.Width(320);root.Height(120);
        Controls::TextBlock text;text.Text(L"Native content 42");text.FontSize(28);text.Foreground(Media::SolidColorBrush({255,255,255,255}));root.Children().Append(text);
        source.Content(root);ShowWindow(window,SW_SHOWNOACTIVATE);root.UpdateLayout();
        auto before=capture(root);size_t ink=0;for(size_t i=3;i<before.size();i+=4)if(before[i])++ink;
        auto visual=Hosting::ElementCompositionPreview::GetElementVisual(root);auto opacity=visual.Opacity();visual.Opacity(0);
        auto hidden=capture(root);size_t hiddenInk=0;for(size_t i=3;i<hidden.size();i+=4)if(hidden[i])++hiddenInk;
        visual.Opacity(opacity);
        printf("native pixels=%zu hidden-visual pixels=%zu identical=%d\n",ink,hiddenInk,before==hidden);
        bool usable=ink>100&&before==hidden;
        printf("%s: native XAML content survives composition-only hiding\n",usable?"PASS":"FAIL");
        if(usable&&argc==2){
            lghost::Options options;options.hostPath=argv[1];options.suffix=L"native"+std::to_wstring(GetCurrentProcessId());
            lghost::Spec spec;spec.width=320;spec.height=120;spec.x=spec.y=-10000;spec.hover=false;spec.motion=0;
            HRESULT status=E_PENDING;bool lossRestored=false;
            auto nativeWindow=lghost::nativeIslandWindow(root);bool geometry=nativeWindow&&lghost::nativePlace(root,nativeWindow,spec)&&spec.width==320&&spec.height==120;
            auto bridge=lghost::NativeContentSurface::create(root,options,spec,[root,nativeWindow](auto&s){return lghost::nativePlace(root,nativeWindow,s);},[&](HRESULT hr){status=hr;if(FAILED(hr))lossRestored=visual.Opacity()==opacity&&root.Opacity()==1&&root.IsHitTestVisible();});
            if(nativeWindow)bridge->watchNativeWindow(nativeWindow);
            auto pump=[&](auto done,unsigned ms=10000){auto deadline=GetTickCount64()+ms;while(!done()&&GetTickCount64()<deadline){MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}MsgWaitForMultipleObjects(0,nullptr,FALSE,10,QS_ALLINPUT);}return done();};
            pump([&]{return bridge->isPresented();});
            bool preserved=bridge->isPresented()&&root.Opacity()==1&&root.IsHitTestVisible()&&visual.Opacity()==0&&status==S_OK;
            auto revision=bridge->publishedContentRevision();text.Text(L"Native content changed: 43");
            bool changed=pump([&]{return bridge->publishedContentRevision()>revision;});
            pump([]{return false;},150);auto idleRevision=bridge->publishedContentRevision();pump([]{return false;},250);
            bool idle=bridge->publishedContentRevision()==idleRevision;
            auto oldPid=bridge->hostProcessId();lghost::Handle host(OpenProcess(PROCESS_TERMINATE|SYNCHRONIZE,FALSE,oldPid));
            bool ended=host.value&&TerminateProcess(host.value,77);if(ended)WaitForSingleObject(host.value,2000);
            bool recovered=ended&&pump([&]{return lossRestored&&bridge->isPresented()&&bridge->hostProcessId()!=oldPid;},15000);
            bridge->invalidateContents();bridge->suspend();auto suspendedRevision=bridge->publishedContentRevision();pump([]{return false;},150);
            bool suspended=!bridge->isPresented()&&visual.Opacity()==opacity&&bridge->publishedContentRevision()==suspendedRevision;
            bridge->resume();bool resumedAfterSuspend=pump([&]{return bridge->isPresented();});
            source.Content(nullptr);bool unloaded=pump([&]{return !bridge->isPresented()&&visual.Opacity()==opacity;});
            source.Content(root);root.UpdateLayout();bridge->invalidateContents();bool reloaded=pump([&]{return bridge->isPresented();});
            status=E_PENDING;root.Background(Media::SolidColorBrush({255,32,32,32}));bridge->invalidateContents();
            bool opaqueRejected=pump([&]{return status==E_NOTIMPL&&visual.Opacity()==opacity;});
            root.Background(nullptr);bridge->invalidateContents();bool resumed=pump([&]{return bridge->isPresented();});
            bridge->stop();bool restored=visual.Opacity()==opacity;
            HRESULT inspectionResult=E_PENDING;lghost::NativeContentInfo inspectionInfo;
            lghost::inspectNativeContent(root,[&](HRESULT hr,lghost::NativeContentInfo info){inspectionResult=hr;inspectionInfo=info;});
            bool inspected=pump([&]{return inspectionResult!=E_PENDING;})&&inspectionResult==S_OK&&inspectionInfo.unchanged&&inspectionInfo.ink>0&&inspectionInfo.pixelWidth>0&&inspectionInfo.bytes==size_t(inspectionInfo.pixelWidth)*inspectionInfo.pixelHeight*4&&!wcscmp(inspectionInfo.stage,L"complete");
            auto cancelledControl=std::make_shared<lghost::NativeInspectionControl>();cancelledControl->cancel();HRESULT cancelledResult=E_PENDING;
            lghost::inspectNativeContent(root,[&](HRESULT hr,lghost::NativeContentInfo){cancelledResult=hr;},cancelledControl);
            bool cancellation=cancelledResult==HRESULT_FROM_WIN32(ERROR_CANCELLED)&&visual.Opacity()==opacity;
            printf("%s: cancelled inspection leaves native content unchanged\n",cancellation?"PASS":"FAIL");
            printf("%s: original host presents native content; XAML opacity and hit-testing unchanged\n",preserved?"PASS":"FAIL");
            printf("%s: native island geometry resolved without foreground-window guessing\n",geometry?"PASS":"FAIL");
            printf("%s: native text changes publish new content without replacing controls\n",changed?"PASS":"FAIL");
            printf("%s: unchanged native content does not cause a capture feedback loop\n",idle?"PASS":"FAIL");
            printf("%s: dedicated host loss restores native visual and reconnect recreates glass\n",recovered?"PASS":"FAIL");
            printf("%s: opaque capture is rejected without hiding native UI; valid content resumes\n",opaqueRejected&&resumed?"PASS":"FAIL");
            printf("%s: suspension rejects pending snapshots and resume recreates content\n",suspended&&resumedAfterSuspend?"PASS":"FAIL");
            printf("%s: unloading native tree restores visual and reloading recreates content\n",unloaded&&reloaded?"PASS":"FAIL");
            printf("%s: stopping bridge restores original composition visual\n",restored?"PASS":"FAIL");usable=preserved&&restored;
            printf("%s: diagnostic reports actual pixel size, byte count and completion stage\n",inspected?"PASS":"FAIL");
            usable=usable&&changed&&recovered&&idle&&geometry&&opaqueRejected&&resumed&&inspected&&cancellation&&unloaded&&reloaded&&suspended&&resumedAfterSuspend;
        }
        source.Close();DestroyWindow(window);manager.Close();return usable?0:1;
    }catch(winrt::hresult_error const&e){printf("UNAVAILABLE: XAML content probe HRESULT 0x%08lx\n",static_cast<unsigned long>(e.code().value));return 2;}
}
