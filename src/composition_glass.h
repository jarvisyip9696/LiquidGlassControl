#pragma once
// Standalone composition blur. No Windhawk SDK or runtime calls.
#undef GetCurrentTime
#include <functional>
#include <cmath>
#include <d2d1effects.h>
#include <d2d1_1.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Xaml.Hosting.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
struct GlassEffectInterop:IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetEffectId(GUID*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,UINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT*)=0;
    virtual HRESULT STDMETHODCALLTYPE GetProperty(UINT,IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetSource(UINT,IUnknown**)=0;
    virtual HRESULT STDMETHODCALLTYPE GetSourceCount(UINT*)=0;
};
#ifdef __MINGW32__
__CRT_UUID_DECL(GlassEffectInterop,0x2fc57384,0xa068,0x44d7,0xa3,0x31,0x30,0x98,0x2f,0xcf,0x71,0x77)
#else
template<> inline constexpr winrt::guid winrt::impl::guid_v<GlassEffectInterop>{0x2fc57384,0xa068,0x44d7,{0xa3,0x31,0x30,0x98,0x2f,0xcf,0x71,0x77}};
#endif
namespace liquid {
namespace fx=winrt::Windows::Graphics::Effects;
struct Effect:winrt::implements<Effect,fx::IGraphicsEffect,fx::IGraphicsEffectSource,GlassEffectInterop>{
    bool matrix=false;float blur=12,opacity=.35f;bool dark=false;
    fx::IGraphicsEffectSource source{nullptr};winrt::hstring name;
    winrt::hstring Name(){return name;}void Name(winrt::hstring const&v){name=v;}
    HRESULT STDMETHODCALLTYPE GetEffectId(GUID*out)noexcept override{if(!out)return E_POINTER;*out=matrix?CLSID_D2D1ColorMatrix:CLSID_D2D1GaussianBlur;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,UINT*)noexcept override{return E_INVALIDARG;}
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT*out)noexcept override{if(!out)return E_POINTER;*out=3;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT*out)noexcept override{if(!out)return E_POINTER;*out=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSource(UINT i,IUnknown**out)noexcept override{if(!out)return E_POINTER;*out=nullptr;if(i||!source)return E_INVALIDARG;return winrt::get_unknown(source)->QueryInterface(winrt::guid_of<fx::IGraphicsEffectSource>(),(void**)out);}
    HRESULT STDMETHODCALLTYPE GetProperty(UINT i,IUnknown**out)noexcept override try{
        if(!out)return E_POINTER;*out=nullptr;if(i>2)return E_INVALIDARG;
        winrt::Windows::Foundation::IPropertyValue value{nullptr};
        using V=winrt::Windows::Foundation::PropertyValue;
        // Source-over tint, as in the Styler's flood/composite effect. Alpha
        // must receive the tint as well, including over a transparent backdrop.
        if(matrix&&i==0){float a=1-opacity,t=dark?.08f:.96f;float values[20]={a,0,0,0,0,a,0,0,0,0,a,0,0,0,0,a,t*opacity,t*opacity,t*opacity,opacity};value=V::CreateSingleArray(values).as<winrt::Windows::Foundation::IPropertyValue>();}
        else if(!matrix&&i==0)value=V::CreateSingle(blur).as<winrt::Windows::Foundation::IPropertyValue>();
        else if(matrix&&i==2)value=V::CreateBoolean(false).as<winrt::Windows::Foundation::IPropertyValue>();
        else value=V::CreateUInt32(1u).as<winrt::Windows::Foundation::IPropertyValue>();
        *out=(IUnknown*)winrt::detach_abi(value);return S_OK;
    }catch(...){return winrt::to_hresult();}
};
inline bool testTint(){try{
    for(bool dark:{false,true})for(float opacity:{0.f,.35f,1.f}){
        auto effect=winrt::make_self<Effect>();effect->matrix=true;effect->dark=dark;effect->opacity=opacity;winrt::com_ptr<IUnknown> encoded;if(FAILED(effect->GetProperty(0,encoded.put())))return false;
        winrt::com_array<float> matrix;encoded.as<winrt::Windows::Foundation::IPropertyValue>().GetSingleArray(matrix);if(matrix.size()!=20)return false;
        for(float alpha:{0.f,.5f,1.f}){float input[4]={.2f*alpha,.4f*alpha,.6f*alpha,alpha};for(int channel=0;channel<4;++channel){float actual=matrix[16+channel];for(int source=0;source<4;++source)actual+=input[source]*matrix[source*4+channel];float expected=input[channel]*(1-opacity)+(channel==3?1.f:dark?.08f:.96f)*opacity;if(std::abs(actual-expected)>0.00001f)return false;}}
    }
    return true;
}catch(...){return false;}}
template<class Compositor>auto makeGlass(Compositor const&compositor,glass::Config const&c,bool hostBackdrop=false){
    auto gaussian=winrt::make_self<Effect>();gaussian->blur=(float)c.blur;
    // Both composition frameworks consume the standard graphics effect source.
    using Param=std::conditional_t<std::is_same_v<Compositor,winrt::Windows::UI::Composition::Compositor>,winrt::Windows::UI::Composition::CompositionEffectSourceParameter,winrt::Microsoft::UI::Composition::CompositionEffectSourceParameter>;
    gaussian->source=Param(L"Backdrop");auto tint=winrt::make_self<Effect>();tint->matrix=true;tint->opacity=c.opacity/100.f;tint->dark=c.tint==2;tint->source=*gaussian;
    auto result=compositor.CreateEffectFactory(*tint).CreateBrush();auto backdrop=compositor.CreateBackdropBrush();if constexpr(std::is_same_v<Compositor,winrt::Windows::UI::Composition::Compositor>){if(hostBackdrop)backdrop=compositor.CreateHostBackdropBrush();}result.SetSourceParameter(L"Backdrop",backdrop);return result;
}
struct SystemBrush:winrt::Windows::UI::Xaml::Media::XamlCompositionBrushBaseT<SystemBrush>{
    winrt::Windows::UI::Composition::Compositor compositor{nullptr};glass::Config config;std::function<void(HRESULT)> completed;bool hostBackdrop=false;
    SystemBrush(winrt::Windows::UI::Xaml::UIElement const&e,glass::Config c,std::function<void(HRESULT)> callback,bool useHostBackdrop=false):compositor(winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e).Compositor()),config(c),completed(std::move(callback)),hostBackdrop(useHostBackdrop){FallbackColor(c.tint==2?winrt::Windows::UI::Color{255,24,32,44}:winrt::Windows::UI::Color{255,244,247,252});}
    void OnConnected(){try{if(!CompositionBrush())CompositionBrush(config.economy?compositor.CreateColorBrush(FallbackColor()).as<winrt::Windows::UI::Composition::CompositionBrush>():makeGlass(compositor,config,hostBackdrop).as<winrt::Windows::UI::Composition::CompositionBrush>());completed(config.economy?S_FALSE:S_OK);}catch(...){completed(winrt::to_hresult());CompositionBrush(compositor.CreateColorBrush(FallbackColor()));}}
    void OnDisconnected(){CompositionBrush(nullptr);}
};
struct ModernBrush:winrt::Microsoft::UI::Xaml::Media::XamlCompositionBrushBaseT<ModernBrush>{
    winrt::Microsoft::UI::Composition::Compositor compositor{nullptr};glass::Config config;std::function<void(HRESULT)> completed;
    ModernBrush(winrt::Microsoft::UI::Xaml::UIElement const&e,glass::Config c,std::function<void(HRESULT)> callback):compositor(winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e).Compositor()),config(c),completed(std::move(callback)){FallbackColor(c.tint==2?winrt::Windows::UI::Color{255,24,32,44}:winrt::Windows::UI::Color{255,244,247,252});}
    void OnConnected(){try{if(!CompositionBrush())CompositionBrush(config.economy?compositor.CreateColorBrush(FallbackColor()).as<winrt::Microsoft::UI::Composition::CompositionBrush>():makeGlass(compositor,config).as<winrt::Microsoft::UI::Composition::CompositionBrush>());completed(config.economy?S_FALSE:S_OK);}catch(...){completed(winrt::to_hresult());CompositionBrush(compositor.CreateColorBrush(FallbackColor()));}}
    void OnDisconnected(){CompositionBrush(nullptr);}
};
}
