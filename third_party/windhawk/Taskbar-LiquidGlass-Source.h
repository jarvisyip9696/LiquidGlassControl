// SPDX-License-Identifier: GPL-3.0-only
// Exact g_themeLiquidGlass from the user supplied m417z Taskbar Styler 1.10.
inline const Theme TaskbarSource = {{
    ThemeTargetStyles{L"Taskbar.TaskbarFrame > Grid#RootGrid", {
        L"BorderThickness=$BorderThickness",
        L"BorderBrush:=$BorderBrush",
        L"Background:=$Background"}},
    ThemeTargetStyles{L"Rectangle#BackgroundFill", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"Rectangle#BackgroundStroke", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"Taskbar.AugmentedEntryPointButton#AugmentedEntryPointButton > Taskbar.TaskListButtonPanel#ExperienceToggleButtonRootPanel", {
        L"Margin=-3,0"}},
    ThemeTargetStyles{L"StackPanel#SystemTrayFrameGrid, Grid#SystemTrayFrameGrid", {
        L"Background:=$ElementBackground",
        L"BorderBrush:=$ElementBorderBrush",
        L"BorderThickness=$ElementBorderThickness",
        L"CornerRadius=$ElementCornerRadius",
        L"Margin=6"}},
    ThemeTargetStyles{L"SystemTray.ChevronIconView", {
        L"CornerRadius=$ElementCornerRadius"}},
    ThemeTargetStyles{L"SystemTray.NotifyIconView#NotifyItemIcon", {
        L"CornerRadius=$ElementCornerRadius"}},
    ThemeTargetStyles{L"SystemTray.OmniButton", {
        L"CornerRadius=$ElementCornerRadius"}},
    ThemeTargetStyles{L"SystemTray.IconView#SystemTrayIcon > Grid#ContainerGrid > ContentPresenter#ContentPresenter > Grid#ContentGrid > SystemTray.TextIconContent > Grid#ContainerGrid", {
        L"CornerRadius=$ElementCornerRadius"}},
    ThemeTargetStyles{L"Taskbar.Gripper#GripperControl", {
        L"Width=Auto",
        L"MinWidth=24",
        L"HorizontalAlignment=Left"}},
    ThemeTargetStyles{L"TextBlock#TimeInnerTextBlock", {
        L"FontSize=13",
        L"Margin=0",
        L"Padding=0",
        L"RenderTransform:=<TranslateTransform X=\"0\" Y=\"0\" />"}},
    ThemeTargetStyles{L"TextBlock#DateInnerTextBlock", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"TextBlock#InnerTextBlock[Text=\uE971]", {
        L"Text=\uED14"}},
    ThemeTargetStyles{L"TextBlock#SearchBoxTextBlock", {
        L"Text=Search This PC",
        L"FontSize=10"}},
    ThemeTargetStyles{L"SystemTray.OmniButton#NotificationCenterButton > Grid > ContentPresenter > ItemsPresenter > StackPanel > ContentPresenter > SystemTray.IconView#SystemTrayIcon > Grid > Grid > SystemTray.TextIconContent", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"Border#OverflowFlyoutBackgroundBorder", {
        L"BorderThickness=$BorderThickness",
        L"BorderBrush:=$BorderBrush",
        L"Background:=$Background",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab > Windows.UI.Xaml.Controls.Grid#ModalRootGrid > Windows.UI.Xaml.Controls.Border#BackgroundElement", {
        L"BorderThickness=0",
        L"Background=Transparent",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"WindowsInternal.ComposableShell.Experiences.Switcher.AltTab > Windows.UI.Xaml.Controls.Grid#ModalRootGrid > Windows.UI.Xaml.Controls.Border#BackgroundElement > WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemList", {
        L"Background:=$Background",
        L"BorderThickness=$BorderThickness",
        L"BorderBrush:=$BorderBrush",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"WindowsInternal.ComposableShell.Experiences.Switcher.VirtualDesktopBarElement#VirtualDesktopBar", {
        L"CornerRadius=$CornerRadius",
        L"Background:=$Background"}},
    ThemeTargetStyles{L"Border#BackgroundDimmingLayer", {
        L"Background:=$Background",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"Taskbar.TaskListButtonPanel#ExperienceToggleButtonRootPanel > Grid > Border#BackgroundElement, Taskbar.TaskListButtonPanel#ExperienceToggleButtonRootPanel > Border#BackgroundElement", {
        L"CornerRadius=$CornerRadius",
        L"BorderThickness=$ElementBorderThickness"}},
    ThemeTargetStyles{L"Taskbar.TaskListButton#TaskListButton", {
        L"CornerRadius=$ElementCornerRadius",
        L"BorderThickness=$ElementBorderThickness"}},
    ThemeTargetStyles{L"Border#SnapBarBorder", {
        L"Background:=$Background",
        L"BorderBrush:=$BorderBrush",
        L"CornerRadius=$CornerRadius",
        L"BorderThickness=$BorderThickness",
        L"Margin=2"}},
    ThemeTargetStyles{L"Grid#IconPanel@CommonStates > Border#BackgroundElement, Taskbar.TaskListLabeledButtonPanel@CommonStates > Border#BackgroundElement", {
        L"CornerRadius=$ElementCornerRadius",
        L"BorderThickness=$ElementBorderThickness",
        L"Background@ActiveNormal:=$ElementBackground",
        L"Background@ActivePointerOver:=$AccentBackground",
        L"Background@ActivePressed:=$ElementBackground2",
        L"Background@InactivePointerOver:=$AccentBackground",
        L"Background@InactivePressed:=$ElementBackground2",
        L"BorderBrush@ActiveNormal:=$ElementBorderBrush",
        L"BorderBrush@ActivePointerOver:=$ElementBorderBrush",
        L"BorderBrush@ActivePressed:=$ElementBorderBrush",
        L"BorderBrush@InactivePointerOver:=$ElementBorderBrush",
        L"BorderBrush@InactivePressed:=$ElementBorderBrush",
        L"Margin=1"}},
    ThemeTargetStyles{L"ContentPresenter#ContentPresenter@CommonStates", {
        L"CornerRadius=$ElementCornerRadius",
        L"BorderThickness=$ElementBorderThickness",
        L"Background@ActiveNormal:=$ElementBackground",
        L"Background@ActivePointerOver:=$AccentBackground",
        L"Background@ActivePressed:=$ElementBackground2",
        L"Background@InactivePointerOver:=$AccentBackground",
        L"Background@InactivePressed:=$ElementBackground2",
        L"BorderBrush@ActiveNormal:=$ElementBorderBrush",
        L"BorderBrush@ActivePointerOver:=$ElementBorderBrush",
        L"BorderBrush@ActivePressed:=$ElementBorderBrush",
        L"BorderBrush@InactivePointerOver:=$ElementBorderBrush",
        L"BorderBrush@InactivePressed:=$ElementBorderBrush",
        L"Margin=1"}},
    ThemeTargetStyles{L"Border#SnapPickerBorder", {
        L"Background:=$Background",
        L"BorderBrush:=$BorderBrush",
        L"CornerRadius=$CornerRadius",
        L"BorderThickness=$BorderThickness",
        L"Margin=2"}},
    ThemeTargetStyles{L"Taskbar.TaskListButtonPanel#ExperienceToggleButtonRootPanel", {
        L"Background:=Transparent"}},
    ThemeTargetStyles{L"ToolTip > ContentPresenter#LayoutRoot", {
        L"Background:=$Background",
        L"BorderBrush:=$BorderBrush",
        L"BorderThickness:=$BorderThickness",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"WindowsInternal.ComposableShell.Experiences.Switcher.VirtualDesktopBarElement > Grid#GridElement > Border#VirtualDesktopSwitcherBackground", {
        L"BorderBrush:=$BorderBrush",
        L"BorderThickness=$BorderThickness",
        L"CornerRadius=$CornerRadius",
        L"Background:=$Background"}},
    ThemeTargetStyles{L"WindowsInternal.ComposableShell.Experiences.Switcher.SwitchItemListViewItem > Grid > Border", {
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"Border#VirtualDesktopBarBackground", {
        L"Background:=$Background",
        L"BorderBrush:=$BorderBrush",
        L"BorderThickness=$BorderThickness",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"Rectangle#RunningIndicator", {
        L"Fill:=$AccentBackground",
        L"Width=14"}},
    ThemeTargetStyles{L"Rectangle#ShowDesktopPipe", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"Rectangle#RightOverflowButtonDivider", {
        L"Visibility=1"}},
    ThemeTargetStyles{L"SearchUx.SearchUI.SearchIconButton > SearchUx.SearchUI.SearchButtonRootGrid@CommonStates > Grid > Border#BackgroundElement, SearchUx.SearchUI.SearchIconButton > SearchUx.SearchUI.SearchButtonRootGrid@CommonStates > Border#BackgroundElement", {
        L"Background:=Transparent",
        L"BorderBrush:=Transparent"}},
    ThemeTargetStyles{L"SearchUx.SearchUI.SearchButtonRootGrid", {
        L"Background:=Transparent",
        L"BorderBrush:=Transparent"}},
    ThemeTargetStyles{L"Border#SearchPillBackgroundElement", {
        L"BorderBrush:=$ElementBorderBrush",
        L"BorderThickness=$ElementBorderThickness",
        L"CornerRadius=$ElementCornerRadius",
        L"Margin=0,1"}},
    ThemeTargetStyles{L"SearchUx.SearchUI.SearchBoxButton > SearchUx.SearchUI.SearchButtonRootGrid@CommonStates > Grid > Border#BackgroundElement, SearchUx.SearchUI.SearchBoxButton > SearchUx.SearchUI.SearchButtonRootGrid@CommonStates > Border#BackgroundElement", {
        L"CornerRadius=$ElementCornerRadius",
        L"BorderThickness=$ElementBorderThickness",
        L"BorderBrush:=$ElementBorderBrush",
        L"Background:=$ElementBackground",
        L"Margin=0,-4"}},
    ThemeTargetStyles{L"Canvas#HoverFlyoutCanvas > Grid#HoverFlyoutGrid > Border#HoverFlyoutBackground", {
        L"Shadow:=",
        L"Background:=$Background",
        L"BorderBrush:=$BorderBrush",
        L"BorderThickness=$BorderThickness",
        L"CornerRadius=$CornerRadius"}},
    ThemeTargetStyles{L"SystemTray.SystemTrayFrame", {
        L"HorizontalAlignment=Right"}},
    ThemeTargetStyles{L"Grid#AugmentedEntryPointContentGrid", {
        L"HorizontalAlignment=Left"}},
}, {
    L"Background=<WindhawkBlur BlurAmount=\"15\" TintColor=\"{ThemeResource SystemAltLowColor}\" TintOpacity=\"0.2\" />",
    L"ElementBackground=<WindhawkBlur BlurAmount=\"20\" TintColor=\"{ThemeResource SystemAltLowColor}\" TintOpacity=\"0.4\" />",
    L"ElementBackground2=<WindhawkBlur BlurAmount=\"20\" TintColor=\"{ThemeResource SystemAltLowColor}\" TintOpacity=\"0.2\" />",
    L"AccentBackground=<WindhawkBlur BlurAmount=\"15\" TintColor=\"{ThemeResource SystemAccentColorLight1}\" TintOpacity=\"0.2\" />",
    L"BorderBrush=<LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"0,1\"><GradientStop Color=\"#50808080\" Offset=\"0.0\" /><GradientStop Color=\"#50404040\" Offset=\"0.25\" /><GradientStop Color=\"#50808080\" Offset=\"1\" /></LinearGradientBrush>",
    L"ElementBorderBrush=<LinearGradientBrush StartPoint=\"0,0\" EndPoint=\"0,1\"><GradientStop Color=\"#50808080\" Offset=\"1\" /><GradientStop Color=\"#50606060\" Offset=\"0.15\" /></LinearGradientBrush>",
    L"BorderThickness=0.3,1,0.3,0.3",
    L"ElementBorderThickness=0.3,0.3,0.3,1",
    L"CornerRadius=12",
    L"ElementCornerRadius=8",
}};
