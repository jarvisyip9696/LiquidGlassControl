#pragma once
// Standalone composition blur. No Windhawk SDK or runtime calls.
#undef GetCurrentTime
#include <functional>
#include <array>
#include <algorithm>
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
    bool matrix=false,adaptiveTint=false,transform=false,explicitTint=false;float tintR=1,tintG=1,tintB=1;float blur=12,opacity=.35f;bool dark=false;float width=0,height=0,magnification=1.035f;
    fx::IGraphicsEffectSource source{nullptr};winrt::hstring name;
    winrt::hstring Name(){return name;}void Name(winrt::hstring const&v){name=v;}
    HRESULT STDMETHODCALLTYPE GetEffectId(GUID*out)noexcept override{if(!out)return E_POINTER;*out=transform?CLSID_D2D12DAffineTransform:matrix?CLSID_D2D1ColorMatrix:CLSID_D2D1GaussianBlur;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetNamedPropertyMapping(LPCWSTR,UINT*,UINT*)noexcept override{return E_INVALIDARG;}
    HRESULT STDMETHODCALLTYPE GetPropertyCount(UINT*out)noexcept override{if(!out)return E_POINTER;*out=transform?4:3;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSourceCount(UINT*out)noexcept override{if(!out)return E_POINTER;*out=1;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetSource(UINT i,IUnknown**out)noexcept override{if(!out)return E_POINTER;*out=nullptr;auto value=i==0?source:nullptr;if(!value)return E_INVALIDARG;return winrt::get_unknown(value)->QueryInterface(winrt::guid_of<fx::IGraphicsEffectSource>(),(void**)out);}
    HRESULT STDMETHODCALLTYPE GetProperty(UINT i,IUnknown**out)noexcept override try{
        if(!out)return E_POINTER;*out=nullptr;if(i>(transform?3u:2u))return E_INVALIDARG;
        winrt::Windows::Foundation::IPropertyValue value{nullptr};
        using V=winrt::Windows::Foundation::PropertyValue;
        // Source-over tint, as in the Styler's flood/composite effect. Alpha
        // must receive the tint as well, including over a transparent backdrop.
        if(transform){if(i==2){float zoom=std::clamp(magnification,1.f,1.08f);float values[6]={zoom,0,0,zoom,width*(1-zoom)*.5f,height*(1-zoom)*.5f};value=V::CreateSingleArray(values).as<winrt::Windows::Foundation::IPropertyValue>();}else if(i==3)value=V::CreateSingle(0.f).as<winrt::Windows::Foundation::IPropertyValue>();else value=V::CreateUInt32(i==1?0u:1u).as<winrt::Windows::Foundation::IPropertyValue>();}
        else if(adaptiveTint&&i==0){
            // Continuous background luminance controls the tint. All coefficients
            // stay in the premultiplied range; no unsupported clamping node.
            float a=1-opacity,k=.88f*opacity,o=.08f*opacity;
            float r=.299f*k,g=.587f*k,b=.114f*k;
            float values[20]={a+r,r,r,0,g,a+g,g,0,b,b,a+b,0,0,0,0,a,o,o,o,opacity};
            value=V::CreateSingleArray(values).as<winrt::Windows::Foundation::IPropertyValue>();
        }
        else if(matrix&&i==0){float a=1-opacity,t=dark?.08f:.96f;float values[20]={a,0,0,0,0,a,0,0,0,0,a,0,0,0,0,a,(explicitTint?tintR:t)*opacity,(explicitTint?tintG:t)*opacity,(explicitTint?tintB:t)*opacity,opacity};value=V::CreateSingleArray(values).as<winrt::Windows::Foundation::IPropertyValue>();}
        else if(!matrix&&i==0)value=V::CreateSingle(blur).as<winrt::Windows::Foundation::IPropertyValue>();
        else if(matrix&&i==2)value=V::CreateBoolean(false).as<winrt::Windows::Foundation::IPropertyValue>();
        else value=V::CreateUInt32(1u).as<winrt::Windows::Foundation::IPropertyValue>();
        *out=(IUnknown*)winrt::detach_abi(value);return S_OK;
    }catch(...){return winrt::to_hresult();}
};
inline bool testTint(){try{
    {auto lens=winrt::make_self<Effect>();lens->transform=true;lens->width=400;lens->height=200;winrt::com_ptr<IUnknown> encoded;if(FAILED(lens->GetProperty(2,encoded.put())))return false;winrt::com_array<float> m;encoded.as<winrt::Windows::Foundation::IPropertyValue>().GetSingleArray(m);if(m.size()!=6||m[0]<=1||std::abs(200*m[0]+m[4]-200)>.0001f||std::abs(100*m[3]+m[5]-100)>.0001f)return false;}
    for(bool dark:{false,true})for(float opacity:{0.f,.35f,.82f,1.f}){
        auto effect=winrt::make_self<Effect>();effect->matrix=true;effect->dark=dark;effect->opacity=opacity;winrt::com_ptr<IUnknown> encoded;if(FAILED(effect->GetProperty(0,encoded.put())))return false;
        winrt::com_array<float> matrix;encoded.as<winrt::Windows::Foundation::IPropertyValue>().GetSingleArray(matrix);if(matrix.size()!=20)return false;
        for(float alpha:{0.f,.5f,1.f}){float input[4]={.2f*alpha,.4f*alpha,.6f*alpha,alpha};for(int channel=0;channel<4;++channel){float actual=matrix[16+channel];for(int source=0;source<4;++source)actual+=input[source]*matrix[source*4+channel];float expected=input[channel]*(1-opacity)+(channel==3?1.f:dark?.08f:.96f)*opacity;if(std::abs(actual-expected)>0.00001f)return false;}}
    }
    auto adaptive=winrt::make_self<Effect>();adaptive->matrix=true;adaptive->adaptiveTint=true;adaptive->opacity=.35f;
    auto apply=[](winrt::com_ptr<Effect> const&effect,std::array<float,4> input){
        winrt::com_ptr<IUnknown> encoded;if(FAILED(effect->GetProperty(0,encoded.put())))throw winrt::hresult_error(E_FAIL);
        winrt::com_array<float> m;encoded.as<winrt::Windows::Foundation::IPropertyValue>().GetSingleArray(m);std::array<float,4> out{};
        for(int c=0;c<4;++c){out[c]=m[16+c];for(int j=0;j<4;++j)out[c]+=input[j]*m[j*4+c];out[c]=std::max(0.f,std::min(1.f,out[c]));}return out;
    };
    auto keyboard=winrt::make_self<Effect>();keyboard->matrix=true;keyboard->dark=true;keyboard->opacity=.82f;
    auto whiteBackdrop=apply(keyboard,{1,1,1,1});
    float luminance=std::pow((whiteBackdrop[0]+.055f)/1.055f,2.4f);
    if(1.05f/(luminance+.05f)<4.5f)return false;
    float previous=-1;
    for(float gray:{0.f,110.f/255,138.f/255,165.f/255,1.f}){
        auto tone=apply(adaptive,{gray,gray,gray,1});
        if(tone[0]<previous||std::abs(tone[3]-1.f)>.00001f||tone[0]>tone[3])return false;previous=tone[0];
        float amount=gray;
        if(std::abs(tone[0]-(gray*.65f+(.08f+.88f*amount)*.35f))>.00001f)return false;
    }
    for(float opacity:{.05f,.35f,.9f})for(float alpha:{0.f,.5f,1.f}){
        adaptive->opacity=opacity;auto sample=apply(adaptive,{.1f*alpha,.3f*alpha,.7f*alpha,alpha});
        for(int c=0;c<3;++c)if(sample[c]<0||sample[c]>sample[3]+.00001f)return false;
    }
    return true;
}catch(...){return false;}}
template<class Compositor>auto makeGlass(Compositor const&compositor,glass::Config const&c,bool hostBackdrop=false,float width=0,float height=0,winrt::Windows::UI::Color const* color=nullptr){
    auto gaussian=winrt::make_self<Effect>();gaussian->blur=(float)c.blur;
    // Both composition frameworks consume the standard graphics effect source.
    using Param=std::conditional_t<std::is_same_v<Compositor,winrt::Windows::UI::Composition::Compositor>,winrt::Windows::UI::Composition::CompositionEffectSourceParameter,winrt::Microsoft::UI::Composition::CompositionEffectSourceParameter>;
    // Shell fallback remains a supported blur/tint graph. Full spatial refraction belongs to the LGEngine taskbar bridge, not this affine approximation.
    (void)width;(void)height;gaussian->source=Param(L"Backdrop");auto tint=winrt::make_self<Effect>();tint->matrix=true;tint->opacity=c.opacity/100.f;tint->dark=c.tint==2;tint->source=*gaussian;
    fx::IGraphicsEffect graph=*tint;
    tint->adaptiveTint=c.tint==3&&!color;if(color){tint->explicitTint=true;tint->tintR=color->R/255.f;tint->tintG=color->G/255.f;tint->tintB=color->B/255.f;}
    auto result=compositor.CreateEffectFactory(graph).CreateBrush();auto backdrop=compositor.CreateBackdropBrush();if constexpr(std::is_same_v<Compositor,winrt::Windows::UI::Composition::Compositor>){if(hostBackdrop)backdrop=compositor.CreateHostBackdropBrush();}result.SetSourceParameter(L"Backdrop",backdrop);return result;
}
struct SystemBrush:winrt::Windows::UI::Xaml::Media::XamlCompositionBrushBaseT<SystemBrush>{
    winrt::Windows::UI::Composition::Compositor compositor{nullptr};glass::Config config;std::function<void(HRESULT)> completed;bool hostBackdrop=false;float width=0,height=0;winrt::Windows::UI::Color overrideTint{};bool hasOverrideTint=false;
    SystemBrush(winrt::Windows::UI::Xaml::UIElement const&e,glass::Config c,std::function<void(HRESULT)> callback,bool useHostBackdrop=false,winrt::Windows::UI::Color const* color=nullptr):compositor(winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e).Compositor()),config(c),completed(std::move(callback)),hostBackdrop(useHostBackdrop){if(color){overrideTint=*color;hasOverrideTint=true;}if(auto frame=e.try_as<winrt::Windows::UI::Xaml::FrameworkElement>()){width=(float)frame.ActualWidth();height=(float)frame.ActualHeight();}FallbackColor(c.tint==2?winrt::Windows::UI::Color{255,24,32,44}:winrt::Windows::UI::Color{255,244,247,252});}
    void OnConnected(){try{if(!CompositionBrush())CompositionBrush(config.economy?compositor.CreateColorBrush(FallbackColor()).as<winrt::Windows::UI::Composition::CompositionBrush>():makeGlass(compositor,config,hostBackdrop,width,height,hasOverrideTint?&overrideTint:nullptr).as<winrt::Windows::UI::Composition::CompositionBrush>());completed(config.economy?S_FALSE:S_OK);}catch(...){completed(winrt::to_hresult());CompositionBrush(compositor.CreateColorBrush(FallbackColor()));}}
    void OnDisconnected(){CompositionBrush(nullptr);}
};
struct ModernBrush:winrt::Microsoft::UI::Xaml::Media::XamlCompositionBrushBaseT<ModernBrush>{
    winrt::Microsoft::UI::Composition::Compositor compositor{nullptr};glass::Config config;std::function<void(HRESULT)> completed;float width=0,height=0;
    ModernBrush(winrt::Microsoft::UI::Xaml::UIElement const&e,glass::Config c,std::function<void(HRESULT)> callback):compositor(winrt::Microsoft::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e).Compositor()),config(c),completed(std::move(callback)){if(auto frame=e.try_as<winrt::Microsoft::UI::Xaml::FrameworkElement>()){width=(float)frame.ActualWidth();height=(float)frame.ActualHeight();}FallbackColor(c.tint==2?winrt::Windows::UI::Color{255,24,32,44}:winrt::Windows::UI::Color{255,244,247,252});}
    void OnConnected(){try{if(!CompositionBrush())CompositionBrush(config.economy?compositor.CreateColorBrush(FallbackColor()).as<winrt::Microsoft::UI::Composition::CompositionBrush>():makeGlass(compositor,config,false,width,height).as<winrt::Microsoft::UI::Composition::CompositionBrush>());completed(config.economy?S_FALSE:S_OK);}catch(...){completed(winrt::to_hresult());CompositionBrush(compositor.CreateColorBrush(FallbackColor()));}}
    void OnDisconnected(){CompositionBrush(nullptr);}
};
}
