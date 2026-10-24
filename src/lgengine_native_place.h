#pragma once
#include "lgengine_native_content.h"
#include <cmath>
namespace lghost {
// Match only an unambiguous native host on the source XAML thread. No guessed
// desktop coordinates or global foreground-window fallback is allowed.
inline HWND nativeIslandWindow(winrt::Windows::UI::Xaml::FrameworkElement const&e){
    auto root=e.XamlRoot();if(!root)return nullptr;
    auto content=root.Content().try_as<winrt::Windows::UI::Xaml::FrameworkElement>();if(!content)return nullptr;
    struct Search {int width,height;unsigned visited=0;HWND candidate{};POINT origin{};bool ambiguous=false;};
    Search search{int(std::lround(content.ActualWidth()*root.RasterizationScale())),int(std::lround(content.ActualHeight()*root.RasterizationScale()))};
    if(search.width<=0||search.height<=0)return nullptr;
    auto inspect=[](HWND h,LPARAM value)->BOOL{auto&s=*reinterpret_cast<Search*>(value);if(++s.visited>2048)return FALSE;
        if(!IsWindowVisible(h))return TRUE;RECT r{};if(!GetClientRect(h,&r)||std::abs(r.right-s.width)>1||std::abs(r.bottom-s.height)>1)return TRUE;
        POINT point{};if(!ClientToScreen(h,&point))return TRUE;
        if(s.candidate&&(point.x!=s.origin.x||point.y!=s.origin.y))s.ambiguous=true;
        s.candidate=h;s.origin=point;return TRUE;
    };
    struct Context {Search* search;WNDENUMPROC inspect;};Context context{&search,inspect};
    EnumThreadWindows(GetCurrentThreadId(),[](HWND h,LPARAM value)->BOOL{auto&c=*reinterpret_cast<Context*>(value);c.inspect(h,reinterpret_cast<LPARAM>(c.search));EnumChildWindows(h,c.inspect,reinterpret_cast<LPARAM>(c.search));return c.search->visited<=2048;},reinterpret_cast<LPARAM>(&context));
    return search.ambiguous||search.visited>2048?nullptr:search.candidate;
}
inline bool nativePlace(winrt::Windows::UI::Xaml::FrameworkElement const&e,HWND island,Spec&spec){
    if(!island||!IsWindow(island)||!IsWindowVisible(island)||!e.IsLoaded())return false;
    DWORD pid{};if(GetWindowThreadProcessId(island,&pid)!=GetCurrentThreadId()||pid!=GetCurrentProcessId())return false;
    // Shell-owned XamlRoot visibility can disagree with its live HWND. The exact
    // same-thread island and ancestor visibility checks are authoritative here.
    auto root=e.XamlRoot();if(!root)return false;
    using namespace winrt::Windows::UI::Xaml;
    DependencyObject parent=e;for(unsigned n=0;parent&&n<128;++n){if(auto ui=parent.try_as<UIElement>();ui&&ui.Visibility()!=Visibility::Visible)return false;parent=Media::VisualTreeHelper::GetParent(parent);}
    auto transform=e.TransformToVisual(root.Content());auto top=transform.TransformPoint({0,0});
    auto x=transform.TransformPoint({float(e.ActualWidth()),0}),y=transform.TransformPoint({0,float(e.ActualHeight())});
    if(std::abs(x.Y-top.Y)>.1f||std::abs(y.X-top.X)>.1f||x.X<=top.X||y.Y<=top.Y)return false;
    double scale=root.RasterizationScale();POINT at{};if(!ClientToScreen(island,&at))return false;
    int width=int(std::lround((x.X-top.X)*scale)),height=int(std::lround((y.Y-top.Y)*scale));
    RECT client{};if(!GetClientRect(island,&client))return false;
    int left=int(std::lround(top.X*scale)),upper=int(std::lround(top.Y*scale));
    if(left<0||upper<0||left+width>client.right+1||upper+height>client.bottom+1)return false;
    spec.x=at.x+left;spec.y=at.y+upper;spec.width=width;spec.height=height;spec.scale=float(scale);return wire::valid(spec);
}
}
