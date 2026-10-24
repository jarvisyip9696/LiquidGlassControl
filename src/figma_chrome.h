#pragma once
// Figma artwork is embedded, so previews work offline in the portable executable.
struct FigmaArtwork {
    ULONG_PTR token{};
    IStream* streams[6]{};
    std::unique_ptr<Gdiplus::Bitmap> images[6];
    FigmaArtwork(){
        Gdiplus::GdiplusStartupInput input;
        if(Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok)return;
        for(int i=0;i<6;++i){auto resource=FindResourceW(nullptr,MAKEINTRESOURCEW(210+i),RT_RCDATA);if(!resource)continue;
            DWORD size=SizeofResource(nullptr,resource);auto loaded=LoadResource(nullptr,resource);auto data=LockResource(loaded);
            HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(!memory)continue;auto target=GlobalLock(memory);if(!target){GlobalFree(memory);continue;}memcpy(target,data,size);GlobalUnlock(memory);
            if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&streams[i]))){GlobalFree(memory);continue;}
            images[i].reset(Gdiplus::Bitmap::FromStream(streams[i]));
        }
    }
    ~FigmaArtwork(){for(int i=0;i<6;++i){images[i].reset();if(streams[i])streams[i]->Release();}if(token)Gdiplus::GdiplusShutdown(token);}
};
inline void figmaImage(HDC dc,int index,RECT r){static FigmaArtwork artwork;if(index<0||index>=6||!artwork.images[index])return;
    Gdiplus::Graphics graphics(dc);graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    if(index>=3){Gdiplus::ImageAttributes attributes;attributes.SetColorKey(Gdiplus::Color(255,245,245,245),Gdiplus::Color(255,255,255,255));auto&source=artwork.images[index];
        graphics.DrawImage(source.get(),Gdiplus::Rect(int(r.left),int(r.top),int(r.right-r.left),int(r.bottom-r.top)),0,0,int(source->GetWidth()),int(source->GetHeight()),Gdiplus::UnitPixel,&attributes);
    }else graphics.DrawImage(artwork.images[index].get(),int(r.left),int(r.top),int(r.right-r.left),int(r.bottom-r.top));
}
RECT navigationRect(int index){int y=54*index;return {0,MulDiv(y,dpi,96),MulDiv(184,dpi,96),MulDiv(y+46,dpi,96)};}
int navigationHit(LPARAM l){int x=static_cast<short>(LOWORD(l)),y=static_cast<short>(HIWORD(l));for(int i=0;i<3;++i){RECT r=navigationRect(i);POINT p{x,y};if(PtInRect(&r,p))return i;}return -1;}
void paintNavigation(HDC dc,HWND window){
    RECT bounds{};GetClientRect(window,&bounds);FillRect(dc,&bounds,background);auto px=[](int n){return MulDiv(n,dpi,96);};
    for(int i=0;i<3;++i){RECT r=navigationRect(i);bool selected=i==currentPage;bool hot=motionOf(window)->hotTab==i;
        COLORREF fill=selected?cardColor():hot?palette().selection:palette().canvas;roundFill(dc,r,fill,fill,px(11));
        if(selected){RECT mark{r.left,r.top+px(15),r.left+px(3),r.top+px(31)};roundFill(dc,mark,palette().accent,palette().accent,px(2));}
        figmaImage(dc,3+i,{px(14),r.top+px(14),px(32),r.top+px(32)});
        wchar_t text[256]{};TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=text;item.cchTextMax=256;TabCtrl_GetItem(window,i,&item);
        RECT label=r;label.left+=px(44);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,selected?palette().accent:mutedColor());SelectObject(dc,selected?actionFont:font);DrawTextW(dc,text,-1,&label,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        if(showFocus(window)&&TabCtrl_GetCurFocus(window)==i)drawFocusRing(dc,r);
    }
}
void paintChrome(HWND h){
    if(!appui::legacy&&(!selfTestMode||appReviewMode||appui::active())){appui::paint(h,nullptr);return;}
    ControlPaint paint(h);HDC dc=paint.dc;RECT bounds{};GetClientRect(h,&bounds);if(bounds.right<=0||bounds.bottom<=0)return;
    auto px=[](int n){return MulDiv(n,dpi,96);};auto rect=[&](int x,int y,int w,int height){return RECT{px(x),px(y),px(x+w),px(y+height)};};
    COLORREF top=palette().canvas,bottom=highContrast()?top:darkUi()?RGB(15,29,47):colorTheme==8?palette().canvas:RGB(222,234,249);
    for(int y=0;y<bounds.bottom;++y){RECT row{0,y,bounds.right,y+1};HBRUSH brush=CreateSolidBrush(mix(top,bottom,float(y)/bounds.bottom));FillRect(dc,&row,brush);DeleteObject(brush);}
    auto panel=[&](int x,int y,int w,int height,bool subdued=false){auto r=rect(x,y,w,height);roundFill(dc,r,subdued?palette().selection:cardColor(),highContrast()?palette().edge:darkUi()?palette().edge:RGB(255,255,255),px(16));};
    auto text=[&](const wchar_t*value,int x,int y,int w,int height,HFONT face,COLORREF color,UINT flags=DT_LEFT|DT_WORDBREAK|DT_NOPREFIX){auto r=rect(x,y,w,height);SetBkMode(dc,TRANSPARENT);SelectObject(dc,face);SetTextColor(dc,color);DrawTextW(dc,value,-1,&r,flags);};
    RECT sidebar=rect(0,0,216,804);FillRect(dc,&sidebar,background);
    text(tr(L"工作空间",L"WORKSPACE"),16,32,184,20,font,mutedColor());
    panel(16,671,184,109);text(L"LGEngine",30,687,150,22,actionFont,textColor());
    text(tr(L"独立材质引擎\n无需 Windhawk",L"Independent materials\nNo Windhawk required"),30,717,156,40,font,mutedColor());text(L"v" LG_VERSION_W,30,762,150,18,font,mutedColor());
    const wchar_t* titles[]={tr(L"应用玻璃",L"App glass"),tr(L"系统玻璃",L"System glass"),tr(L"设置",L"Settings")};
    const wchar_t* subtitles[]={tr(L"为选定窗口赋予更轻盈的材质，让桌面保持专注。",L"Give selected windows a lighter material and keep your desktop focused."),tr(L"独立控制系统界面，按需调整每一种玻璃材质。",L"Control each system surface and fine-tune its glass material."),tr(L"让 Liquid Glass 更符合你的使用习惯。",L"Make Liquid Glass feel at home on your desktop.")};
    text(titles[currentPage],248,32,1000,40,headingFont,textColor());text(subtitles[currentPage],248,80,1000,24,font,mutedColor());
    if(currentPage==0&&pageScroll[0]==0){panel(248,120,600,304);panel(248,440,600,332);}
    else if(currentPage==2){panel(248,120,600,354);panel(248,490,600,120);panel(248,628,600,58,true);panel(248,704,600,52);}
    else if(currentPage==1)panel(248,176,600,596);
    else panel(248,120,600,652);
    panel(872,120,376,652);
    if(currentPage==0){
        text(tr(L"选择应用窗口",L"Choose an app window"),268,140,340,25,actionFont,textColor());
        if(pageScroll[0]==0)text(tr(L"窗口材质与标题栏",L"Window material and title bar"),268,456,500,22,actionFont,textColor());
    }else if(currentPage==1){
        text(systemView==2?tr(L"LGEngine 动画与指示器",L"LGEngine animation and indicators"):systemView==1?tr(L"按区域独立调整材质",L"Tune each surface independently"):tr(L"效果开关 · 点击箭头调整独立参数",L"Effects · expand a row to adjust its parameters"),268,196,560,24,actionFont,textColor());
    }else{
        text(tr(L"界面偏好",L"Interface preferences"),268,140,560,24,actionFont,textColor());
        text(tr(L"关闭窗口后，继续在托盘运行",L"Keep running in the tray when closed"),268,508,560,25,actionFont,textColor());
        text(tr(L"设置自动保存，即时生效。",L"Preferences save automatically and take effect immediately."),268,648,560,25,font,mutedColor());
    }
    if(currentPage==2){
        text(tr(L"关于 Liquid Glass",L"About Liquid Glass"),892,140,336,25,actionFont,textColor());figmaImage(dc,2,rect(892,178,336,242));
        text(L"Liquid Glass",892,440,336,36,headingFont,textColor());
    }else{
        text(tr(L"材质预览",L"Material preview"),892,140,220,24,actionFont,textColor());text(L"LIGHT · DEPTH · GLASS",892,166,230,18,font,mutedColor());
        text(tr(L"仅预览",L"PREVIEW"),1160,142,68,22,font,palette().accent);
        figmaImage(dc,currentPage==0?0:1,rect(892,192,336,342));
        const wchar_t* qualities[]={tr(L"柔光漫射",L"Diffusion"),tr(L"边缘高光",L"Highlights"),tr(L"层次景深",L"Depth")};
        for(int i=0;i<3;++i){text(qualities[i],892+i*115,559,106,22,font,mutedColor(),DT_CENTER|DT_SINGLELINE|DT_NOPREFIX);}
        text(tr(L"在这里感受玻璃材质",L"A study in light and glass"),892,616,336,24,actionFont,textColor());
        text(tr(L"此区域展示设计中的材质效果。实际应用效果取决于目标窗口；参数面板继续控制原有材质与指示器。",L"This study illustrates the design material. Actual effects depend on the target window. Controls adjust the existing materials and indicators."),892,652,336,94,font,mutedColor());
    }
    RECT footer=rect(0,804,1280,44);HBRUSH b=CreateSolidBrush(palette().canvas);FillRect(dc,&footer,b);DeleteObject(b);
}
