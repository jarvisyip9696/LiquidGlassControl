#pragma once
// SPDX-License-Identifier: GPL-3.0-only
// Native CommonStates subscription for the user-supplied m417z LiquidGlass rules.
#include <winrt/Windows.UI.Xaml.h>
#include <winrt/Windows.UI.Xaml.Media.h>
#include <functional>
namespace imported_glass {
inline std::function<void()> watchCommonStates(winrt::Windows::UI::Xaml::FrameworkElement element,std::function<void(std::wstring)> changed){
    using namespace winrt::Windows::UI::Xaml;
    for(unsigned depth=0;element&&depth<16;++depth){
        for(auto group:VisualStateManager::GetVisualStateGroups(element))if(group.Name()==L"CommonStates"){
            auto token=group.CurrentStateChanged([changed](auto&&,VisualStateChangedEventArgs const& args){try{changed(args.NewState()?std::wstring(args.NewState().Name()):L"");}catch(...){}});
            try{changed(group.CurrentState()?std::wstring(group.CurrentState().Name()):L"");}catch(...){}
            auto weak=winrt::make_weak(group);return [weak,token]{if(auto g=weak.get())g.CurrentStateChanged(token);};
        }
        element=Media::VisualTreeHelper::GetParent(element).try_as<FrameworkElement>();
    }
    return {};
}
}
