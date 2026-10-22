#pragma once
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Automation.h>
#include <winrt/Windows.UI.Xaml.Automation.Peers.h>
namespace liquid {
struct CapsulePlacement {double left=0,right=0,apps=0,tray=0;bool valid=false;};
inline CapsulePlacement placeCapsule(double width,double appLeft,double appRight,double trayLeft,double trayRight){
    double apps=appRight-appLeft,tray=trayRight-trayLeft,total=apps+8+tray;
    if(apps<=0||tray<=0||total>width-16)return {};
    double left=(width-total)/2;return {left,left+total,left-appLeft,left+apps+8-trayLeft,true};
}
// The element's layout position in host space without its own RenderTransform, so a measure never reads back the capsule's shift.
inline double naturalLeft(winrt::Windows::UI::Xaml::FrameworkElement const&e,winrt::Windows::UI::Xaml::UIElement const&host){
    auto parent=winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(e).try_as<winrt::Windows::UI::Xaml::UIElement>();auto o=e.ActualOffset();
    return parent?(double)parent.TransformToVisual(host).TransformPoint({o.x,o.y}).X:(double)e.TransformToVisual(host).TransformPoint({0,0}).X;
}
// The id UI Automation reports: the attached AutomationId, else the peer's. Not x:Name: every experience button is LaunchListButton.
inline std::wstring automationId(winrt::Windows::UI::Xaml::FrameworkElement const&e){
    std::wstring id;try{id=winrt::Windows::UI::Xaml::Automation::AutomationProperties::GetAutomationId(e).c_str();}catch(...){}
    if(id.empty())try{if(auto peer=winrt::Windows::UI::Xaml::Automation::Peers::FrameworkElementAutomationPeer::FromElement(e))id=peer.GetAutomationId().c_str();}catch(...){}
    return id;
}
// Widgets is the one experience button that may stand apart from the apps. Search is as wide, so judge by id; width only without one.
inline bool joinsCapsule(std::wstring const&id,double span,double height){return id.empty()?span<=height*2:id!=L"WidgetsButton";}
// XAML transforms move both visuals and hit testing. Never change the native
// taskbar window, monitor work area, or the user's taskbar alignment preference.
class CapsuleLayout {
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    using Transform=winrt::Windows::UI::Xaml::Media::Transform;
    using Group=winrt::Windows::UI::Xaml::Media::TransformGroup;
    using Translation=winrt::Windows::UI::Xaml::Media::TranslateTransform;
    struct Target {winrt::weak_ref<Element> element;Transform original{nullptr};Group group{nullptr};Translation shift{nullptr};bool used=false,applied=false;};
    std::vector<Target> targets;std::function<void(const std::wstring&)> log;
    static void restore(Target&t){try{if(auto e=t.element.get())if(e.RenderTransform()==t.group)e.RenderTransform(t.original);}catch(...){}}
    Target&target(Element const&e){for(auto&t:targets)if(t.element.get()==e){t.used=true;return t;}
        Target t;t.element=winrt::make_weak(e);t.original=e.RenderTransform();t.group=Group();t.shift=Translation();if(t.original){winrt::Windows::UI::Xaml::Media::MatrixTransform copy;auto origin=t.original.TransformPoint({0,0}),x=t.original.TransformPoint({1,0}),y=t.original.TransformPoint({0,1});copy.Matrix({x.X-origin.X,x.Y-origin.Y,y.X-origin.X,y.Y-origin.Y,origin.X,origin.Y});t.group.Children().Append(copy);}t.group.Children().Append(t.shift);t.used=true;targets.push_back(std::move(t));return targets.back();}
    void move(Element const&e,double x){auto&t=target(e);auto current=e.RenderTransform();if(t.applied&&current!=t.group)throw winrt::hresult_error(E_ABORT);
        if(std::abs(t.shift.X()-x)>.25)t.shift.X(x);if(!t.applied){e.RenderTransform(t.group);t.applied=true;}}
public:
    explicit CapsuleLayout(std::function<void(const std::wstring&)> logger={}):log(std::move(logger)){}
    ~CapsuleLayout(){stop();}
    void stop(){for(auto&t:targets)restore(t);targets.clear();}
    CapsulePlacement apply(std::vector<Element>const&icons,Element const&tray,Element const&host){try{
        if(icons.empty()||!tray||!host||host.ActualWidth()<host.ActualHeight()){if(log)log(L"capsule layout missing icons="+std::to_wstring(icons.size())+L" tray="+std::to_wstring((bool)tray)+L" host="+std::to_wstring((bool)host));stop();return {};}
        double left=host.ActualWidth(),right=0;for(auto const&e:icons){double x=naturalLeft(e,host);left=std::min(left,x);right=std::max(right,x+e.ActualWidth());}
        double trayLeft=naturalLeft(tray,host);auto plan=placeCapsule(host.ActualWidth(),left,right,trayLeft,trayLeft+tray.ActualWidth());if(log)log(L"capsule layout measure width="+std::to_wstring(host.ActualWidth())+L" apps="+std::to_wstring(left)+L","+std::to_wstring(right)+L" tray="+std::to_wstring(trayLeft)+L","+std::to_wstring(trayLeft+tray.ActualWidth())+L" valid="+std::to_wstring(plan.valid));if(!plan.valid){stop();return {};}
        for(auto&t:targets)t.used=false;for(auto const&e:icons)move(e,plan.apps);move(tray,plan.tray);
        for(auto i=targets.begin();i!=targets.end();)if(!i->used){restore(*i);i=targets.erase(i);}else ++i;
        host.UpdateLayout();
        double actualLeft=host.ActualWidth(),actualRight=0;for(auto const&e:icons){auto point=e.TransformToVisual(host).TransformPoint({0,0});actualLeft=std::min(actualLeft,(double)point.X);actualRight=std::max(actualRight,(double)point.X+e.ActualWidth());}auto actualTray=tray.TransformToVisual(host).TransformPoint({0,0});double gap=actualTray.X-actualRight;
        if(log)log(L"capsule layout actual apps="+std::to_wstring(actualLeft)+L","+std::to_wstring(actualRight)+L" tray="+std::to_wstring(actualTray.X)+L" gap="+std::to_wstring(gap));
        // TransformToVisual can lag until the next XAML layout/render pass; the queued remeasure observes the settled position.
        return plan;
    }catch(...){if(log)log(L"capsule layout exception="+std::to_wstring((long)winrt::to_hresult()));stop();return {};}}
};
inline bool testCapsulePlacement(){auto p=placeCapsule(1920,550,1350,1600,1900);return p.valid&&p.left==406&&p.right==1514&&550+p.apps==406&&1600+p.tray==1214&&1214-(1350+p.apps)==8&&!placeCapsule(900,0,800,800,1000).valid&&!placeCapsule(1920,550,550,1600,1900).valid
    &&joinsCapsule(L"SearchButton",240,48)&&joinsCapsule(L"StartButton",48,48)&&joinsCapsule(L"",48,48)&&!joinsCapsule(L"WidgetsButton",48,48)&&!joinsCapsule(L"",240,48);}
}
