#pragma once
#include <winrt/Windows.UI.Xaml.Input.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <memory>
#include <vector>
#include <functional>
namespace liquid {
// Animate only native image ink. Button bounds and routed clicks stay native.
// XAML transforms are included in RenderTargetBitmap, unlike compositor-only
// expressions. Restore only transforms still owned by this instance.
class TaskbarContentMotion:public std::enable_shared_from_this<TaskbarContentMotion>{
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    using Args=winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs;
    using Transform=winrt::Windows::UI::Xaml::Media::Transform;
    struct Icon {winrt::weak_ref<Element> image;Transform old{nullptr};winrt::Windows::UI::Xaml::Media::TransformGroup group{nullptr};winrt::Windows::UI::Xaml::Media::ScaleTransform scale{nullptr};float value=1,target=1;bool hovered=false;};
    winrt::weak_ref<Element> root;std::vector<Icon> icons;std::function<void()> refresh;std::function<bool(Element const&,RECT&)> bounds;uint64_t scanned=0;
    winrt::Windows::UI::Xaml::DispatcherTimer timer{nullptr};
    winrt::event_token moved{},left{},tick{},pressed{},released{};bool down=false;uint64_t last=0;
    void discover(){auto r=root.get();if(!r)return;using Tree=winrt::Windows::UI::Xaml::Media::VisualTreeHelper;std::vector<winrt::Windows::UI::Xaml::DependencyObject> nodes{r};unsigned count=0;
        while(!nodes.empty()&&count++<2048){auto node=nodes.back();nodes.pop_back();auto e=node.try_as<Element>();if(e&&winrt::get_class_name(e)==L"Windows.UI.Xaml.Controls.Image"&&e.ActualWidth()>=16&&e.ActualWidth()<=64&&e.ActualHeight()>=16&&e.ActualHeight()<=64){bool found=false;for(auto&i:icons)if(i.image.get()==e)found=true;if(!found){using namespace winrt::Windows::UI::Xaml::Media;Icon i;i.image=winrt::make_weak(e);i.old=e.RenderTransform();i.group=TransformGroup();i.scale=ScaleTransform();i.scale.CenterX(e.ActualWidth()/2);i.scale.CenterY(e.ActualHeight()/2);if(i.old)i.group.Children().Append(i.old);i.group.Children().Append(i.scale);e.RenderTransform(i.group);icons.push_back(std::move(i));}}
            int n=Tree::GetChildrenCount(node);for(int j=0;j<n;++j)nodes.push_back(Tree::GetChild(node,j));}
    }
    void aim(Args const&args,bool outside=false){discover();for(auto&i:icons)if(auto e=i.image.get()){auto p=args.GetCurrentPoint(e).Position();bool over=!outside&&p.X>=-8&&p.X<=e.ActualWidth()+8&&p.Y>=-8&&p.Y<=e.ActualHeight()+8;i.hovered=over;i.target=over?(down?.88f:1.10f):1.f;}last=GetTickCount64();timer.Start();}
    void step(){auto now=GetTickCount64();POINT cursor{};bool pointer=GetCursorPos(&cursor);RECT area{};auto r=root.get();bool inside=pointer&&r&&bounds(r,area)&&PtInRect(&area,cursor);if(inside&&now-scanned>500){discover();scanned=now;}float dt=std::min(.05f,float(now-last)/1000);last=now;down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;bool changed=false;for(auto&i:icons)if(auto e=i.image.get();e&&e.RenderTransform()==i.group){RECT ink{};i.hovered=inside&&bounds(e,ink)&&PtInRect(&ink,cursor);i.target=i.hovered?(down?.88f:1.10f):1.f;float next=i.value+(i.target-i.value)*(1-std::exp(-dt*24));if(std::abs(next-i.target)<.001f)next=i.target;if(next!=i.value){i.value=next;i.scale.ScaleX(next);i.scale.ScaleY(next);changed=true;}}if(changed)refresh();}
public:
    static std::shared_ptr<TaskbarContentMotion> create(Element const&e,std::function<void()> refresh,std::function<bool(Element const&,RECT&)> bounds){BOOL animate=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&animate,0);if(!animate)return {};auto s=std::shared_ptr<TaskbarContentMotion>(new TaskbarContentMotion);s->root=winrt::make_weak(e);s->refresh=std::move(refresh);s->bounds=std::move(bounds);s->timer=winrt::Windows::UI::Xaml::DispatcherTimer();s->timer.Interval(std::chrono::milliseconds(16));auto weak=std::weak_ptr<TaskbarContentMotion>(s);
        s->tick=s->timer.Tick([weak](auto&&,auto&&){if(auto p=weak.lock())try{p->step();}catch(...){p->timer.Stop();}});
        s->moved=e.PointerMoved([weak](auto&&,Args const&a){if(auto p=weak.lock())try{p->aim(a);}catch(...){}});
        s->left=e.PointerExited([weak](auto&&,Args const&a){if(auto p=weak.lock())try{p->down=false;p->aim(a,true);}catch(...){}});
        s->pressed=e.PointerPressed([weak](auto&&,Args const&a){if(auto p=weak.lock())try{p->down=true;p->aim(a);}catch(...){}});
        s->released=e.PointerReleased([weak](auto&&,Args const&a){if(auto p=weak.lock())try{p->down=false;p->aim(a);}catch(...){}});
        s->last=GetTickCount64();s->timer.Start();return s;}
    ~TaskbarContentMotion(){try{if(timer){timer.Stop();timer.Tick(tick);}if(auto r=root.get()){r.PointerMoved(moved);r.PointerExited(left);r.PointerPressed(pressed);r.PointerReleased(released);}for(auto&i:icons)if(auto e=i.image.get();e&&e.RenderTransform()==i.group)e.RenderTransform(i.old);}catch(...){}}
};
}
