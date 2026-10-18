#pragma once
namespace liquid {
inline float pillRadius(float width,float height){return std::max(0.f,std::min(width,height)/2);}
template<class Element>std::function<void()> watchPillRadius(Element const&e,std::function<void(float)> apply){
    auto update=[apply](Element const&element){float r=pillRadius((float)element.ActualWidth(),(float)element.ActualHeight());if(r>0)apply(r);};update(e);
    auto token=e.SizeChanged([update](auto const&sender,auto const&){try{update(sender.template as<Element>());}catch(...){}});auto weak=winrt::make_weak(e);
    return [weak,token]{try{if(auto element=weak.get())element.SizeChanged(token);}catch(...){}};
}
// StackPanel has no CornerRadius property. Clip its composition visual without
// changing layout, hit targets, or taskbar window bounds. Restore the exact clip.
inline std::function<void()> clipCapsule(IInspectable*p){
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    winrt::Windows::Foundation::IInspectable obj{nullptr};winrt::copy_from_abi(obj,p);auto e=obj.try_as<Element>();if(!e)return {};bool tray=e.Name()==L"SystemTrayFrameGrid";if(!tray&&e.Name()!=L"ConfirmatorMainGrid"&&e.Name()!=L"VolumeConfirmator"&&e.Name()!=L"BrightnessConfirmator")return {};
    auto visual=winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e);auto original=visual.Clip();auto geometry=visual.Compositor().CreateRoundedRectangleGeometry();auto clip=visual.Compositor().CreateGeometricClip(geometry);
    auto update=[geometry,tray](Element const&element){float w=(float)element.ActualWidth(),h=(float)element.ActualHeight(),vertical=tray?std::min(h/4,std::max(6.f,(h-36.f)/2)):0,horizontal=tray?std::min(4.f,w/4):0;geometry.Offset({horizontal,vertical});geometry.Size({std::max(0.f,w-2*horizontal),std::max(0.f,h-2*vertical)});float r=std::min(std::max(0.f,w-2*horizontal),std::max(0.f,h-2*vertical))/2;geometry.CornerRadius({r,r});};update(e);
    auto token=e.SizeChanged([update](auto const&sender,auto const&){try{update(sender.template as<Element>());}catch(...){}});
    try{visual.Clip(clip);}catch(...){e.SizeChanged(token);throw;}
    auto weak=winrt::make_weak(e);
    return [weak,original,clip,token]{try{if(auto element=weak.get()){element.SizeChanged(token);auto v=winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);if(v.Clip()==clip)v.Clip(original);}}catch(...){}};
}
}
