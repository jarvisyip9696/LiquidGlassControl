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
// Row expanders: the chevron button after a row's switch. Its window text is the accessible name and follows the open state; Space clicks it natively, Enter is taken from the dialog manager's default button.
void labelExpanders(){
    // plain row names (the heading text can carry a status suffix from updateNativeInfo)
    const wchar_t*zh[]={L"音量提示",L"亮度提示",L"任务栏",L"开始菜单",L"通知中心",L"日历",L"资源管理器",L"Dock 悬停动画",L"标题栏玻璃"};const wchar_t*en[]={L"Volume indicator",L"Brightness indicator",L"Taskbar",L"Start menu",L"Notification center",L"Calendar",L"File Explorer",L"Dock hover animation",L"Title-bar glass"};
    for(int i=0;i<9;++i){HWND e=GetDlgItem(mainWindow,ExpandVolume+i);if(!e)continue;bool open=i==8?titleOpen:systemOpen==i;if(open)SetPropW(e,L"LG.Open",(HANDLE)1);else RemovePropW(e,L"LG.Open");
        const wchar_t*name=tr(zh[i],en[i]);std::wstring text=open?tr(L"隐藏“%s”设置",L"Hide %s settings"):tr(L"显示“%s”设置",L"Show %s settings");auto at=text.find(L"%s");if(at!=std::wstring::npos)text.replace(at,2,name);SetWindowTextW(e,text.c_str());InvalidateRect(e,nullptr,FALSE);}
}
LRESULT CALLBACK expanderKeys(HWND h,UINT m,WPARAM w,LPARAM l,UINT_PTR id,DWORD_PTR){
    if(m==WM_GETDLGCODE&&l&&(reinterpret_cast<MSG*>(l)->message==WM_KEYDOWN||reinterpret_cast<MSG*>(l)->message==WM_CHAR)&&reinterpret_cast<MSG*>(l)->wParam==VK_RETURN)return DefSubclassProc(h,m,w,l)|DLGC_WANTMESSAGE;
    if(m==WM_KEYDOWN&&w==VK_RETURN){if(IsWindowEnabled(h))SendMessageW(GetParent(h),WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(h),BN_CLICKED),reinterpret_cast<LPARAM>(h));return 0;}
    if(m==WM_CHAR&&w==VK_RETURN)return 0;
    if(m==WM_NCDESTROY){RemovePropW(h,L"LG.Expander");RemovePropW(h,L"LG.Open");RemoveWindowSubclass(h,expanderKeys,id);}
    return DefSubclassProc(h,m,w,l);
}
void localizeOptical(){auto lang=int(l10n::current);auto ui=opticalui::kUi[lang];for(int i=0;i<4;++i)SetDlgItemTextW(mainWindow,OptLabel1+i,ui[i]);SetDlgItemTextW(mainWindow,OptReplay,ui[4]);SetDlgItemTextW(mainWindow,OptSample,ui[5]);int ids[]={OptArrival,OptIndicator,OptSlosh,OptKnob};int counts[]={9,9,4,3};const wchar_t* const* lists[]={opticalui::kArrival[lang],opticalui::kArrival[lang],opticalui::kSlosh[lang],opticalui::kKnob[lang]};for(int i=0;i<4;++i){HWND c=GetDlgItem(mainWindow,ids[i]);int keep=selection(c);SendMessageW(c,CB_RESETCONTENT,0,0);if(i==1)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)ui[7]);for(int j=0;j<counts[i];++j)SendMessageW(c,CB_ADDSTRING,0,(LPARAM)lists[i][j]);SendMessageW(c,CB_SETCURSEL,keep<0?(i==0||i==1?5:i==3?2:0):keep,0);}{HWND shape=GetDlgItem(mainWindow,OptShape);if(shape){int keep=selection(shape);SendMessageW(shape,CB_RESETCONTENT,0,0);for(int j=0;j<4;++j)SendMessageW(shape,CB_ADDSTRING,0,(LPARAM)opticalui::kShape[lang][j]);SendMessageW(shape,CB_SETCURSEL,keep<0?0:keep,0);}updateLookLabels();}SetDlgItemTextW(mainWindow,OptNote,tr(L"设置自动保存。只要音量或亮度开关开启，就会应用于指示器。",L"Settings save automatically. They apply to the indicator whenever the Volume or Brightness switch is on."));}
void localize(){
    SetWindowTextW(mainWindow,L"Liquid Glass · v" LG_VERSION_W);
    struct Text{int id;const wchar_t*zh;const wchar_t*en;};
    const Text texts[]={
      {Header,L"Liquid Glass",L"Liquid Glass"},{Refresh,L"刷新窗口",L"Refresh windows"},{Apply,L"应用",L"Apply"},{Restore,L"还原",L"Restore"},{Pause,L"隐藏至托盘",L"Hide to tray"},
      {Note,L"选择应用后点击「应用」，点击「还原」撤销效果。\r\n系统玻璃独立运行；部分应用的自绘内容区仍可能不透明。",L"Choose an app and Apply; Restore reverts the effect.\r\nSystem glass runs independently; custom-drawn content can remain opaque."},
      {VolumeHeading,L"音量提示",L"Volume indicator"},{BrightnessHeading,L"亮度提示",L"Brightness indicator"},
      {VolumeOn,L"音量玻璃",L"Volume glass"},{BrightnessOn,L"亮度玻璃",L"Brightness glass"},
      {TaskbarToggle,L"任务栏玻璃",L"Taskbar glass"},{StartToggle,L"开始菜单玻璃",L"Start menu glass"},{NotificationToggle,L"通知中心玻璃",L"Notification center glass"},
      {TaskbarHeading,L"任务栏",L"Taskbar"},{StartHeading,L"开始菜单",L"Start menu"},{NotificationHeading,L"通知中心",L"Notification center"},{CalendarHeading,L"日历",L"Calendar"},{CalendarToggle,L"日历玻璃",L"Calendar glass"},
      {ExplorerToggle,L"资源管理器玻璃",L"Explorer glass"},{ExplorerHeading,L"资源管理器",L"File Explorer"},
      {BatteryFriendly,L"节电模式时降低材质开销",L"Reduce material cost in battery saver"},
      {RestoreOriginal,L"重置所有外观",L"Reset all looks"},
      {KeyboardToggle,L"键盘布局切换",L"Keyboard layout switcher"},
      {OpticalPreview,L"LGEngine 光学预览",L"LGEngine optical preview"},
      {LanguageLabel,L"界面语言",L"Interface language"},{ColorThemeLabel,L"界面配色",L"Interface color"},{StartupAnimation,L"程序启动时淡入并轻轻上移",L"Fade and gently slide the app in at startup"},
      {SettingsNote,L"语言、配色和启动动画偏好自动保存。\r\n关闭设置窗口会继续在托盘运行。",L"Language, color and startup animation preferences are saved automatically.\r\nClosing settings keeps the app running in the tray."},
      {AboutText,L"Liquid Glass " LG_VERSION_W L"\r\n独立 Windows 材质引擎，单 EXE 分发。\r\n\r\n事件触发，无后台定时扫描。闲置时不持续重绘。\r\n图标缓存随窗口列表清理；XAML 节点使用弱引用。\r\n电池模式可使用低成本回退材质。\r\n不包含完整光学折射。",L"Liquid Glass " LG_VERSION_W L"\r\nStandalone Windows materials, distributed as one EXE.\r\n\r\nEvent driven, with no periodic background scans or idle repaints.\r\nIcon cache is pruned; XAML nodes use weak references.\r\nOptional low-cost material fallback on battery.\r\nFull optical refraction is not implemented."},
      {DockHeading,L"Dock 悬停动画",L"Dock hover animation"},{DockToggle,L"Dock 悬停动画",L"Dock hover animation"},
      {TitleHeading,L"标题栏玻璃",L"Title-bar glass"},{TitleOn,L"标题栏玻璃",L"Title-bar glass"},

      {ExitApp,L"还原并退出",L"Restore and exit"}
    };
    for(auto&t:texts)SetWindowTextW(GetDlgItem(mainWindow,t.id),tr(t.zh,t.en));
    {const wchar_t*fade[]={L"Smooth fade at startup",L"启动时平滑淡入",L"啟動時平順淡入",L"Płynne pojawianie przy starcie",L"Hiện dần mượt khi khởi động"};SetWindowTextW(GetDlgItem(mainWindow,StartupAnimation),fade[int(l10n::current)]);}
    const wchar_t*names[]={tr(L"应用玻璃",L"App glass"),tr(L"系统玻璃",L"System glass"),tr(L"设置",L"Settings")};
    for(int i=0;i<3;++i){TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(names[i]);TabCtrl_SetItem(tabs,i,&item);}
    localizeOptical();
    int s=selection(scope),m=selection(material),c=selection(corners);
    combo(scope,{tr(L"所选应用（按进程名）",L"Selected app (by executable name)"),tr(L"资源管理器窗口",L"File Explorer windows"),tr(L"全部应用（内容区不保证）",L"All apps (content not guaranteed)")},s);
    combo(material,{L"Acrylic",L"Mica",L"Mica Alt",tr(L"关闭背景材质",L"No backdrop")},m);combo(corners,{tr(L"圆角",L"Rounded"),tr(L"小圆角",L"Small rounded"),tr(L"直角",L"Square")},c);
    {HWND borderBox=GetDlgItem(mainWindow,AppBorder),textBox=GetDlgItem(mainWindow,AppText);int b=selection(borderBox),t=selection(textBox);
    combo(borderBox,{tr(L"边框：默认",L"Border: default"),tr(L"边框：无",L"Border: none"),tr(L"边框：强调色",L"Border: accent"),tr(L"边框：浅色",L"Border: light"),tr(L"边框：深色",L"Border: dark")},b);
    combo(textBox,{tr(L"标题文字：默认",L"Title text: default"),tr(L"标题文字：浅色",L"Title text: light"),tr(L"标题文字：深色",L"Title text: dark")},t);updateTitleLabels();}
    combo(GetDlgItem(mainWindow,ColorTheme),{tr(L"经典蓝",L"Classic blue"),tr(L"流光紫",L"Violet"),tr(L"青绿",L"Jade"),tr(L"暖橙",L"Warm orange"),tr(L"玫瑰",L"Rose"),tr(L"湖蓝",L"Lake blue"),tr(L"石墨灰",L"Graphite")},colorTheme);
    {const wchar_t*background[]={L"Adapt to background",L"根据背景自适应",L"依背景自動調整",L"Dopasuj do tła",L"Tự điều chỉnh theo nền"};combo(GetDlgItem(mainWindow,LookTint),{tr(L"跟随系统",L"Follow system"),tr(L"浅色",L"Light"),tr(L"深色",L"Dark"),background[int(l10n::current)]},(int)glassConfig.tint);}loadLookPanel();labelExpanders();
    lstrcpynW(tray.szTip,tr(L"Liquid Glass · 原生提示条",L"Liquid Glass · Native indicators"),128);if(tray.hWnd)Shell_NotifyIconW(NIM_MODIFY,&tray);
    combo(language,{l10n::names[0],l10n::names[1],l10n::names[2],l10n::names[3],l10n::names[4]},int(l10n::current));createFonts();fitTabItems(tabs);refresh(entries.empty());updateNativeInfo();updateTitleNote(true);
    message(migrationJob?tr(L"正在恢复旧主题……",L"Restoring the old theme..."):tr(L"选择应用或切换分页。语言在“设置”中；点击系统玻璃各项的箭头可调整其参数。",L"Choose an app or a page. Language: Settings. Each System glass row opens its own settings."));
}
HWND control(const wchar_t*cls,const wchar_t*text,DWORD style,int id,int x,int y,int w,int h){
    bool isSwitch=_wcsicmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_AUTOCHECKBOX;if(isSwitch)style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
    if(_wcsicmp(cls,L"BUTTON")==0&&((style&BS_TYPEMASK)==BS_PUSHBUTTON||(style&BS_TYPEMASK)==BS_DEFPUSHBUTTON))style=(style&~BS_TYPEMASK)|BS_OWNERDRAW;
    if(_wcsicmp(cls,L"COMBOBOX")==0)style|=CBS_OWNERDRAWFIXED|CBS_HASSTRINGS;
    auto px=[](int a){return MulDiv(a,dpi,96);};HWND result=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),mainWindow,reinterpret_cast<HMENU>((INT_PTR)id),GetModuleHandleW(nullptr),nullptr);
    if(result)controlLayout[result]={x,y,x+w,y+h};if(_wcsicmp(cls,L"LISTBOX")==0)SetWindowSubclass(result,listHover,5,0);
    SendMessageW(result,WM_SETFONT,reinterpret_cast<WPARAM>(id==Header?headingFont:font),TRUE);SetWindowTheme(result,L"Explorer",nullptr);if(isSwitch){SetPropW(result,L"LG.IsSwitch",(HANDLE)1);SetWindowSubclass(result,compactSwitch,3,0);}if(_wcsicmp(cls,TRACKBAR_CLASSW)==0)SetWindowSubclass(result,modernSlider,4,0);if(_wcsicmp(cls,L"COMBOBOX")==0){SetWindowSubclass(result,themedControl,2,1);SendMessageW(result,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(23));SendMessageW(result,CB_SETITEMHEIGHT,0,px(26));}if(_wcsicmp(cls,WC_TABCONTROLW)==0)SetWindowSubclass(result,themedControl,2,0);if(!isSwitch&&_wcsicmp(cls,L"BUTTON")==0&&(style&BS_TYPEMASK)==BS_OWNERDRAW)SetWindowSubclass(result,buttonHover,1,0);if(buildingPage>=0)pageControls[buildingPage].push_back(result);if(buildingBlock&&result)buildingBlock->items.push_back({result,RECT{x,y,x+w,y+h}});return result;
}
HWND expander(int id,int x,int y){HWND e=control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,id,x,y,34,32);SetPropW(e,L"LG.Expander",(HANDLE)1);SetWindowSubclass(e,expanderKeys,6,0);return e;}
// Page layout. A page is a sequence of blocks starting at ContentTop; a block's rects are relative to its top. Only controls wholly inside the card's view (ViewTop..ViewBottom,
// above the status line) are shown; a taller page scrolls in DIP (wheel, scrollbar, Tab) and an expanded row is scrolled into view together with its panel.
constexpr int ViewTop=100,ViewBottom=540,ContentTop=108;
bool isPanel(const Block*b){return b==&titlePanel||b==&indicatorPanel||b==&lookPanel||b==&dockPanel;}
std::vector<std::pair<const Block*,int>> pageSequence(int page){
    std::vector<std::pair<const Block*,int>> s;int y=ContentTop;auto add=[&](const Block&b){s.push_back({&b,y});y+=b.height;};
    if(page==0){add(appBlock);add(titleRow);if(titleOpen)add(titlePanel);}
    else if(page==1){for(int i=0;i<9;++i){add(systemRows[i]);if(systemOpen==i)add(i<2?indicatorPanel:i==7?dockPanel:lookPanel);}add(systemFooter);}
    else if(page==2)add(settingsBlock);
    return s;
}
int visualHeight(HWND h,const RECT&r){wchar_t cls[16]{};GetClassNameW(h,cls,16);return _wcsicmp(cls,L"COMBOBOX")?int(r.bottom-r.top):30;} // a closed combo is 30 DIP; its rect height is the drop-down
// Line span of a block item: the union of the vertical ranges of the items overlapping it, grown until stable, so a heading, its switch and chevron (or a label and its slider) show and hide together.
std::pair<int,int> lineSpan(const Block&b,size_t index){
    auto range=[&](size_t i){int top=b.items[i].second.top;return std::make_pair(top,top+visualHeight(b.items[i].first,b.items[i].second));};auto span=range(index);
    for(bool grown=true;grown;){grown=false;for(size_t i=0;i<b.items.size();++i){auto r=range(i);if(r.first<span.second&&r.second>span.first&&(r.first<span.first||r.second>span.second)){span={std::min(span.first,r.first),std::max(span.second,r.second)};grown=true;}}}
    return span;
}
void placePage(){
    auto seq=pageSequence(currentPage);int end=seq.empty()?ContentTop:seq.back().second+seq.back().first->height;for(auto&entry:seq)for(size_t i=0;i<entry.first->items.size();++i)end=std::max(end,entry.second+lineSpan(*entry.first,i).second);
    int&scroll=pageScroll[currentPage];int&maxScroll=pageMaxScroll[currentPage];maxScroll=std::max(0,end+4-ViewBottom);
    for(size_t i=0;revealBlock&&i<seq.size();++i)if(seq[i].first==revealBlock){int bottom=seq[i].second+revealBlock->height;if(i+1<seq.size()&&isPanel(seq[i+1].first))bottom+=seq[i+1].first->height;scroll=std::min(std::max(scroll,bottom+4-ViewBottom),seq[i].second-ContentTop);}
    revealBlock=nullptr;scroll=std::clamp(scroll,0,maxScroll);
    auto px=[](int n){return MulDiv(n,dpi,96);};std::set<HWND> placed;HWND after=tabs;
    for(auto&entry:seq)for(size_t i=0;i<entry.first->items.size();++i){auto&item=entry.first->items[i];HWND h=item.first;RECT r=item.second;OffsetRect(&r,0,entry.second-scroll);controlLayout[h]=r;auto span=lineSpan(*entry.first,i);bool show=entry.second-scroll+span.first>=ViewTop&&entry.second-scroll+span.second<=ViewBottom;
        SetWindowPos(h,after,px(r.left),px(r.top),px(r.right-r.left),px(r.bottom-r.top),SWP_NOACTIVATE|(show?SWP_SHOWWINDOW:SWP_HIDEWINDOW));after=h;placed.insert(h);}
    for(int p=0;p<3;++p)for(HWND h:pageControls[p])if(!placed.count(h)&&(GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE))ShowWindow(h,SW_HIDE);
    if(pageScrollBar){SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_ALL;si.nMin=0;si.nMax=maxScroll+(ViewBottom-ViewTop)-1;si.nPage=ViewBottom-ViewTop;si.nPos=scroll;SetScrollInfo(pageScrollBar,SB_CTL,&si,TRUE);SetWindowPos(pageScrollBar,after,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|(maxScroll>0?SWP_SHOWWINDOW:SWP_HIDEWINDOW));}
}
// Redraw is switched off only while the window is shown: WM_SETREDRAW FALSE clears WS_VISIBLE, so focus is checked by style after redraw is back on.
void placeAndRedraw(){
    bool redraw=IsWindowVisible(mainWindow)!=FALSE;if(redraw)SendMessageW(mainWindow,WM_SETREDRAW,FALSE,0);placePage();
    if(redraw){SendMessageW(mainWindow,WM_SETREDRAW,TRUE,0);RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
    if(HWND focused=GetFocus();focused&&IsChild(mainWindow,focused)&&!(GetWindowLongPtrW(focused,GWL_STYLE)&WS_VISIBLE))SetFocus(tabs);
}
void relayout(){labelExpanders();placeAndRedraw();if(currentPage==1)updateNativeInfo();syncIndicatorPanel();}
void scrollPage(int offset){offset=std::clamp(offset,0,pageMaxScroll[currentPage]);if(offset==pageScroll[currentPage])return;pageScroll[currentPage]=offset;placeAndRedraw();}
// Opens one System glass row (-1 closes all); a row's own sliders load first. Callers that used to jump to a sub-tab call this instead.
void expandSystemRow(int row){if(row<-1||row>7)return;systemOpen=row;if(row>=2)loadLookPanel();if(row>=0)revealBlock=&systemRows[row];if(row>=0&&currentPage!=1)selectPage(1);else relayout();}
void expandTitleRow(bool open){titleOpen=open;if(open)revealBlock=&titleRow;if(open&&currentPage!=0)selectPage(0);else relayout();}
// Called from the message loop before the dialog manager handles Tab: the next tab stop of a scrolled page is scrolled into view first, because hidden controls are skipped.
void revealTabTarget(bool back){
    if(pageMaxScroll[currentPage]<=0)return;HWND focus=GetFocus();std::vector<std::pair<HWND,std::pair<int,int>>> stops;
    for(auto&entry:pageSequence(currentPage))for(size_t i=0;i<entry.first->items.size();++i){HWND c=entry.first->items[i].first;if((GetWindowLongPtrW(c,GWL_STYLE)&WS_TABSTOP)&&IsWindowEnabled(c)){auto span=lineSpan(*entry.first,i);stops.push_back({c,{entry.second+span.first,span.second-span.first}});}}
    if(stops.empty())return;int next=-1;for(int i=0;i<(int)stops.size();++i)if(stops[i].first==focus)next=back?i-1:i+1;
    if(next<0&&!back&&focus==tabs)next=0;if(next<0&&back&&focus==GetDlgItem(mainWindow,Pause))next=(int)stops.size()-1;if(next<0||next>=(int)stops.size())return;
    int top=stops[next].second.first,bottom=top+stops[next].second.second,scroll=pageScroll[currentPage];if(top-scroll<ViewTop)scroll=top-ContentTop;if(bottom-scroll>ViewBottom)scroll=bottom+4-ViewBottom;scrollPage(scroll);
}
void tabItem(HWND tab,int count,int){for(int i=0;i<count;++i){TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(L" ");TabCtrl_InsertItem(tab,i,&item);}fitTabItems(tab);}
void resizeControls(){auto px=[](int n){return MulDiv(n,dpi,96);};for(auto&entry:controlLayout){auto h=entry.first;auto&r=entry.second;SetWindowPos(h,nullptr,px(r.left),px(r.top),px(r.right-r.left),px(r.bottom-r.top),SWP_NOZORDER|SWP_NOACTIVATE);wchar_t cls[64]{};GetClassNameW(h,cls,64);if(!_wcsicmp(cls,L"COMBOBOX")){SendMessageW(h,CB_SETITEMHEIGHT,static_cast<WPARAM>(-1),px(23));SendMessageW(h,CB_SETITEMHEIGHT,0,px(26));}if(h==list)SendMessageW(h,LB_SETITEMHEIGHT,0,px(48));}fontLanguage=-1;createFonts();RemovePropW(tabs,L"LG.Sel");fitTabItems(tabs);RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);}
void buildUI(){
    createFonts();control(L"STATIC",L"",0,Header,68,14,450,38);
    DWORD tabStyle=WS_TABSTOP|TCS_OWNERDRAWFIXED|TCS_FIXEDWIDTH|TCS_BUTTONS|TCS_FLATBUTTONS;
    tabs=control(WC_TABCONTROLW,L"",tabStyle,Pages,24,54,742,38);tabItem(tabs,3,240);
    // App glass: apps and backdrop, then the Title-bar glass row; its panel holds the inset, the eight title sliders and the note. Rects are DIP, relative to the block top.
    buildingPage=0;buildingBlock=&appBlock;appBlock.height=382;
    scope=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,Scope,40,0,320,180);material=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Material,372,0,170,180);corners=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Corners,554,0,182,150);
    control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,AppBorder,40,40,340,150);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,AppText,392,40,344,150);
    list=control(L"LISTBOX",L"",LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS|WS_BORDER|WS_VSCROLL|WS_TABSTOP,List,40,80,696,148);SendMessageW(list,LB_SETITEMHEIGHT,0,MulDiv(48,dpi,96));
    compatibility=control(L"STATIC",L"",0,Compatibility,40,234,696,36);
    control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Refresh,40,276,200,34);control(L"BUTTON",L"",BS_DEFPUSHBUTTON|WS_TABSTOP,Apply,252,276,240,34);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Restore,504,276,232,34);note=control(L"STATIC",L"",0,Note,40,318,696,58);
    buildingBlock=&titleRow;titleRow.height=36;
    control(L"STATIC",L"",0,TitleHeading,40,4,556,28);control(L"BUTTON",L"",BS_AUTOCHECKBOX|WS_TABSTOP,TitleOn,604,0,100,32);expander(ExpandTitle,710,0);CheckDlgButton(mainWindow,TitleOn,persistent()&&GetPrivateProfileIntW(L"AppGlass",L"TitleOn",0,iniPath.c_str())?BST_CHECKED:BST_UNCHECKED);
    buildingBlock=&titlePanel;titlePanel.height=282;
    control(L"STATIC",L"",0,TitleInsetLabel,40,4,132,28);{HWND inset=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,TitleInset,174,0,186,30);SendMessageW(inset,TBM_SETRANGE,TRUE,MAKELPARAM(0,16));SendMessageW(inset,TBM_SETPOS,TRUE,std::clamp((int)GetPrivateProfileIntW(L"AppGlass",L"TitleInset",0,iniPath.c_str()),0,16));}
    for(int i=0;i<8;++i){int x=i%2?392:40,y=36+(i/2)*36;control(L"STATIC",L"",0,TitleLabel1+i,x,y+2,132,28);HWND titleSlider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,TitleLook1+i,x+134,y,186,30);SendMessageW(titleSlider,TBM_SETRANGE,TRUE,MAKELPARAM(0,lookMax[i]));SendMessageW(titleSlider,TBM_SETPOS,TRUE,std::clamp((int)GetPrivateProfileIntW(L"AppGlass",titleKey(i).c_str(),titleDefault[i],iniPath.c_str()),0,lookMax[i]));}
    control(L"STATIC",L"",SS_NOPREFIX,TitleNote,40,180,696,96);
    // System glass: nine rows (switch + expander; Keyboard has no panel). Volume and Brightness share the LGEngine indicator panel, Taskbar..Explorer share one look panel
    // (loaded for the open row's family), Dock has its own; the footer holds the status text and Reset all looks.
    buildingPage=1;
    int headings[]={VolumeHeading,BrightnessHeading,TaskbarHeading,StartHeading,NotificationHeading,CalendarHeading,ExplorerHeading,DockHeading,KeyboardHeading};int switches[]={VolumeOn,BrightnessOn,TaskbarToggle,StartToggle,NotificationToggle,CalendarToggle,ExplorerToggle,DockToggle,KeyboardToggle};
    for(int i=0;i<9;++i){buildingBlock=&systemRows[i];systemRows[i].height=36;control(L"STATIC",L"",0,headings[i],40,4,556,28);HWND toggle=control(L"BUTTON",L"",BS_OWNERDRAW|WS_TABSTOP,switches[i],604,0,100,32);RemoveWindowSubclass(toggle,buttonHover,1);SetPropW(toggle,L"LG.IsSwitch",(HANDLE)1);SetWindowSubclass(toggle,compactSwitch,3,0);if(i<8)expander(ExpandVolume+i,710,0);}
    buildingBlock=&lookPanel;lookPanel.height=74;
    {int ids[]={LookOpacity,LookBlur,LookRadius},low[]={5,0,0},high[]={90,40,28};for(int i=0;i<4;++i){int x=i%2?392:40,y=2+(i/2)*36;if(i<3){control(L"STATIC",L"",0,ids[i]-1,x,y+2,140,28);HWND s=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,ids[i],x+142,y,178,30);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(low[i],high[i]));}else{control(L"STATIC",L"",0,LookTintLabel,x,y+2,140,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,LookTint,x+142,y,178,150);}}}
    buildingBlock=&dockPanel;dockPanel.height=74;
    {int ids[]={DockScale,DockSpread,DockSpeed,DockBounce},low[]={100,50,0,0},high[]={200,300,100,100};for(int i=0;i<4;++i){int x=i%2?392:40,y=2+(i/2)*36;control(L"STATIC",L"",0,ids[i]-1,x,y+2,140,28);HWND s=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,ids[i],x+142,y,178,30);SendMessageW(s,TBM_SETRANGE,TRUE,MAKELPARAM(low[i],high[i]));}}
    buildingBlock=&systemFooter;systemFooter.height=86;
    nativeInfo=control(L"STATIC",L"",0,NativeInfo,40,6,696,44);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,RestoreOriginal,40,52,220,30);
    buildingBlock=&indicatorPanel;indicatorPanel.height=334;
    int opticalLabels[]={OptLabel1,OptLabel2,OptLabel3,OptLabel4};int opticalCombos[]={OptArrival,OptIndicator,OptSlosh,OptKnob};
    for(int i=0;i<4;++i){int x=i%2?392:40,y=2+(i/2)*36;control(L"STATIC",L"",0,opticalLabels[i],x,y+4,130,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,opticalCombos[i],x+132,y,188,280);}
    control(L"STATIC",L"",0,OptShapeLabel,40,78,130,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,OptShape,172,74,188,150);
    control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,OptReplay,392,74,156,32);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,OptSample,556,74,156,32);
    for(int i=0;i<8;++i){int x=i%2?392:40,y=112+(i/2)*34;control(L"STATIC",L"",0,OptLookLabel1+i,x,y+2,132,28);HWND lookSlider=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,OptLook1+i,x+134,y,186,30);SendMessageW(lookSlider,TBM_SETRANGE,TRUE,MAKELPARAM(0,lookMax[i]));SendMessageW(lookSlider,TBM_SETPOS,TRUE,std::clamp((int)GetPrivateProfileIntW(L"LGEngine",lookKey[i],lookDefault[i],iniPath.c_str()),0,lookMax[i]));}
    control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,OpticalPreview,40,250,320,30);control(L"STATIC",L"",0,OptNote,40,286,696,44);
    // Settings: language, color, startup animation, battery saving, About.
    buildingPage=2;buildingBlock=&settingsBlock;settingsBlock.height=424;
    control(L"STATIC",L"",0,LanguageLabel,40,6,210,28);language=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP,Language,272,0,440,220);combo(language,{l10n::names[0],l10n::names[1],l10n::names[2],l10n::names[3],l10n::names[4]},int(l10n::current));
    control(L"STATIC",L"",0,ColorThemeLabel,40,46,210,28);control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,ColorTheme,272,40,440,260);
    control(L"BUTTON",L"",BS_AUTOCHECKBOX|WS_TABSTOP,StartupAnimation,40,80,690,34);SendMessageW(GetDlgItem(mainWindow,StartupAnimation),BM_SETCHECK,startupAnimation?BST_CHECKED:BST_UNCHECKED,0);
    control(L"BUTTON",L"",BS_AUTOCHECKBOX|WS_TABSTOP,BatteryFriendly,40,118,690,34);SendMessageW(GetDlgItem(mainWindow,BatteryFriendly),BM_SETCHECK,batteryFriendly?BST_CHECKED:BST_UNCHECKED,0);
    control(L"STATIC",L"",0,SettingsNote,40,160,696,44);control(L"STATIC",L"",0,AboutText,40,210,696,170);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,ExitApp,40,388,300,36);
    buildingBlock=nullptr;buildingPage=-1;
    pageScrollBar=control(L"SCROLLBAR",L"",SBS_VERT,PageScroll,752,104,14,432);ShowWindow(pageScrollBar,SW_HIDE);
    status=control(L"STATIC",L"",0,Status,24,552,570,64);control(L"BUTTON",L"",BS_PUSHBUTTON|WS_TABSTOP,Pause,614,570,152,36);localize();{int ids[]={OptArrival,OptIndicator,OptSlosh,OptKnob};const wchar_t* keys[]={L"Arrival",L"Indicator",L"Slosh",L"Knob"};int defaults[]={5,5,0,2},maximum[]={8,9,3,2};for(int i=0;i<4;++i){int v=GetPrivateProfileIntW(L"LGEngine",keys[i],defaults[i],iniPath.c_str());SendDlgItemMessageW(mainWindow,ids[i],CB_SETCURSEL,std::clamp(v,0,maximum[i]),0);}SendDlgItemMessageW(mainWindow,OptShape,CB_SETCURSEL,std::clamp((int)GetPrivateProfileIntW(L"LGEngine",L"Shape",0,iniPath.c_str()),0,3),0);updateLookLabels();}opticalUiReady=true;selectPage(0);syncOpticalPreview();
}
