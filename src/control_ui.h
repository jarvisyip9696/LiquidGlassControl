void fitTabItems(HWND tab){
    RECT client{};GetClientRect(tab,&client);int count=TabCtrl_GetItemCount(tab);if(!count)return;
    int width=std::max(1,(int(client.right)-MulDiv(24,dpi,96))/count-MulDiv(8,dpi,96));
    // Button tabs add their own spacing. Measure the final item in device pixels.
    for(int attempt=0;attempt<16;++attempt){
        TabCtrl_SetItemSize(tab,width,MulDiv(34,dpi,96));RECT last{};TabCtrl_GetItemRect(tab,count-1,&last);
        int overflow=last.right-(client.right-MulDiv(4,dpi,96));if(overflow<=0)break;
        width=std::max(1,width-(overflow+count-1)/count-1);
    }
    InvalidateRect(tab,nullptr,TRUE);
}
void labelSectionTabs(){
    const wchar_t*systemNames[5][2]={{L"Effects",L"Material"},{L"效果开关",L"材质调节"},{L"效果開關",L"材質調整"},{L"Efekty",L"Materiał"},{L"Hiệu ứng",L"Chất liệu"}};
    const wchar_t*settingsNames[]={tr(L"常规",L"General"),tr(L"关于",L"About")};
    for(int i=0;i<2;++i){TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(systemNames[int(l10n::current)][i]);TabCtrl_SetItem(systemTabs,i,&item);item.pszText=const_cast<wchar_t*>(settingsNames[i]);TabCtrl_SetItem(settingsTabs,i,&item);}
}
void localize(){
    SetWindowTextW(mainWindow,L"Liquid Glass · v" LG_VERSION_W);
    struct Text{int id;const wchar_t*zh;const wchar_t*en;};
    const Text texts[]={
      {Header,L"Liquid Glass",L"Liquid Glass"},{Refresh,L"刷新窗口",L"Refresh windows"},{Apply,L"应用",L"Apply"},{Restore,L"还原",L"Restore"},{Pause,L"隐藏至托盘",L"Hide to tray"},
      {Note,L"选择应用后点击「应用」，点击「还原」撤销效果。\r\n系统玻璃独立运行；部分应用的自绘内容区仍可能不透明。",L"Choose an app and Apply; Restore reverts the effect.\r\nSystem glass runs independently; custom-drawn content can remain opaque."},
      {VolumeHeading,L"音量提示",L"Volume indicator"},{BrightnessHeading,L"亮度提示",L"Brightness indicator"},
      {VolumeOn,L"音量玻璃",L"Volume glass"},{BrightnessOn,L"亮度玻璃",L"Brightness glass"},
      {TaskbarToggle,L"任务栏玻璃",L"Taskbar glass"},{StartToggle,L"开始菜单玻璃",L"Start menu glass"},{NotificationToggle,L"通知中心玻璃",L"Notification center glass"},
      {TaskbarHeading,L"任务栏",L"Taskbar"},{StartHeading,L"开始菜单",L"Start menu"},{NotificationHeading,L"通知中心与日历",L"Notification center and calendar"},
      {ExplorerToggle,L"资源管理器玻璃",L"Explorer glass"},{ExplorerHeading,L"资源管理器",L"File Explorer"},
      {OpacityLabel,L"着色强度（越低越透明）",L"Tint strength (lower is clearer)"},{RadiusLabel,L"面板圆角",L"Panel corner radius"},{TintLabel,L"玻璃色调",L"Glass tint"},
      {BatteryFriendly,L"使用电池或节电模式时降低材质开销",L"Reduce material cost on battery or in battery saver"},
      {RestoreOriginal,L"恢复默认参数",L"Reset material sliders"},
      {KeyboardToggle,L"键盘布局切换",L"Keyboard layout switcher"},
      {OpticalPreview,L"LGEngine 光学预览",L"LGEngine optical preview"},
      {EffectNote,L"更改后自动保存并应用；只调整材质，不改变任务栏尺寸。\r\n独立模糊与着色滑块；Dock 悬停动画单独开启。",L"Changes are saved and applied; taskbar geometry is preserved.\r\nIndependent blur and tint controls; Dock hover has a separate switch."},
      {LanguageLabel,L"界面语言",L"Interface language"},{ColorThemeLabel,L"界面配色",L"Interface color"},{StartupAnimation,L"程序启动时淡入并轻轻上移",L"Fade and gently slide the app in at startup"},
      {SettingsNote,L"语言、配色和启动动画偏好自动保存。\r\n关闭设置窗口会继续在托盘运行。",L"Language, color and startup animation preferences are saved automatically.\r\nClosing settings keeps the app running in the tray."},
      {AboutText,L"Liquid Glass " LG_VERSION_W L"\r\n独立 Windows 材质引擎，单 EXE 分发。\r\n\r\n事件触发，无后台定时扫描。闲置时不持续重绘。\r\n图标缓存随窗口列表清理；XAML 节点使用弱引用。\r\n电池模式可使用低成本回退材质。\r\n不包含完整光学折射。",L"Liquid Glass " LG_VERSION_W L"\r\nStandalone Windows materials, distributed as one EXE.\r\n\r\nEvent driven, with no periodic background scans or idle repaints.\r\nIcon cache is pruned; XAML nodes use weak references.\r\nOptional low-cost material fallback on battery.\r\nFull optical refraction is not implemented."},
      {DockHeading,L"Dock 悬停动画",L"Dock hover animation"},{DockToggle,L"Dock 悬停动画",L"Dock hover animation"},

      {ExitApp,L"还原并退出",L"Restore and exit"}
    };
    for(auto&t:texts)SetWindowTextW(GetDlgItem(mainWindow,t.id),tr(t.zh,t.en));
    {const wchar_t*fade[]={L"Smooth fade at startup",L"启动时平滑淡入",L"啟動時平順淡入",L"Płynne pojawianie przy starcie",L"Hiện dần mượt khi khởi động"};SetWindowTextW(GetDlgItem(mainWindow,StartupAnimation),fade[int(l10n::current)]);}
    const wchar_t*names[]={tr(L"应用玻璃",L"App glass"),tr(L"系统玻璃",L"System glass"),tr(L"设置",L"Settings")};
    for(int i=0;i<3;++i){TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(names[i]);TabCtrl_SetItem(tabs,i,&item);}
    labelSectionTabs();
    int s=selection(scope),m=selection(material),c=selection(corners);
    combo(scope,{tr(L"所选应用（按进程名）",L"Selected app (by executable name)"),tr(L"资源管理器窗口",L"File Explorer windows"),tr(L"全部应用（内容区不保证）",L"All apps (content not guaranteed)")},s);
    combo(material,{L"Acrylic",L"Mica",L"Mica Alt",tr(L"关闭背景材质",L"No backdrop")},m);combo(corners,{tr(L"圆角",L"Rounded"),tr(L"小圆角",L"Small rounded"),tr(L"直角",L"Square")},c);
    combo(GetDlgItem(mainWindow,ColorTheme),{tr(L"经典蓝",L"Classic blue"),tr(L"流光紫",L"Violet"),tr(L"青绿",L"Jade"),tr(L"暖橙",L"Warm orange"),tr(L"玫瑰",L"Rose"),tr(L"湖蓝",L"Lake blue"),tr(L"石墨灰",L"Graphite")},colorTheme);
    {const wchar_t*background[]={L"Adapt to background",L"根据背景自适应",L"依背景自動調整",L"Dopasuj do tła",L"Tự điều chỉnh theo nền"};combo(GetDlgItem(mainWindow,TintCombo),{tr(L"跟随系统",L"Follow system"),tr(L"浅色",L"Light"),tr(L"深色",L"Dark"),background[int(l10n::current)]},(int)glassConfig.tint);}updateEffectLabels();
    {const wchar_t*note[]={L"Applies to enabled system glass surfaces. Background mode adjusts tint from the pixels behind the glass. Battery saving may use a solid fallback.",L"作用于已开启的系统玻璃。背景模式根据玻璃背后的画面调整色调；省电时可能使用纯色回退。",L"套用至已啟用的系統玻璃。背景模式依玻璃背後的畫面調整色調；省電時可能使用純色替代。",L"Dotyczy włączonego szkła systemowego. Tryb tła dopasowuje odcień do obrazu za szkłem. Oszczędzanie energii może użyć jednolitego koloru.",L"Áp dụng cho kính hệ thống đã bật. Chế độ nền điều chỉnh màu theo hình ảnh phía sau. Tiết kiệm pin có thể dùng màu đặc."};SetWindowTextW(GetDlgItem(mainWindow,EffectNote),note[int(l10n::current)]);}
    lstrcpynW(tray.szTip,tr(L"Liquid Glass · 原生提示条",L"Liquid Glass · Native indicators"),128);if(tray.hWnd)Shell_NotifyIconW(NIM_MODIFY,&tray);
    combo(language,{l10n::names[0],l10n::names[1],l10n::names[2],l10n::names[3],l10n::names[4]},int(l10n::current));createFonts();fitTabItems(tabs);fitTabItems(settingsTabs);fitTabItems(systemTabs);refresh(entries.empty());updateNativeInfo();
    message(migrationJob?tr(L"正在恢复旧主题……",L"Restoring the old theme..."):tr(L"选择应用或切换分页。语言在设置 → 常规；材质在系统玻璃 → 材质调节。",L"Choose an app or a page. Language: Settings > General. Materials: System glass > Material."));
}
HWND control(const wchar_t*cls,const wchar_t*text,DWORD style,int id,int x,int y,int w,int h){
    bool isSwitch=_wcsicmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_AUTOCHECKBOX;if(isSwitch)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
    if(_wcsicmp(cls,L"BUTTON")==0&&((style&BS_TYPEMASK)==BS_PUSHBUTTON||(style&BS_TYPEMASK)==BS_DEFPUSHBUTTON))style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
    if(_wcsicmp(cls,L"COMBOBOX")==0)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
    auto px=[](int a){return MulDiv(a,dpi,96);};HWND result=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),mainWindow,reinterpret_cast<HMENU>((INT_PTR)id),GetModuleHandleW(nullptr),nullptr);
    if(result)controlLayout[result]={x,y,x+w,y+h};if(_wcsicmp(cls,L"LISTBOX")==0)SetWindowSubclass(result,listHover,5,0);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(id==Header?headingFont:font),TRUE);SetWindowTheme(result,L"Explorer",nullptr);if(isSwitch){SetPropW(result,L"LG.IsSwitch",(HANDLE)1);SetWindowSubclass(result,compactSwitch,3,0);}if(_wcsicmp(cls,TRACKBAR_CLASSW)==0)SetWindowSubclass(result,modernSlider,4,0);if(_wcsicmp(cls,L"COMBOBOX")==0){SetWindowSubclass(result,themedControl,2,1);SendMessageW(result,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(23));SendMessageW(result,CB_SETITEMHEIGHT,0,px(26));}if(_wcsicmp(cls,WC_TABCONTROLW)==0)SetWindowSubclass(result,themedControl,2,0);if(!isSwitch&&_wcsicmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_OWNERDRAW)SetWindowSubclass(result,buttonHover,1,0);if(buildingPage>=0)pageControls[buildingPage].push_back(result);if(buildingSettings>=0)settingsControls[buildingSettings].push_back(result);return result;
}
void tabItem(HWND tab,int count,int){for(int i=0;i<count;++i){TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(L" ");TabCtrl_InsertItem(tab,i,&item);}fitTabItems(tab);}
void resizeControls(){auto px=[](int n){return MulDiv(n,dpi,96);};for(auto&entry:controlLayout){auto h=entry.first;auto&r=entry.second;SetWindowPos(h,nullptr,px(r.left),px(r.top),px(r.right-r.left),px(r.bottom-r.top),SWP_NOZORDER|SWP_NOACTIVATE);wchar_t cls[64]{};GetClassNameW(h,cls,64);if(!_wcsicmp(cls,L"COMBOBOX")){SendMessageW(h,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(23));SendMessageW(h,CB_SETITEMHEIGHT,0,px(26));}if(h==list)SendMessageW(h,LB_SETITEMHEIGHT,0,px(48));}fontLanguage=-1;createFonts();RemovePropW(tabs,L"LG.Sel");RemovePropW(settingsTabs,L"LG.Sel");fitTabItems(tabs);fitTabItems(settingsTabs);fitTabItems(systemTabs);RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void buildUI(){
    createFonts();control(L"STATIC",L"",0,Header,68,14,450,38);
    DWORD tabStyle=WS_TABSTOP|TCS_OWNERDRAWFIXED|TCS_FIXEDWIDTH|TCS_BUTTONS|TCS_FLATBUTTONS;
    tabs=control(WC_TABCONTROLW,L"",tabStyle,Pages,24,54,742,38);tabItem(tabs,3,240);
    buildingPage=0;
    scope=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,Scope,24,104,332,180);material=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Material,368,104,180,180);corners=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Corners,560,104,206,150);
    list=control(L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_BORDER|WS_VSCROLL|WS_TABSTOP,List,24,148,742,190);SendMessageW(list,LB_SETITEMHEIGHT,0,MulDiv(48,dpi,96));
    compatibility=control(L"STATIC",L"",0,Compatibility,24,346,742,42);
    control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Refresh,24,398,212,36);control(L"BUTTON",L"",BS_DEFPUSHBUTTON|WS_TABSTOP,Apply,250,398,250,36);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Restore,514,398,252,36);note=control(L"STATIC",L"",0,Note,24,454,742,82);
    buildingPage=1;
    systemTabs=control(WC_TABCONTROLW,L"",tabStyle,SettingsPages+1000,32,112,720,38);tabItem(systemTabs,2,340);
    int headings[]={VolumeHeading,BrightnessHeading,TaskbarHeading,StartHeading,NotificationHeading,ExplorerHeading,DockHeading,KeyboardHeading};int switches[]={VolumeOn,BrightnessOn,TaskbarToggle,StartToggle,NotificationToggle,ExplorerToggle,DockToggle,KeyboardToggle};
    // Seven system-effect rows; taskbar includes the capsule appearance.
    for(int i=0;i<8;++i){int y=164+i*36;control(L"STATIC",L"",0,headings[i],40,y,590,28);HWND toggle=control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,switches[i],650,y,100,32);RemoveWindowSubclass(toggle,buttonHover,1);SetPropW(toggle,L"LG.IsSwitch",(HANDLE)1);SetWindowSubclass(toggle,compactSwitch,3,0);}
    nativeInfo=control(L"STATIC",L"",0,NativeInfo,40,458,710,72);
    buildingPage=2;
    settingsTabs=control(WC_TABCONTROLW,L"",tabStyle,SettingsPages,32,112,720,38);tabItem(settingsTabs,2,340);
    buildingSettings=0;control(L"STATIC",L"",0,LanguageLabel,40,184,210,28);language=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Language,272,178,440,220);combo(language,{l10n::names[0],l10n::names[1],l10n::names[2],l10n::names[3],l10n::names[4]},int(l10n::current));
    control(L"STATIC",L"",0,ColorThemeLabel,40,244,210,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,ColorTheme,272,238,440,260);
    control(L"BUTTON",L"",BS_AUTOCHECKBOX|WS_TABSTOP,StartupAnimation,40,304,690,34);SendMessageW(GetDlgItem(mainWindow,StartupAnimation),BM_SETCHECK,startupAnimation?BST_CHECKED:BST_UNCHECKED,0);control(L"STATIC",L"",0,SettingsNote,40,368,680,110);
    buildingPage=1;buildingSettings=1;control(L"STATIC",L"",0,OpacityLabel,40,166,620,28);HWND opacity=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,OpacitySlider,40,196,680,32);SendMessageW(opacity,TBM_SETRANGE,TRUE,MAKELPARAM(5,90));SendMessageW(opacity,TBM_SETPOS,TRUE,glassConfig.opacity);
    control(L"STATIC",L"",0,BlurLabel,40,226,620,28);HWND blur=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,BlurSlider,40,256,680,32);SendMessageW(blur,TBM_SETRANGE,TRUE,MAKELPARAM(0,40));SendMessageW(blur,TBM_SETPOS,TRUE,glassConfig.blur);
    control(L"STATIC",L"",0,RadiusLabel,40,286,430,28);HWND radius=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,RadiusSlider,40,316,680,32);SendMessageW(radius,TBM_SETRANGE,TRUE,MAKELPARAM(0,28));SendMessageW(radius,TBM_SETPOS,TRUE,glassConfig.radius);control(L"STATIC",L"",0,EffectValue,480,286,240,28);
    control(L"STATIC",L"",0,TintLabel,40,362,220,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,TintCombo,272,356,440,150);
    control(L"BUTTON",L"",BS_AUTOCHECKBOX|WS_TABSTOP,BatteryFriendly,40,404,680,34);SendMessageW(GetDlgItem(mainWindow,BatteryFriendly),BM_SETCHECK,batteryFriendly?BST_CHECKED:BST_UNCHECKED,0);control(L"STATIC",L"",0,EffectNote,40,444,680,54);
    control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,RestoreOriginal,40,504,680,30);
    buildingPage=2;buildingSettings=2;control(L"STATIC",L"",0,AboutText,40,182,680,230);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,ExitApp,40,446,460,42);
    buildingSettings=-1;buildingPage=-1;
    status=control(L"STATIC",L"",0,Status,24,552,570,64);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Pause,614,570,152,36);localize();selectPage(0);
}
