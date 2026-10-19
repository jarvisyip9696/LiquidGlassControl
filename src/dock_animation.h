#pragma once
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
namespace liquid {
inline float dockWeight(float x,float y,float width,float height){if(width<=0||height<=0||y<0||y>height)return 0;float d=std::abs(x-width/2)/std::max(36.f,width*1.35f);return d<1?(1-d*d)*(1-d*d):0;}
inline float dockScale(float requested,float iconWidth,float iconHeight,float buttonWidth,float buttonHeight){if(iconWidth<=0||iconHeight<=0)return 1;return std::min(requested,std::max(1.f,std::min((buttonWidth-4)/iconWidth,(buttonHeight-4)/iconHeight)));}
// C++ mirror of the compositor expression (x pointer, c centre, r radius, on 0..1, m scale cap), without the base scale B.
inline float dockExpr(float x,float c,float r,float on,float m){float d=std::min(std::abs(x-c)/r,1.f);float w=std::max(0.f,1-d*d);return std::min(m,1+.26f*on*w*w);}
inline bool testDockExpr(){const float bw[]={48,60,100};const float iw[]={32,40,48};for(float w:bw)for(float s:iw){float cap=dockScale(1e9f,s,s,w,48);for(float x=-20;x<=w+140;x+=1.5f){float a=dockExpr(x,w/2,std::max(36.f,w*1.35f),1,cap),b=dockScale(1+.26f*dockWeight(x,24,w,48),s,s,w,48);if(std::abs(a-b)>1e-5f)return false;}if(dockExpr(w/2,w/2,36,0,cap)!=1)return false;}return true;}
inline bool testDockWeight(){return dockWeight(24,24,48,48)==1&&dockWeight(24,-1,48,48)==0&&dockWeight(24,49,48,48)==0&&dockWeight(120,24,48,48)==0&&dockWeight(72,24,48,48)>0&&dockWeight(72,24,48,48)<1&&dockWeight(0,0,0,0)==0&&dockScale(1.2f,32,32,48,48)==1.2f&&dockScale(1.2f,40,40,48,48)<=1.1f&&dockScale(1.2f,48,48,48,48)==1&&dockScale(1.2f,0,0,48,48)==1&&testDockExpr();}
// Pointer moves write one scalar (X) into a shared composition property set. Each icon image runs one
// ExpressionAnimation on Scale that reads X, a 0..1 On switch (eased over 150 ms) and its own centre,
// radius and cap from a per-icon property set, so there is no per-move animation, rendering callback,
// global mouse hook, polling thread, or idle timer. If the expression cannot be built, the old
// short keyframe animations per move are used instead.
class DockAnimation {
    using Element=winrt::Windows::UI::Xaml::FrameworkElement;
    using Visual=winrt::Windows::UI::Composition::Visual;
    using Props=winrt::Windows::UI::Composition::CompositionPropertySet;
    using Args=winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs;
    struct Icon {winrt::weak_ref<Element> element,image;winrt::Windows::Foundation::Numerics::float3 scale,center;bool changed=false;float target=1;winrt::event_token entered{},moved{},exited{};Props props{nullptr};bool bound=false;};
    winrt::weak_ref<Element> root;std::vector<Icon> icons;winrt::event_token rootEntered{},moved{},exited{};bool attached=false;
    winrt::Windows::UI::Composition::Compositor comp{nullptr};Props p{nullptr};bool fallback=false,curOn=false;float bandTop=0,bandBottom=-1;std::chrono::steady_clock::time_point lastRefresh{};
    static Visual visual(Element const&e){return winrt::Windows::UI::Xaml::Hosting::ElementCompositionPreview::GetElementVisual(e);}
    static Element iconImage(Element const&button){using Tree=winrt::Windows::UI::Xaml::Media::VisualTreeHelper;Element best{nullptr};double area=0;std::vector<winrt::Windows::UI::Xaml::DependencyObject> pending{button};unsigned visited=0;
        while(!pending.empty()&&visited++<128){auto item=pending.back();pending.pop_back();auto e=item.try_as<Element>();if(e&&e.Visibility()!=winrt::Windows::UI::Xaml::Visibility::Visible)continue;
            if(e&&winrt::get_class_name(item)==L"Windows.UI.Xaml.Controls.Image"&&e.ActualWidth()>=12&&e.ActualHeight()>=12&&e.ActualWidth()<=button.ActualWidth()&&e.ActualHeight()<=button.ActualHeight()){double size=e.ActualWidth()*e.ActualHeight();if(size>area){best=e;area=size;}}
            int count=Tree::GetChildrenCount(item);for(int j=0;j<count;++j)pending.push_back(Tree::GetChild(item,j));}return best;}
    // Fallback path: one keyframe animation per icon per pointer move.
    void animate(Icon&i,Element const&e,float scale){
        // Keep the button, its native highlight, and its hit target at scale 1.
        auto image=i.image.get();if(!image){if(scale<=1&&!i.changed)return;image=iconImage(e);if(!image)return;i.image=winrt::make_weak(image);i.changed=false;i.target=1;}
        scale=dockScale(scale,(float)image.ActualWidth(),(float)image.ActualHeight(),(float)e.ActualWidth(),(float)e.ActualHeight());
        if(std::abs(i.target-scale)<.015f)return;auto v=visual(image);if(!i.changed){i.scale=v.Scale();i.center=v.CenterPoint();i.changed=true;}
        v.CenterPoint({(float)image.ActualWidth()/2,(float)image.ActualHeight(),0});auto a=v.Compositor().CreateVector3KeyFrameAnimation();a.Duration(std::chrono::milliseconds(150));a.InsertKeyFrame(1,{i.scale.x*scale,i.scale.y*scale,i.scale.z});v.StartAnimation(L"Scale",a);i.target=scale;
    }
    void slow(Args const&args){try{
        auto r=root.get();bool horizontal=!r||r.ActualWidth()>=r.ActualHeight();
        for(auto i=icons.begin();i!=icons.end();){auto e=i->element.get();if(!e){i=icons.erase(i);continue;}
            // Local pointer coordinates include the capsule's RenderTransform.
            // Do not compare an animated visual's bounds against the old root.
            float f=0;if(horizontal)try{auto q=args.GetCurrentPoint(e).Position();f=dockWeight(q.X,q.Y,(float)e.ActualWidth(),(float)e.ActualHeight());}catch(...){}
            try{animate(*i,e,1+.26f*f);}catch(...){}++i;}}catch(...){}}
    // Stop the expression and put the image visual back exactly as found.
    void unbind(Icon&i){try{if(i.changed)if(auto image=i.image.get()){auto v=visual(image);v.StopAnimation(L"Scale");v.Scale(i.scale);v.CenterPoint(i.center);}}catch(...){}i.changed=false;i.bound=false;i.props=nullptr;i.target=1;}
    // Find the image of an unbound icon (or one whose image died) and start its single Scale expression.
    void bind(Icon&i,Element const&e){
        if(i.bound&&i.image.get())return;
        if(i.bound){i.bound=false;i.changed=false;i.props=nullptr;}
        auto image=iconImage(e);if(!image)return;
        auto v=visual(image);auto props=comp.CreatePropertySet();props.InsertScalar(L"C",0);props.InsertScalar(L"R",36);props.InsertScalar(L"M",1);
        i.image=winrt::make_weak(image);i.scale=v.Scale();i.center=v.CenterPoint();i.changed=true;
        v.CenterPoint({(float)image.ActualWidth()/2,(float)image.ActualHeight(),0});
        auto ex=comp.CreateExpressionAnimation(L"B * Min(i.M, 1 + 0.26 * p.On * Pow(Max(0, 1 - Pow(Min(Abs(p.X - i.C) / i.R, 1), 2)), 2))");
        ex.SetVector3Parameter(L"B",i.scale);ex.SetReferenceParameter(L"p",p);ex.SetReferenceParameter(L"i",props);
        v.StartAnimation(L"Scale",ex);i.props=props;i.bound=true;
    }
    // Refresh each icon's centre, radius and cap (root coordinates) and the buttons' vertical band; bind new icons.
    void refresh(){lastRefresh=std::chrono::steady_clock::now();if(fallback||!p)return;try{auto r=root.get();if(!r)return;float top=1e9f,bottom=-1e9f;
        for(auto&i:icons){auto e=i.element.get();if(!e)continue;
            try{float bw=(float)e.ActualWidth(),bh=(float)e.ActualHeight();if(bw<=0||bh<=0)continue;
                auto t=e.TransformToVisual(r);auto c=t.TransformPoint({bw/2,0.f});float y0=t.TransformPoint({0.f,0.f}).Y,y1=t.TransformPoint({0.f,bh}).Y;
                bind(i,e);auto image=i.image.get();
                if(i.bound&&image){i.props.InsertScalar(L"C",c.X);i.props.InsertScalar(L"R",std::max(36.f,bw*1.35f));i.props.InsertScalar(L"M",dockScale(1e9f,(float)image.ActualWidth(),(float)image.ActualHeight(),bw,bh));}
                top=std::min(top,std::min(y0,y1));bottom=std::max(bottom,std::max(y0,y1));
            }catch(...){fallback=true;break;}}
        if(fallback){for(auto&i:icons)unbind(i);p=nullptr;return;}
        bandTop=top;bandBottom=bottom;}catch(...){}}
    // (Re)build the compositor property set and leave the keyframe fallback; called on attach and on every root PointerEntered after a failure.
    void retry(){if(!fallback&&p)return;try{auto r=root.get();if(!r)return;comp=visual(r).Compositor();p=comp.CreatePropertySet();p.InsertScalar(L"X",0);p.InsertScalar(L"On",0);fallback=false;curOn=false;}catch(...){fallback=true;p=nullptr;comp=nullptr;}}
    void setOn(bool on){if(on==curOn)return;auto a=comp.CreateScalarKeyFrameAnimation();a.Duration(std::chrono::milliseconds(150));a.InsertKeyFrame(1,on?1.f:0.f);p.StartAnimation(L"On",a);curOn=on;}
    // leaving: PointerExited can report a point on the edge, so require a margin before keeping On.
    void point(Args const&args,bool leaving=false){if(fallback){slow(args);return;}try{
        auto r=root.get();if(!r||!p)return;
        // Icons can shift, vanish or ride the capsule transform while the pointer is inside: re-measure at most every 100 ms; between refreshes a move is one InsertScalar.
        if(!leaving&&std::chrono::steady_clock::now()-lastRefresh>=std::chrono::milliseconds(100)){refresh();if(fallback){slow(args);return;}}
        auto q=args.GetCurrentPoint(r).Position();p.InsertScalar(L"X",q.X);
        float w=(float)r.ActualWidth(),h=(float)r.ActualHeight(),m=leaving?2.f:0.f;
        setOn(w>=h&&q.X>=m&&q.X<=w-m&&q.Y>=bandTop+m&&q.Y<=bandBottom-m);}catch(...){}}
public:
    ~DockAnimation(){stop();}
    bool ready()const{return attached&&!icons.empty();}
    bool attach(IInspectable*p_){try{auto e=winrt::Windows::Foundation::IInspectable{nullptr};winrt::copy_from_abi(e,p_);auto r=e.try_as<Element>();if(!r)return false;if(attached&&root.get()==r)return true;
        // One taskbar thread hosts one root. A second live root is reported rather than silently taking over the first root's icons.
        if(attached&&root.get())return false;
        if(attached)stop();root=r;fallback=true;p=nullptr;curOn=false;bandTop=0;bandBottom=-1;retry();
        rootEntered=r.PointerEntered([this](auto const&,auto const&args){retry();refresh();point(args);});
        moved=r.PointerMoved([this](auto const&,auto const&args){point(args);});
        exited=r.PointerExited([this](auto const&,auto const&args){point(args,true);});attached=true;return true;
    }catch(...){return false;}}
    void add(IInspectable*p_){try{winrt::Windows::Foundation::IInspectable obj{nullptr};winrt::copy_from_abi(obj,p_);auto e=obj.try_as<Element>();if(!e)return;for(auto&i:icons)if(i.element.get()==e)return;if(icons.size()>=256)return;
        icons.push_back({winrt::make_weak(e),{},{1,1,1},{0,0,0},false,1});auto&i=icons.back();
        // Buttons can handle pointer movement before it reaches the taskbar root.
        try{i.entered=e.PointerEntered([this](auto const&,auto const&args){point(args);});i.moved=e.PointerMoved([this](auto const&,auto const&args){point(args);});i.exited=e.PointerExited([this](auto const&,auto const&args){point(args,true);});}
        catch(...){try{e.PointerEntered(i.entered);}catch(...){}try{e.PointerMoved(i.moved);}catch(...){}try{e.PointerExited(i.exited);}catch(...){}icons.pop_back();throw;}
        refresh();
    }catch(...){}}
    // A button left the taskbar: unhook it, put its image back and re-measure the rest.
    void remove(IInspectable*p_){try{winrt::Windows::Foundation::IInspectable obj{nullptr};winrt::copy_from_abi(obj,p_);auto e=obj.try_as<Element>();if(!e)return;
        for(auto it=icons.begin();it!=icons.end();++it)if(it->element.get()==e){
            try{e.PointerEntered(it->entered);}catch(...){}try{e.PointerMoved(it->moved);}catch(...){}try{e.PointerExited(it->exited);}catch(...){}
            unbind(*it);icons.erase(it);break;}
        refresh();
    }catch(...){}}
    void clear(){for(auto&i:icons){if(auto e=i.element.get()){try{e.PointerEntered(i.entered);}catch(...){}try{e.PointerMoved(i.moved);}catch(...){}try{e.PointerExited(i.exited);}catch(...){}}unbind(i);}icons.clear();bandTop=0;bandBottom=-1;}
    void stop(){try{if(auto r=root.get()){if(attached){r.PointerEntered(rootEntered);r.PointerMoved(moved);r.PointerExited(exited);}}}catch(...){}attached=false;for(auto&i:icons)if(auto e=i.element.get()){
        try{e.PointerEntered(i.entered);}catch(...){}try{e.PointerMoved(i.moved);}catch(...){}try{e.PointerExited(i.exited);}catch(...){}
        if(i.changed)try{if(auto image=i.image.get()){auto v=visual(image);v.StopAnimation(L"Scale");v.Scale(i.scale);v.CenterPoint(i.center);}}catch(...){}
    }
        icons.clear();root=nullptr;p=nullptr;comp=nullptr;curOn=false;}
};
}
