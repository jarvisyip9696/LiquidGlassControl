#pragma once
#include <functional>
#include <winrt/Windows.UI.Xaml.Controls.h>
#include <winrt/Windows.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

namespace liquid {
// Only the XAML namespace differs between system XAML and WinUI 3; the watch logic is shared.
template <bool Modern> struct XamlTypes;

template <> struct XamlTypes<true> {
    using Object = winrt::Microsoft::UI::Xaml::DependencyObject;
    using Property = winrt::Microsoft::UI::Xaml::DependencyProperty;
    using Panel = winrt::Microsoft::UI::Xaml::Controls::Panel;
    using Border = winrt::Microsoft::UI::Xaml::Controls::Border;
    using Control = winrt::Microsoft::UI::Xaml::Controls::Control;
    using Shape = winrt::Microsoft::UI::Xaml::Shapes::Shape;
};

template <> struct XamlTypes<false> {
    using Object = winrt::Windows::UI::Xaml::DependencyObject;
    using Property = winrt::Windows::UI::Xaml::DependencyProperty;
    using Panel = winrt::Windows::UI::Xaml::Controls::Panel;
    using Border = winrt::Windows::UI::Xaml::Controls::Border;
    using Control = winrt::Windows::UI::Xaml::Controls::Control;
    using Shape = winrt::Windows::UI::Xaml::Shapes::Shape;
};

template <bool Modern>
std::function<void()>
watchProperty(IInspectable* p, std::wstring const& name, std::function<void()> changed) {
    using T = XamlTypes<Modern>;
    winrt::Windows::Foundation::IInspectable obj{nullptr};
    winrt::copy_from_abi(obj, p);
    auto d = obj.try_as<typename T::Object>();
    if (!d)
        return {};
    typename T::Property dp{nullptr};
    if (name == L"Background") {
        if (obj.try_as<typename T::Panel>())
            dp = T::Panel::BackgroundProperty();
        else if (obj.try_as<typename T::Border>())
            dp = T::Border::BackgroundProperty();
        else if (obj.try_as<typename T::Control>())
            dp = T::Control::BackgroundProperty();
    }
    if (name == L"Fill" && obj.try_as<typename T::Shape>())
        dp = T::Shape::FillProperty();
    if (!dp)
        return {};
    auto token = d.RegisterPropertyChangedCallback(dp, [changed](auto const&, auto const&) {
        changed();
    });
    auto weak = winrt::make_weak(d);
    return [weak, dp, token] {
        try {
            if (auto v = weak.get())
                v.UnregisterPropertyChangedCallback(dp, token);
        } catch (...) {
        }
    };
}

// Keep the override when Windows changes a visual state. Unregister every
// callback on restore, using weak references so hidden surfaces can be freed.
inline std::function<void()>
watch(IInspectable* p, bool modern, std::wstring const& name, std::function<void()> changed) {
    return modern ? watchProperty<true>(p, name, std::move(changed))
                  : watchProperty<false>(p, name, std::move(changed));
}
}
