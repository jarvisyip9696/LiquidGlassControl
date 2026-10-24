#pragma once
#include "lgengine_native_place.h"
namespace lghost {
struct NativeInspectionControl {
    winrt::Windows::Foundation::IAsyncInfo operation{nullptr};bool cancelled=false;
    void cancel()noexcept{cancelled=true;try{if(operation)operation.Cancel();}catch(...){}}
    void check()const{if(cancelled)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_CANCELLED));}
};
struct NativeContentInfo {int width=0,height=0,x=0,y=0,rootWidth=0,rootHeight=0,pixelWidth=0,pixelHeight=0;size_t ink=0,opaque=0,bytes=0;bool geometry=false,unchanged=false;const wchar_t* stage=L"layout";};
// Diagnostic only: samples the existing XAML tree, never hides it or starts a
// glass client. The callback receives counts/geometry, never text or pixels.
inline winrt::fire_and_forget inspectNativeContent(winrt::Windows::UI::Xaml::FrameworkElement element,std::function<void(HRESULT,NativeContentInfo)> report,std::shared_ptr<NativeInspectionControl> control=std::make_shared<NativeInspectionControl>()){
    NativeContentInfo info;HRESULT result=S_OK;
    try{
        control->check();
        auto opacity=element.Opacity();bool hit=element.IsHitTestVisible();
        auto visual=winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);auto visualOpacity=visual.Opacity();
        Spec spec;auto root=element.XamlRoot();double scale=root.RasterizationScale();
        if(auto content=root.Content().try_as<winrt::Windows::UI::Xaml::FrameworkElement>()){info.rootWidth=int(std::lround(content.ActualWidth()*scale));info.rootHeight=int(std::lround(content.ActualHeight()*scale));}
        spec.width=int(std::lround(element.ActualWidth()*scale));spec.height=int(std::lround(element.ActualHeight()*scale));
        auto window=nativeIslandWindow(element);info.geometry=window&&nativePlace(element,window,spec);
        // Inspection can still establish capture support when placement cannot
        // be resolved. Production presentation must require geometry=true.
        if(!wire::valid(spec))throw winrt::hresult_error(E_INVALIDARG);
        info.width=spec.width;info.height=spec.height;info.x=spec.x;info.y=spec.y;
        winrt::Windows::Foundation::IInspectable object{nullptr};
        auto className=winrt::hstring(L"Windows.UI.Xaml.Media.Imaging.RenderTargetBitmap");
        info.stage=L"activate";
        winrt::check_hresult(RoActivateInstance(reinterpret_cast<HSTRING>(winrt::get_abi(className)),reinterpret_cast<IInspectable**>(winrt::put_abi(object))));
        winrt::com_ptr<NativeRenderBitmap> bitmap;const GUID id{0x500dee81,0x893c,0x4c0a,{0x8f,0xec,0x46,0x78,0xac,0x71,0x75,0x89}};
        info.stage=L"interface";winrt::check_hresult(winrt::get_unknown(object)->QueryInterface(id,bitmap.put_void()));
        auto ui=element.as<winrt::Windows::UI::Xaml::UIElement>();winrt::Windows::Foundation::IAsyncAction render{nullptr};
        info.stage=L"render-call";winrt::check_hresult(bitmap->RenderSized(winrt::get_abi(ui),std::max(1,int(std::lround(spec.width/scale))),std::max(1,int(std::lround(spec.height/scale))),winrt::put_abi(render)));control->operation=render.as<winrt::Windows::Foundation::IAsyncInfo>();info.stage=L"render-await";co_await render;control->check();
        info.stage=L"pixel-size";winrt::check_hresult(bitmap->Width(&info.pixelWidth));winrt::check_hresult(bitmap->Height(&info.pixelHeight));
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::Storage::Streams::IBuffer> operation{nullptr};info.stage=L"pixels-call";winrt::check_hresult(bitmap->Pixels(winrt::put_abi(operation)));control->operation=operation.as<winrt::Windows::Foundation::IAsyncInfo>();info.stage=L"pixels-await";auto buffer=co_await operation;control->check();
        info.stage=L"buffer-length";info.bytes=buffer.Length();
        if(info.pixelWidth<1||info.pixelHeight<1||info.pixelWidth>4096||info.pixelHeight>4096||uint64_t(info.pixelWidth)*info.pixelHeight>4000000||info.bytes!=size_t(info.pixelWidth)*info.pixelHeight*4)throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
        info.stage=L"read-pixels";
        std::vector<uint8_t> pixels(buffer.Length());winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer).ReadBytes(pixels);
        for(size_t i=3;i<pixels.size();i+=4){if(pixels[i])++info.ink;if(pixels[i]>250)++info.opaque;}
        info.unchanged=element.Opacity()==opacity&&element.IsHitTestVisible()==hit&&visual.Opacity()==visualOpacity;
        if(!info.unchanged)result=E_ABORT;
        info.stage=L"complete";
    }catch(...){result=winrt::to_hresult();}
    control->operation=nullptr;
    try{report(result,info);}catch(...){}
}
}
