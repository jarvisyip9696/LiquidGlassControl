#pragma once

// Presentation-only adapter. Hidden native controls remain the source of truth
// for all backend commands. No window-material or LGEngine code lives here.
namespace appui {
inline float switchPosition(int id,bool on);
template<class T> void drop(T*&p){if(p){p->Release();p=nullptr;}}
inline std::wstring directory(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);std::wstring s=path;return s.substr(0,s.find_last_of(L"\\")+1);}
struct FontEnumerator final:IDWriteFontFileEnumerator {
    ULONG refs=1;IDWriteFactory*factory;IDWriteFontFile*file=nullptr;int position=-1;
    explicit FontEnumerator(IDWriteFactory*f):factory(f){factory->AddRef();}
    ~FontEnumerator(){drop(file);drop(factory);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteFontFileEnumerator)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE MoveNext(BOOL*has)override{if(!has)return E_POINTER;drop(file);*has=FALSE;if(++position>=2)return S_OK;auto path=directory()+L"fonts\\"+(position==0?L"Inter.ttf":L"NotoSansSC.ttf");HRESULT hr=factory->CreateFontFileReference(path.c_str(),nullptr,&file);*has=SUCCEEDED(hr);return hr;}
    HRESULT STDMETHODCALLTYPE GetCurrentFontFile(IDWriteFontFile**out)override{if(!out)return E_POINTER;*out=file;if(!file)return E_FAIL;file->AddRef();return S_OK;}
};
struct FontLoader final:IDWriteFontCollectionLoader {
    ULONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void**out)override{if(!out)return E_POINTER;*out=nullptr;if(id==__uuidof(IUnknown)||id==__uuidof(IDWriteFontCollectionLoader)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}ULONG STDMETHODCALLTYPE Release()override{ULONG n=--refs;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateEnumeratorFromKey(IDWriteFactory*f,const void*,UINT32,IDWriteFontFileEnumerator**out)override{if(!out)return E_POINTER;*out=new FontEnumerator(f);return S_OK;}
};
struct Theme {
    UINT32 top,middle,bottom,primary,secondary,tertiary,card,control,edge,selected,selectedEdge;
    float cardAlpha,controlAlpha;
};
inline Theme theme(){return darkUi()?Theme{0x0b1220,0x111c2e,0x0f172a,0xf8fafc,0x94a3b8,0x64748b,0xffffff,0xffffff,0xffffff,0x1e3a8a,0x3b82f6,18.f/255,16.f/255}:Theme{0xf4f9ff,0xedf4fa,0xdeeaf9,0x20364b,0x61798f,0x7f93a6,0xffffff,0xffffff,0xd7e4f0,0xe9f2ff,0x9dc3f1,184.f/255,217.f/255};}
struct Hit {int id;D2D1_RECT_F r;};
enum {NavApp=1,NavSystem,NavSettings,RefreshButton,Row0,Row1,MaterialBox,CornerBox,RestoreButton,ApplyButton,TrayButton};
inline const std::vector<Hit>&appHits(){static const std::vector<Hit> list={
    {NavApp,{16,52,200,98}},{NavSystem,{16,106,200,152}},{NavSettings,{16,160,200,206}},
    {RefreshButton,{721,140,828,178}},{Row0,{268,194,828,272}},{Row1,{268,280,828,358}},
    {MaterialBox,{268,523,540,561}},{CornerBox,{556,523,828,561}},
    {RestoreButton,{632,575,713,613}},{ApplyButton,{721,575,828,613}},{TrayButton,{1138,808,1256,840}}};return list;}
int hot=0,pressed=0,focus=RefreshButton,firstRow=0;bool keyboard=false,legacy=false,sample=false;int sampleSelection=1;
float scale=1,originX=0,originY=0;bool actionResult=false;
bool active(){return !legacy&&(!selfTestMode||appReviewMode);}
struct Renderer;void drawWorkspace(Renderer&);const std::vector<Hit>&hits();float dropdownRotation(int id);int snapshotOwner=0;
struct Renderer {
    ID2D1Factory*factory=nullptr;IDWriteFactory*write=nullptr;IWICImagingFactory*wic=nullptr;
    ID2D1RenderTarget*target=nullptr;ID2D1DCRenderTarget*dcTarget=nullptr;ID2D1HwndRenderTarget*windowTarget=nullptr;unsigned targetGeneration=0;ID2D1SolidColorBrush*brush=nullptr;ID2D1LinearGradientBrush*canvasGradient=nullptr;bool gradientDark=false;
    IDWriteFontCollection*fonts=nullptr;FontLoader*loader=nullptr;
    std::map<std::wstring,ID2D1Bitmap*> images;
    std::map<std::wstring,IDWriteTextFormat*> formats;
    using LayoutKey=std::tuple<std::wstring,float,float,float,bool,bool,bool,float,bool>;
    struct Layout {IDWriteTextLayout*text=nullptr;float x=0,y=0;};
    std::map<LayoutKey,Layout> layouts;size_t layoutCreations=0;
    void clearLayouts(){for(auto&entry:layouts)drop(entry.second.text);layouts.clear();}
    void trim(){clearLayouts();for(auto&entry:images)drop(entry.second);images.clear();for(auto&entry:formats)drop(entry.second);formats.clear();drop(canvasGradient);drop(brush);target=nullptr;drop(dcTarget);drop(windowTarget);drop(fonts);if(loader&&write)write->UnregisterFontCollectionLoader(loader);drop(loader);drop(write);drop(factory);drop(wic);}
    HRESULT status=E_PENDING,lastDraw=E_PENDING;
    Renderer()=default;
    HRESULT createTextFactory(){
        HRESULT hr=DWriteCreateFactory(DWRITE_FACTORY_TYPE_ISOLATED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&write));if(FAILED(hr))return hr;
        loader=new FontLoader();hr=write->RegisterFontCollectionLoader(loader);if(SUCCEEDED(hr)){int key=1;hr=write->CreateCustomFontCollection(loader,&key,sizeof(key),&fonts);if(FAILED(hr))write->UnregisterFontCollectionLoader(loader);}
        if(FAILED(hr)){drop(loader);drop(write);drop(factory);}return hr;
    }
    void selectTarget(ID2D1RenderTarget*next){
        if(target==next&&brush)return;
        for(auto&entry:images)drop(entry.second);images.clear();drop(canvasGradient);drop(brush);target=next;++targetGeneration;
        if(target)status=target->CreateSolidColorBrush(D2D1::ColorF(0),&brush);
    }
    bool ensureFactory(){if(!write&&FAILED(status=createTextFactory()))return false;if(!factory&&FAILED(status=D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,&factory)))return false;if(!wic)CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_IWICImagingFactory,reinterpret_cast<void**>(&wic));return true;}
    bool bindDc(HDC dc,RECT physical){
        if(!ensureFactory())return false;
        // Software snapshots avoid a GPU-to-CPU readback while a game is using the GPU.
        if(!dcTarget){auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96,96);status=factory->CreateDCRenderTarget(&properties,&dcTarget);if(FAILED(status))return false;}
        selectTarget(dcTarget);return SUCCEEDED(status)&&SUCCEEDED(dcTarget->BindDC(dc,&physical));
    }
    bool bindWindow(HWND window){
        if(!ensureFactory())return false;RECT client{};GetClientRect(window,&client);auto size=D2D1::SizeU(std::max(1L,client.right),std::max(1L,client.bottom));
        if(!windowTarget){auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN,D2D1_ALPHA_MODE_IGNORE),96,96);status=factory->CreateHwndRenderTarget(properties,D2D1::HwndRenderTargetProperties(window,size,D2D1_PRESENT_OPTIONS_IMMEDIATELY),&windowTarget);if(FAILED(status))return false;}
        auto current=windowTarget->GetPixelSize();if((current.width!=size.width||current.height!=size.height)&&FAILED(status=windowTarget->Resize(size)))return false;
        selectTarget(windowTarget);drop(dcTarget);return SUCCEEDED(status);
    }
    void draw(HDC dc,RECT physical){if(bindDc(dc,physical))render();}
    bool drawWindow(HWND window){if(!bindWindow(window))return false;render();return SUCCEEDED(lastDraw);}
    ~Renderer(){trim();drop(wic);drop(factory);}
    void color(UINT32 hex,float alpha=1){brush->SetColor(D2D1::ColorF(hex,alpha));}
    void box(D2D1_RECT_F r,float radius,UINT32 fill,float alpha=1,UINT32 edge=0,float edgeAlpha=0){color(fill,alpha);target->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush);if(edgeAlpha>0){color(edge,edgeAlpha);target->DrawRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush,1);}}
    void line(float x,float y,float xx,float yy,UINT32 colorValue,float alpha=1){color(colorValue,alpha);target->DrawLine({x,y},{xx,yy},brush,1);}
    void comboFrame(int id,D2D1_RECT_F rect,UINT32 fill,float alpha,UINT32 edge,float edgeAlpha){
        float hover=switchPosition(id,hot==id||(keyboard&&focus==id));
        box(rect,9,fill,std::min(1.f,alpha+.1f*hover),mix(edge,0x2876db,hover),std::max(edgeAlpha,hover));
        if(pressed==id){color(0x2876db);target->DrawRoundedRectangle(D2D1::RoundedRect(rect,9,9),brush,1.5f);}
    }
    void label(const std::wstring&s,float x,float y,float width,float height,float size,UINT32 value,bool bold=false,bool latin=false,bool centered=false,float lineHeight=0,bool opticalCenter=false){
        LayoutKey key{s,width,height,size,bold,latin,centered,lineHeight,opticalCenter};auto found=layouts.find(key);
        if(found==layouts.end()){
            if(layouts.size()>=64)clearLayouts();
            std::wstring formatKey=std::to_wstring(size)+(bold?L"b":L"r")+(latin?L"i":L"n");auto&format=formats[formatKey];
            if(!format){write->CreateTextFormat(latin?L"Inter":L"Noto Sans SC",fonts,bold?DWRITE_FONT_WEIGHT_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-CN",&format);if(!format)return;format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);}
            Layout cached;if(FAILED(write->CreateTextLayout(s.c_str(),(UINT32)s.size(),format,width,height,&cached.text)))return;++layoutCreations;auto layout=cached.text;
            if(!latin){for(UINT32 at=0;at<s.size();){if(s[at]<0x21||s[at]>0x7e){++at;continue;}UINT32 start=at;while(at<s.size()&&s[at]>=0x21&&s[at]<=0x7e)++at;layout->SetFontFamilyName(L"Inter",DWRITE_TEXT_RANGE{start,at-start});}}
            if(centered){layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);}
            if(lineHeight>0)layout->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,lineHeight,size*.88f);
            if(opticalCenter){layout->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);cached.x=-6;DWRITE_OVERHANG_METRICS ink{};if(SUCCEEDED(layout->GetOverhangMetrics(&ink))){cached.x+=(ink.left-ink.right)/2;cached.y+=(ink.top-ink.bottom)/2;}}
            found=layouts.emplace(std::move(key),cached).first;
        }
        color(value);target->DrawTextLayout({x+found->second.x,y+found->second.y},found->second.text,brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    ID2D1Bitmap* image(const std::wstring&name){auto found=images.find(name);if(found!=images.end())return found->second;if(!wic)return nullptr;
        IWICBitmapDecoder*decoder=nullptr;IWICBitmapFrameDecode*frame=nullptr;IWICFormatConverter*converter=nullptr;ID2D1Bitmap*bitmap=nullptr;
        auto path=directory()+L"art\\"+name+L".png";
        if(SUCCEEDED(wic->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder))&&SUCCEEDED(decoder->GetFrame(0,&frame))&&SUCCEEDED(wic->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(frame,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))target->CreateBitmapFromWicBitmap(converter,nullptr,&bitmap);
        drop(converter);drop(frame);drop(decoder);images[name]=bitmap;return bitmap;
    }
    void art(const wchar_t*name,float x,float y,float width,float height,bool themed=true){auto bitmap=image((themed?(darkUi()?L"dark-":L"white-"):L"")+std::wstring(name));if(bitmap)target->DrawBitmap(bitmap,{x,y,x+width,y+height});}
    void chevron(int id,float x,float y){if(id==snapshotOwner)return;D2D1_MATRIX_3X2_F previous;target->GetTransform(&previous);target->SetTransform(D2D1::Matrix3x2F::Rotation(dropdownRotation(id),{x+7,y+7})*previous);art(L"chevron",x,y,14,14);target->SetTransform(previous);}
    void appIcon(HICON icon,const std::wstring&key,float x,float y){
        auto name=L"app-icon:"+key;auto found=images.find(name);ID2D1Bitmap*bitmap=found==images.end()?nullptr:found->second;
        if(found==images.end()&&wic&&icon){IWICBitmap*source=nullptr;IWICFormatConverter*converter=nullptr;
            if(SUCCEEDED(wic->CreateBitmapFromHICON(icon,&source))&&SUCCEEDED(wic->CreateFormatConverter(&converter))&&SUCCEEDED(converter->Initialize(source,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom)))target->CreateBitmapFromWicBitmap(converter,nullptr,&bitmap);
            drop(converter);drop(source);images[name]=bitmap;
        }if(bitmap)target->DrawBitmap(bitmap,{x,y,x+28,y+28});
    }
    void card(D2D1_RECT_F r,bool preview=false){auto t=theme();float radius=preview?20.f:16.f;
        // Small concentric alpha layers approximate the supplied soft shadow.
        // These are ordinary compositing, never desktop blur or backdrop capture.
        for(int i=12;i>0;--i){auto shadow=r;shadow.left-=i;shadow.right+=i;shadow.top+=6-i*.5f;shadow.bottom+=6+i*.5f;box(shadow,radius+i*.5f,darkUi()?0x000000:0x527da0,(darkUi()?.006f:.0018f)*(13-i)/12);}
        box(r,radius,t.card,preview?(darkUi()?.07f:.44f):t.cardAlpha,0xffffff,darkUi()?20.f/255:preview?.94f:1.f);
    }
    void button(int id,D2D1_RECT_F r,const wchar_t*text,const wchar_t*icon,bool primary=false){auto t=theme();bool over=hot==id,down=pressed==id;
        UINT32 fill=primary?(down?0x1c5bac:over?0x3282e8:0x2876db):0xffffff;
        box({r.left,r.top+1,r.right,r.bottom+1},9,0x000000,.05f);
        box(r,9,fill,primary?1.f:darkUi()?(over?.13f:16.f/255):over?1.f:.80f,primary?0x3b84e5:t.edge,darkUi()&&!primary?26.f/255:1.f);
        float iconX=r.left+16;art(icon,iconX,r.top+11.5f,15,15);label(text,iconX+23,r.top, r.right-iconX-31,r.bottom-r.top,13,primary?0xffffff:t.primary,false,false,true);
    }
    void render(){
        scale=dpi/96.f;originX=0;originY=0;if(IsWindowVisible(mainWindow)){RECT frame{};GetWindowRect(mainWindow,&frame);DwmGetWindowAttribute(mainWindow,9,&frame,sizeof(frame));POINT client{};ClientToScreen(mainWindow,&client);originX=(float)(frame.left-client.x);originY=52*scale+frame.top-client.y;}
        target->BeginDraw();target->SetTransform(D2D1::Matrix3x2F::Identity());target->Clear(D2D1::ColorF(theme().top));target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(originX,originY));target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
        auto t=theme();D2D1_GRADIENT_STOP stops[]={{0,D2D1::ColorF(t.top)},{.55f,D2D1::ColorF(t.middle)},{1,D2D1::ColorF(t.bottom)}};
        if(!canvasGradient||gradientDark!=darkUi()){drop(canvasGradient);ID2D1GradientStopCollection*collection=nullptr;if(SUCCEEDED(target->CreateGradientStopCollection(stops,3,&collection)))target->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties({0,0},{0,848}),collection,&canvasGradient);drop(collection);gradientDark=darkUi();}
        if(canvasGradient)target->FillRectangle({0,0,1280,848},canvasGradient);
        box({0,0,216,804},0,0xffffff,darkUi()?.015f:.19f);line(216,0,216,804,0xffffff,darkUi()?.06f:.7f);
        label(tr(L"工作空间",L"Workspace"),16,32,184,14,10,t.tertiary);
        const wchar_t*nav[]={tr(L"应用玻璃",L"App glass"),tr(L"系统玻璃",L"System glass"),tr(L"设置",L"Settings")};const wchar_t*icons[]={L"nav-app",L"nav-system",L"nav-settings"};
        for(int i=0;i<3;++i){float y=52.f+54*i;if(i==currentPage||hot==NavApp+i)box({16,y,200,y+46},11,darkUi()?0xffffff:0xffffff,i==currentPage?(darkUi()?.10f:.86f):darkUi()?.04f:.3f,0xffffff,i==currentPage?(darkUi()?.04f:1.f):0);if(i==currentPage)box({16,y+15,19,y+31},2,0x2876db);art(icons[i],30,y+14,18,18);label(nav[i],60,y+12,120,24,14,i==currentPage?0x2876db:t.secondary,i==currentPage);}
        box({16,currentPage==1&&systemView?666.f:671.f,200,780},12,0xffffff,darkUi()?.04f:.42f,0xffffff,darkUi()?.08f:.7f);art(L"layers",30,686,15,15);label(L"LGEngine",52,684,95,20,12,t.primary,true,true);color(opticalPreview.connected()?0x358c84:t.tertiary);target->FillEllipse(D2D1::Ellipse({132,693},2.5f,2.5f),brush);
        label(tr(L"独立材质引擎\n无需 Windhawk",L"Independent materials\nNo Windhawk required"),30,717,156,40,11,t.secondary,false,false,false,17.6f);label(L"v" LG_VERSION_W,30,761,156,14,10,t.tertiary,false,true);
        target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(originX,originY+(float)pageOffset));const wchar_t*titles[]={tr(L"应用玻璃",L"App glass"),tr(L"系统玻璃",L"System glass"),tr(L"设置",L"Settings")};label(titles[currentPage],248,32,1000,40,28,t.primary,true);label(currentPage==0?tr(L"为选定窗口赋予更轻盈的材质，让桌面保持专注。",L"Give selected windows a lighter material."):currentPage==1?tr(L"独立控制系统界面，按需调整每一种玻璃材质。",L"Control system effects and fine-tune each material."):tr(L"让 Liquid Glass 更符合你的使用习惯。",L"Make Liquid Glass feel at home on your desktop."),248,80,1000,22,13,t.secondary);if(currentPage==0){
        card({248,120,848,436});card({248,452,848,648});card({872,120,1248,772},true);
        label(tr(L"选择应用窗口",L"Choose an app window"),268,140,350,26,16,t.primary,true);
        size_t count=sample?2:entries.size();label(sample?tr(L"按进程名选择 · 示例 2 个窗口",L"Select by process · 2 sample windows"):tr(L"按进程名选择 · 检测到 ",L"Select by process · Found ")+std::to_wstring(count)+tr(L" 个窗口",L" windows"),268,168,380,18,11,t.tertiary);
        button(RefreshButton,{721,140,828,178},tr(L"刷新窗口",L"Refresh windows"),L"refresh");
        int selected=sample?sampleSelection:(int)SendMessageW(list,LB_GETCURSEL,0,0);
        for(int row=0;row<2;++row){int index=firstRow+row;float y=194.f+row*86;bool chosen=index==selected;if(index>=(int)count)continue;
            box({268,y,828,y+78},11,chosen?t.selected:0xffffff,chosen?1.f:darkUi()?.045f:.5f,chosen?t.selectedEdge:t.edge,chosen?1.f:darkUi()?.1f:1.f);
            box({284,y+21,320,y+57},9,chosen?0xffffff:darkUi()?0xffffff:0xebf2f8,darkUi()?.10f:1.f);
            if(sample)art(index==0?L"keyboard":L"globe",291.5f,y+28.5f,21,21);
            else if(entries[index].icon)appIcon(entries[index].icon,entries[index].exe,288,y+25);
            else art(L"globe",291.5f,y+28.5f,21,21);
            std::wstring process=sample?(index==0?L"TextInputHost.exe":L"chrome.exe"):entries[index].exe;
            std::wstring caption=sample?(index==0?tr(L"Windows 输入体验",L"Windows input experience"):L"Untitled – Figma - Google Chrome"):entries[index].title;
            label(process,334,y+22,447,18,13,t.primary,false,true);label(caption,334,y+40,447,16,11,t.secondary);
            color(chosen?0x2876db:darkUi()?0x64748b:0xbdcbd9);target->DrawEllipse(D2D1::Ellipse({803.5f,y+39},8.5f,8.5f),brush,chosen?5:1.5f);if(chosen){color(0xffffff);target->FillEllipse(D2D1::Ellipse({803.5f,y+39},3,3),brush);}
        }
        if(!count)label(tr(L"暂无可用窗口，请点击“刷新窗口”。",L"No windows available. Click Refresh windows."),268,250,560,48,13,t.secondary,false,false,true);
        std::wstring process=sample?(selected==0?L"TextInputHost.exe":L"chrome.exe"):selected>=0&&selected<(int)entries.size()?entries[selected].exe:tr(L"未选择窗口",L"No window selected");
        art(L"pointer",268,381,13,13);label(selected>=0?tr(L"已选择 ",L"Selected: ")+process+tr(L"，应用后可随时还原。",L". You can restore it after applying."):tr(L"选择窗口后可应用材质。",L"Select a window to apply a material."),288,379,535,20,11,t.secondary);
        label(tr(L"窗口材质",L"Window material"),268,472,560,25,15,t.primary,true);label(tr(L"材质类型",L"Material type"),268,504,272,18,11,t.secondary);label(tr(L"窗口边角",L"Window corners"),556,504,272,18,11,t.secondary);
        auto combo=[&](int id,D2D1_RECT_F r,HWND source){comboFrame(id,r,0xffffff,darkUi()?16.f/255:217.f/255,t.edge,darkUi()?26.f/255:1);const wchar_t*materialNames[]={L"Acrylic",L"Mica",L"Mica Alt",tr(L"关闭背景材质",L"No backdrop")};const wchar_t*cornerNames[]={tr(L"圆角",L"Rounded"),tr(L"小圆角",L"Small rounded"),tr(L"直角",L"Square")};int index=std::max(0,(int)SendMessageW(source,CB_GETCURSEL,0,0));label(id==MaterialBox?materialNames[std::min(index,3)]:cornerNames[std::min(index,2)],r.left+30,r.top,r.right-r.left-60,r.bottom-r.top,13,t.primary,false,id==MaterialBox,false,0,true);chevron(id,r.right-27,r.top+(r.bottom-r.top-14)/2);};
        combo(MaterialBox,{268,523,540,561},material);combo(CornerBox,{556,523,828,561},corners);
        label(process,268,588,325,20,11,t.tertiary,false,true);button(RestoreButton,{632,575,713,613},tr(L"还原",L"Restore"),L"restore");button(ApplyButton,{721,575,828,613},tr(L"应用材质",L"Apply material"),L"check",true);
        box({248,664,848,772},16,darkUi()?0x1e3a8a:0xe8f1fb,darkUi()?.65f:.6f,darkUi()?0x3b82f6:0xd1e3f6,darkUi()?.45f:1);art(L"info",266,682,17,17);label(tr(L"关于应用兼容性",L"App compatibility"),295,681,535,22,12,t.primary,true);
        label(tr(L"其他应用仅尝试外框材质。Chrome / Electron 等应用的内容区域通常不受影响，可能仍保持不透明。",L"Other apps support frame materials only. Chrome and Electron content may remain opaque."),295,706,535,50,12,t.secondary,false,false,false,21);
        }else{drawWorkspace(*this);}if(currentPage!=2){label(tr(L"材质预览",L"Material preview"),892,140,230,24,15,t.primary,true);label(L"LIGHT · DEPTH · GLASS",892,166,260,18,10,t.tertiary,false,true);
        box({1180,146,1228,166},6,darkUi()?0x1e3a8a:0xe3edfc);label(tr(L"仅预览",L"Preview only"),1180,146,48,20,10,0x2876db,false,false,true);
        if(currentPage==1&&systemView==2){auto bitmap=image(darkUi()?L"dark-system-study":L"white-system-study");if(bitmap){auto size=bitmap->GetSize();D2D1_RECT_F source{0,0,size.width,std::min(70.f,size.height)};target->DrawBitmap(bitmap,{892,192,1228,534},1,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,&source);}}else art(currentPage==1?L"system-study":L"study",892,192,336,342);if(currentPage==1&&systemView==2){box({920,297,1200,429},16,0xe5f5ff,.9f,0xffffff,.9f);label(tr(L"音量",L"Volume"),940,315,180,24,12,0x20364b,true);label(tr(L"仅示意",L"Illustration"),1120,315,60,24,10,0x61798f);box({940,365,1180,369},2,0xb4c4d3);box({940,365,1108,369},2,0x2876db);color(0xffffff);target->FillEllipse(D2D1::Ellipse({1108,367},7,7),brush);}const wchar_t*quality[]={tr(L"柔光漫射",L"Soft diffusion"),tr(L"边缘高光",L"Edge highlights"),tr(L"层次景深",L"Layered depth")};for(int i=0;i<3;++i){float x=892+i*114.6667f;label(quality[i],x,552,106.6667f,32,11,t.secondary,false,false,true);}
        line(892,602,1228,602,darkUi()?0xffffff:0xd7e4f0,darkUi()?.10f:1);art(L"scan",892,621,16,16);label(tr(L"在这里感受玻璃材质",L"A study in light and glass"),915,620,313,24,12,t.primary,true);
        label(tr(L"此区域仅展示材质与光学效果，不代表第三方应用的内容区域可以透明。实际效果取决于目标窗口。",L"This area previews materials and optics. Actual transparency depends on the target window."),892,654,336,78,12,t.secondary,false,false,false,22.2f);
        }target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(originX,originY));box({0,804,1280,848},0,darkUi()?0x0b1220:0xf8fbff,.5f);line(0,804,1280,804,0xffffff,darkUi()?.06f:.8f);
        bool connected=opticalPreview.connected();color(connected?0x358c84:t.tertiary);target->FillEllipse(D2D1::Ellipse({27,826},3,3),brush);
        label(connected?tr(L"LGEngine 预览已连接",L"LGEngine preview connected"):tr(L"LGEngine 预览未连接",L"LGEngine preview disconnected"),39,817,160,22,11,t.secondary);line(208,820,208,832,darkUi()?0xffffff:0xd7e4f0,darkUi()?.10f:1);label(actionResult?statusText:tr(L"光学参数仅影响预览",L"Optical settings affect preview only"),222,817,890,22,11,t.tertiary);
        art(L"tray",1144,819,14,14);label(tr(L"隐藏至托盘",L"Hide to tray"),1165,816,91,24,11,t.secondary);
        if(keyboard&&GetFocus()==mainWindow)for(auto&hit:hits())if(hit.id==focus){auto r=hit.r;r.left-=3;r.top-=3;r.right+=3;r.bottom+=3;color(0x2876db);target->DrawRoundedRectangle(D2D1::RoundedRect(r,11,11),brush,2);}
        HRESULT result=target->EndDraw();lastDraw=result;if(result==D2DERR_RECREATE_TARGET){for(auto&v:images)drop(v.second);images.clear();drop(canvasGradient);drop(brush);target=nullptr;drop(dcTarget);drop(windowTarget);status=result;}
    }
};
#include "workspace_pages.h"
inline Renderer&renderer(){static Renderer r;return r;}
void clearTextLayouts(){renderer().clearLayouts();}
void trimIdleUi(){
    if(uiAnimationTimers.count({mainWindow,4})){SetLayeredWindowAttributes(mainWindow,0,255,LWA_ALPHA);SetWindowLongPtrW(mainWindow,GWL_EXSTYLE,GetWindowLongPtrW(mainWindow,GWL_EXSTYLE)&~WS_EX_LAYERED);}
    finishPageMotion();themeMoving=false;stopUiAnimationTimer(mainWindow,12);stopUiAnimationTimer(mainWindow,4);stopUiAnimationTimer(mainWindow,73);stopUiAnimationTimer(mainWindow,76);
    for(auto&segment:segmentMotions)segment.moving=false;switchesMoving=false;switchMotions.clear();
    EnumChildWindows(mainWindow,[](HWND child,LPARAM)->BOOL{motionFinish(child);return TRUE;},0);renderer().trim();
}
bool paintPopupOwner(HDC dc,RECT physical);bool paintPopupOwnerWindow(HWND window);
void paint(HWND h,HDC supplied=nullptr){if(!supplied)supplied=static_cast<HDC>(GetPropW(h,L"LG.PrintDC"));if(supplied){RECT r{};GetClientRect(h,&r);if(!paintPopupOwner(supplied,r))renderer().draw(supplied,r);}else{PAINTSTRUCT paint{};HDC dc=BeginPaint(h,&paint);if(!paintPopupOwnerWindow(h)&&!renderer().drawWindow(h)){RECT r{};GetClientRect(h,&r);renderer().draw(dc,r);}EndPaint(h,&paint);}}
inline int hit(LPARAM l){float x=(static_cast<short>(LOWORD(l))-originX)/scale,y=(static_cast<short>(HIWORD(l))-originY)/scale;for(auto&item:hits()){float localY=y;if(currentPage!=0&&item.r.top>=180&&item.r.left<848)localY-=segmentContentOffset(currentPage==2);if(x>=item.r.left&&x<item.r.right&&localY>=item.r.top&&localY<item.r.bottom)return item.id;}return 0;}
inline void invalidate(){if(mainWindow)InvalidateRect(mainWindow,nullptr,FALSE);}
void sync(){
    if(!mainWindow||!tabs)return;
    if(active()){EnumChildWindows(mainWindow,[](HWND child,LPARAM)->BOOL{ShowWindow(child,SW_HIDE);return TRUE;},0);}
    else if(!legacy&&(!selfTestMode||appReviewMode)){ShowWindow(tabs,SW_HIDE);ShowWindow(status,SW_HIDE);ShowWindow(GetDlgItem(mainWindow,Pause),SW_HIDE);}else {ShowWindow(tabs,SW_SHOW);ShowWindow(status,SW_SHOW);ShowWindow(GetDlgItem(mainWindow,Pause),SW_SHOW);}
    invalidate();
}
struct MenuRow {std::wstring text;bool checked;};
using PopupClock=UiFrameClock;
inline double popupNow(){return uiNow();}
struct PopupState {std::vector<MenuRow>*rows;int hot=0,result=0,ownerId=0;bool done=false,closing=false,closed=false;double opened=0,closeStarted=0;int finalY=0;float closeFrom=1,highlightFrom=0,lastProgress=0;double highlightStarted=0;int width=0,height=0,openingFrames=0,closingFrames=0;double firstFrameDelay=0;int minAlpha=255,maxAlpha=0,minY=MAXLONG,maxY=MINLONG,presentationErrors=0;std::vector<uint32_t>optical,framePixels;UINT frameWidth=0,frameHeight=0;unsigned frameGeneration=0;ID2D1Bitmap*frame=nullptr;D2D1_MATRIX_3X2_F frameTransform{};float arrowX=0,arrowY=0;~PopupState(){drop(frame);}};
PopupState*activePopup=nullptr;
bool renderPopupOwner(RECT physical){
    auto&s=*activePopup;auto&r=renderer();
    if(s.frameWidth!=UINT(physical.right-physical.left)||s.frameHeight!=UINT(physical.bottom-physical.top))return false;
    if(s.frameGeneration!=r.targetGeneration){drop(s.frame);s.frameGeneration=r.targetGeneration;}
    if(!s.frame&&!s.framePixels.empty())r.target->CreateBitmap(D2D1::SizeU(s.frameWidth,s.frameHeight),s.framePixels.data(),s.frameWidth*4,D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE)),&s.frame);
    if(!s.frame)return false;
    r.target->BeginDraw();r.target->SetTransform(D2D1::Matrix3x2F::Identity());r.target->DrawBitmap(s.frame,{0,0,float(s.frameWidth),float(s.frameHeight)});
    r.target->SetTransform(s.frameTransform);r.chevron(s.ownerId,s.arrowX,s.arrowY);r.lastDraw=r.target->EndDraw();if(FAILED(r.lastDraw)){drop(s.frame);return false;}return true;
}
bool paintPopupOwner(HDC dc,RECT physical){return active()&&activePopup&&renderer().bindDc(dc,physical)&&renderPopupOwner(physical);}
bool paintPopupOwnerWindow(HWND window){RECT physical{};GetClientRect(window,&physical);return active()&&activePopup&&renderer().bindWindow(window)&&renderPopupOwner(physical);}inline float popupOpeningProgress(PopupState const&s,double now){if(!s.opened)return 0;float t=std::min(1.f,float((now-s.opened)/180));return 1-std::pow(1-t,3);}
inline float popupProgress(PopupState const&s,double now){if(!s.closing)return popupOpeningProgress(s,now);float t=std::min(1.f,float((now-s.closeStarted)/120));return s.closeFrom*std::pow(1-t,3);}
float dropdownRotation(int id){return activePopup&&activePopup->ownerId==id?180*(reducedMotion()?1.f:popupProgress(*activePopup,popupNow())):0;}
bool roundPopup(HWND window,PopupState const&s){
    auto region=CreateRoundRectRgn(0,0,s.width+1,s.height+1,MulDiv(20,dpi,96),MulDiv(20,dpi,96));
    if(!region)return false;
    if(!SetWindowRgn(window,region,TRUE)){DeleteObject(region);return false;}
    return true;
}
float popupHighlight(PopupState const&s){
    if(!s.highlightStarted||reducedMotion())return float(s.hot);
    float t=std::min(1.f,float((popupNow()-s.highlightStarted)/160));
    float ease=1-std::pow(1-t,3);
    return s.highlightFrom+(s.hot-s.highlightFrom)*ease;
}
void movePopupHighlight(HWND h,PopupState&s,int next){
    if(next==s.hot)return;
    s.highlightFrom=popupHighlight(s);s.hot=next;s.highlightStarted=popupNow();
    if(!reducedMotion()&&!startUiAnimationTimer(h,75))s.highlightStarted=0;
    InvalidateRect(h,nullptr,FALSE);
}
bool capturePopupBackdrop(HWND popup,PopupState&s){
    RECT root{},window{};GetClientRect(mainWindow,&root);GetWindowRect(popup,&window);POINT origin{};ClientToScreen(mainWindow,&origin);int w=root.right,h=root.bottom;
    if(w<=0||h<=0||w>4096||h>4096||s.width<=0||s.height<=0||s.width>2048||s.height>2048)return false;
    HDC dc=CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;uint32_t*pixels=nullptr;HBITMAP bitmap=dc?CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void**)&pixels,nullptr,0):nullptr;if(!bitmap){if(dc)DeleteDC(dc);return false;}
    auto old=SelectObject(dc,bitmap);memset(pixels,0,size_t(w)*h*4);renderer().lastDraw=E_FAIL;int previousOwner=snapshotOwner;snapshotOwner=s.ownerId;renderer().draw(dc,root);snapshotOwner=previousOwner;GdiFlush();if(FAILED(renderer().lastDraw)){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return false;}s.framePixels.assign(pixels,pixels+size_t(w)*h);s.frameWidth=w;s.frameHeight=h;s.frameGeneration=renderer().targetGeneration;drop(s.frame);renderer().target->CreateBitmap(D2D1::SizeU(w,h),pixels,w*4,D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE)),&s.frame);s.frameTransform=D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(originX,originY);for(auto const& hit:hits())if(hit.id==s.ownerId){s.arrowX=hit.r.right-27;s.arrowY=hit.r.top+(hit.r.bottom-hit.r.top-14)/2;if(currentPage!=0&&hit.r.top>=180&&hit.r.left<848)s.arrowY+=segmentContentOffset(currentPage==2);break;}std::vector<uint32_t> source(size_t(s.width)*s.height);int left=window.left-origin.x,top=s.finalY-origin.y;for(int y=0;y<s.height;++y)for(int x=0;x<s.width;++x)source[y*s.width+x]=pixels[std::clamp(top+y,0,h-1)*w+std::clamp(left+x,0,w-1)]|0xff000000;SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);
    liquidfx::Profile profile;profile.radius=10*dpi/96.f;profile.blur=4*dpi/96.f;profile.bend=7*dpi/96.f;profile.edge=18*dpi/96.f;profile.tint=darkUi()?.2f:.12f;profile.dark=darkUi();return liquidfx::render(source,s.width,s.height,profile,s.optical);
}
LRESULT CALLBACK popupProc(HWND h,UINT m,WPARAM w,LPARAM l){auto*s=(PopupState*)GetWindowLongPtrW(h,GWLP_USERDATA);if(m==WM_NCCREATE){s=(PopupState*)((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)s);}if(!s)return DefWindowProcW(h,m,w,l);int pad=MulDiv(6,dpi,96),row=MulDiv(36,dpi,96);
    if(m==WM_NCDESTROY){stopUiAnimationTimer(h,75);stopUiAnimationTimer(h,71);}
    if(m==WM_TIMER&&w==75){InvalidateRect(h,nullptr,FALSE);if(reducedMotion()||popupNow()-s->highlightStarted>=160){s->highlightStarted=0;stopUiAnimationTimer(h,75);}return 0;}if(m==WM_ERASEBKGND)return 1;if(m==WM_TIMER&&w==71){if(s->closing)++s->closingFrames;else if(++s->openingFrames==1)s->firstFrameDelay=popupNow()-s->opened;float progress=reducedMotion()?(s->closing?0.f:1.f):popupProgress(*s,popupNow());s->lastProgress=progress;if(!SetLayeredWindowAttributes(h,0,(BYTE)std::lround(255*progress),LWA_ALPHA))++s->presentationErrors;RECT rect{};GetWindowRect(h,&rect);SetWindowPos(h,nullptr,rect.left,s->finalY-(int)std::lround(MulDiv(8,dpi,96)*(1-progress)),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);BYTE alpha=255;DWORD flags=0;RECT actual{};if(GetLayeredWindowAttributes(h,nullptr,&alpha,&flags)&&GetWindowRect(h,&actual)){s->minAlpha=std::min(s->minAlpha,int(alpha));s->maxAlpha=std::max(s->maxAlpha,int(alpha));s->minY=std::min(s->minY,int(actual.top));s->maxY=std::max(s->maxY,int(actual.top));}else ++s->presentationErrors;invalidate();if(s->ownerId>=Combo)if(auto source=GetDlgItem(mainWindow,s->ownerId-Combo))InvalidateRect(source,nullptr,FALSE);if(s->closing?progress<=0:progress>=1){stopUiAnimationTimer(h,71);if(s->closing)s->closed=true;}return 0;}
    if(m==WM_PRINTCLIENT){printControlClient(h,(HDC)w);return 0;}if(m==WM_PAINT){ControlPaint frame(h);HDC dc=frame.dc;RECT r{};GetClientRect(h,&r);COLORREF bg=darkUi()?RGB(24,35,53):RGB(248,251,255);HBRUSH b=CreateSolidBrush(bg);FillRect(dc,&r,b);DeleteObject(b);if(s->optical.size()==size_t(s->width)*s->height&&!s->optical.empty()){BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=s->width;info.bmiHeader.biHeight=-s->height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;SetDIBitsToDevice(dc,0,0,s->width,s->height,0,0,0,s->height,s->optical.data(),&info,DIB_RGB_COLORS);}float highlight=popupHighlight(*s);RECT highlightRect{pad,pad+(int)std::lround(highlight*row),r.right-pad,pad+(int)std::lround(highlight*row)+row};roundFill(dc,highlightRect,darkUi()?RGB(36,58,91):RGB(229,240,255),darkUi()?RGB(36,58,91):RGB(229,240,255),MulDiv(8,dpi,96));for(int i=0;i<(int)s->rows->size();++i){RECT item{pad,pad+i*row,r.right-pad,pad+(i+1)*row};SetBkMode(dc,TRANSPARENT);SetTextColor(dc,i==s->hot?palette().accent:textColor());SelectObject(dc,font);RECT text=item;text.left+=MulDiv(14,dpi,96);text.right-=MulDiv(32,dpi,96);DrawTextW(dc,(*s->rows)[i].text.c_str(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);if((*s->rows)[i].checked){Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);COLORREF accent=palette().accent;Gdiplus::SolidBrush dot(Gdiplus::Color(255,GetRValue(accent),GetGValue(accent),GetBValue(accent)));float radius=3.f*dpi/96.f;g.FillEllipse(&dot,item.right-MulDiv(20,dpi,96)-radius,(item.top+item.bottom)/2.f-radius,2*radius,2*radius);} }{Gdiplus::Graphics g(dc);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);float inset=2.f*dpi/96.f,diameter=20.f*dpi/96.f;float right=r.right-2*inset,bottom=r.bottom-2*inset;Gdiplus::GraphicsPath path;path.AddArc(inset,inset,diameter,diameter,180,90);path.AddArc(right-diameter,inset,diameter,diameter,270,90);path.AddArc(right-diameter,bottom-diameter,diameter,diameter,0,90);path.AddArc(inset,bottom-diameter,diameter,diameter,90,90);path.CloseFigure();Gdiplus::Pen border(darkUi()?Gdiplus::Color(255,91,124,168):Gdiplus::Color(255,133,170,211),std::max(1.5f,1.5f*dpi/96.f));g.DrawPath(&border,&path);}return 0;}
    if(s->closing&&((m>=WM_MOUSEFIRST&&m<=WM_MOUSELAST)||(m>=WM_KEYFIRST&&m<=WM_KEYLAST)))return 0;
    if(m==WM_MOUSEMOVE){int y=(short)HIWORD(l);int next=std::clamp((y-pad)/row,0,(int)s->rows->size()-1);movePopupHighlight(h,*s,next);return 0;}
    if(m==WM_LBUTTONUP){POINT p{(short)LOWORD(l),(short)HIWORD(l)};RECT r{};GetClientRect(h,&r);if(PtInRect(&r,p)&&p.y>=pad&&p.y<pad+row*(int)s->rows->size()){int picked=(p.y-pad)/row;s->hot=picked;s->result=100+picked;s->done=true;}return 0;}
    if(m==WM_LBUTTONDOWN){POINT p{(short)LOWORD(l),(short)HIWORD(l)};RECT r{};GetClientRect(h,&r);if(!PtInRect(&r,p))s->done=true;return 0;}
    if(m==WM_KEYDOWN){if(w==VK_ESCAPE)s->done=true;else if(w==VK_RETURN||w==VK_SPACE){s->result=100+s->hot;s->done=true;}else if(w==VK_UP||w==VK_DOWN){movePopupHighlight(h,*s,(s->hot+(w==VK_UP?-1:1)+(int)s->rows->size())%(int)s->rows->size());}return 0;}
    if(m==WM_KILLFOCUS||m==WM_CANCELMODE||m==WM_CAPTURECHANGED)s->done=true;return DefWindowProcW(h,m,w,l);
}
int showPopup(std::vector<MenuRow>&rows,POINT at,int width,int ownerId=0){
    if(rows.empty()||width<=0)return 0;
    bool testPopup=selfTestMode&&!IsWindowVisible(mainWindow);
    static bool registered=false;
    if(!registered){WNDCLASSW wc{};wc.lpfnWndProc=popupProc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"LiquidGlass.ThemePopup";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);registered=RegisterClassW(&wc)!=0;}
    PopupState state{};state.rows=&rows;state.ownerId=ownerId;
    for(int i=0;i<(int)rows.size();++i)if(rows[i].checked)state.hot=i;
    int height=MulDiv(12+36*(int)rows.size(),dpi,96);
    MONITORINFO mi{};mi.cbSize=sizeof(mi);GetMonitorInfoW(MonitorFromPoint(at,MONITOR_DEFAULTTONEAREST),&mi);
    at.x=std::clamp(at.x,mi.rcWork.left,std::max(mi.rcWork.left,mi.rcWork.right-width));
    if(at.y+height>mi.rcWork.bottom)at.y=std::max(mi.rcWork.top,mi.rcWork.bottom-height);
    if(testPopup)at={-32000,-32000};
    HWND popup=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST|WS_EX_LAYERED,L"LiquidGlass.ThemePopup",L"",WS_POPUP,at.x,at.y,width,height,mainWindow,nullptr,GetModuleHandleW(nullptr),&state);
    if(!popup)return 0;
    state.width=width;state.height=height;state.finalY=at.y;
    capturePopupBackdrop(popup,state);roundPopup(popup,state);
    bool animate=!reducedMotion();
    activePopup=&state;
    SetLayeredWindowAttributes(popup,0,animate?0:255,LWA_ALPHA);
    if(animate)SetWindowPos(popup,nullptr,at.x,at.y-MulDiv(8,dpi,96),0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    HWND previous=GetFocus();ShowWindow(popup,testPopup?SW_SHOWNOACTIVATE:SW_SHOWNORMAL);UpdateWindow(popup);
    // Start after the first paint so font and bitmap work cannot consume the entrance.
    state.opened=popupNow();invalidate();
    bool scheduled=animate&&startUiAnimationTimer(popup,71);bool useClock=uiAnimationTimers.count({popup,71})!=0;
    if(!scheduled){
        animate=false;state.opened=popupNow()-180;state.lastProgress=1;
        SetLayeredWindowAttributes(popup,0,255,LWA_ALPHA);
        SetWindowPos(popup,nullptr,at.x,at.y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
    }
    if(!testPopup){SetFocus(popup);SetCapture(popup);}MSG msg{};
    while(!state.closed){
        if(state.done&&!state.closing){
            state.closeStarted=popupNow();state.closeFrom=state.lastProgress;state.closing=true;
            stopUiAnimationTimer(popup,71);stopUiAnimationTimer(popup,75);
            if(!animate)break;
            if(!startUiAnimationTimer(popup,71))break;
        }
        BOOL result=nextUiMessage(&msg);if(result<=0){if(!result)PostQuitMessage((int)msg.wParam);break;}
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(GetCapture()==popup)ReleaseCapture();activePopup=nullptr;DestroyWindow(popup);if(!selfTestMode&&(!IsWindowVisible(mainWindow)||IsIconic(mainWindow)))trimIdleUi();invalidate();
    auto log=directory()+L"dropdown-motion.log";HANDLE file=CreateFileW(log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,0,nullptr);
    if(file!=INVALID_HANDLE_VALUE){char line[360]{};int length=snprintf(line,sizeof(line),"control=%d animated=%d openFrames=%d closeFrames=%d firstFrameMs=%.1f alpha=%d..%d slidePx=%d errors=%d clock=%s result=%d\r\n",ownerId,animate?1:0,state.openingFrames,state.closingFrames,state.firstFrameDelay,state.minAlpha,state.maxAlpha,state.maxY>=state.minY?state.maxY-state.minY:0,state.presentationErrors,useClock?"shared":"window",state.result);DWORD written=0;WriteFile(file,line,length,&written,nullptr);CloseHandle(file);}
    if(IsWindow(previous)&&GetForegroundWindow()==mainWindow)SetFocus(previous);return state.result;
}
void showCombo(HWND source){
    int count=(int)SendMessageW(source,CB_GETCOUNT,0,0),selected=(int)SendMessageW(source,CB_GETCURSEL,0,0);if(count<=0)return;bool themeChoice=GetDlgCtrlID(source)==ColorTheme;if(themeChoice)count=2;if(GetDlgCtrlID(source)==SystemSurface)count=std::min(count,5);
    std::vector<MenuRow> rows;rows.reserve(count);
    for(int i=0;i<count;++i){int sourceIndex=themeChoice?(i==0?0:7):i;int length=(int)SendMessageW(source,CB_GETLBTEXTLEN,sourceIndex,0);std::wstring text(std::max(0,length)+1,L'\0');SendMessageW(source,CB_GETLBTEXT,sourceIndex,(LPARAM)text.data());text.resize(std::max(0,length));if(themeChoice)text=i==0?tr(L"冰蓝",L"Ice blue"):tr(L"深色",L"Dark");rows.push_back({text,sourceIndex==selected});}
    RECT r{};GetWindowRect(source,&r);if(active()){for(auto&item:hits())if(item.id==Combo+GetDlgCtrlID(source)){POINT at{(LONG)(originX+item.r.left*scale),(LONG)(originY+item.r.top*scale)};ClientToScreen(mainWindow,&at);r={at.x,at.y,at.x+(LONG)((item.r.right-item.r.left)*scale),at.y+(LONG)((item.r.bottom-item.r.top)*scale)};break;}}
    int picked=showPopup(rows,POINT{r.left,r.bottom},std::max((int)(r.right-r.left),MulDiv(220,dpi,96)),Combo+GetDlgCtrlID(source));
    if(picked>=100){SendMessageW(source,CB_SETCURSEL,themeChoice?(picked==100?0:7):picked-100,0);SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(GetDlgCtrlID(source),CBN_SELCHANGE),(LPARAM)source);}InvalidateRect(source,nullptr,FALSE);
}
void choose(HWND source,int controlId,int hitId){
    int count=(int)SendMessageW(source,CB_GETCOUNT,0,0),selected=(int)SendMessageW(source,CB_GETCURSEL,0,0);
    const wchar_t*materialNames[]={L"Acrylic",L"Mica",L"Mica Alt",tr(L"关闭背景材质",L"No backdrop")};const wchar_t*cornerNames[]={tr(L"圆角",L"Rounded"),tr(L"小圆角",L"Small rounded"),tr(L"直角",L"Square")};
    std::vector<MenuRow>rows;rows.reserve(count);for(int i=0;i<count;++i){const wchar_t*text=hitId==MaterialBox?materialNames[std::min(i,3)]:cornerNames[std::min(i,2)];rows.push_back({text,selected==i});}
    auto rect=hits()[hitId==MaterialBox?6:7].r;POINT at{LONG(originX+rect.left*scale),LONG(originY+rect.bottom*scale)};ClientToScreen(mainWindow,&at);
    int picked=showPopup(rows,at,(int)((rect.right-rect.left)*scale),hitId);
    if(picked>=100){SendMessageW(source,CB_SETCURSEL,picked-100,0);if(!appReviewMode)SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(controlId,CBN_SELCHANGE),(LPARAM)source);}invalidate();
}
void invoke(int id){
    if(id==GeneralTab||id==AboutTab){startSegment(true,settingsAbout?1:0,id==AboutTab?1:0);settingsAbout=id==AboutTab;invalidate();return;}
    if(id==CombinedNotification||id==CombinedIndicator){bool on=id==CombinedIndicator?volumeOn&&brightnessOn:shellOn[2]&&(glassExtraFeatures&glass::Calendar)!=0;int first=id==CombinedIndicator?VolumeOn:NotificationToggle,second=id==CombinedIndicator?BrightnessOn:CalendarToggle;for(int control:{first,second}){bool checked=control==VolumeOn?volumeOn:control==BrightnessOn?brightnessOn:control==NotificationToggle?shellOn[2]:(glassExtraFeatures&glass::Calendar)!=0;if(checked==on)SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(control,BN_CLICKED),(LPARAM)GetDlgItem(mainWindow,control));}invalidate();return;}
    if(id>=Slider&&id<Slider+1000)return;
    if(id>=Combo&&id<Combo+1000){showCombo(GetDlgItem(mainWindow,id-Combo));invalidate();return;}
    if(id>=Command&&id<Command+1000){int control=id-Command;if(HWND h=GetDlgItem(mainWindow,control);h&&!IsWindowEnabled(h)){statusText=tr(L"当前功能尚不可用，请检查引擎连接或恢复状态。",L"This feature is unavailable. Check the engine connection or restore status.");actionResult=true;invalidate();return;}if(control==OpticalPreview)SendMessageW(mainWindow,WM_COMMAND,SystemEngineTab,0);SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(control,BN_CLICKED),(LPARAM)GetDlgItem(mainWindow,control));invalidate();return;}
    if(id==NavSystem||id==NavSettings){SendMessageW(mainWindow,WM_APP+50,id==NavSystem?1:2,0);return;}
    if(id==NavApp){SendMessageW(mainWindow,WM_APP+50,0,0);return;}
    if(id==Row0||id==Row1){int index=firstRow+id-Row0;if(sample)sampleSelection=std::min(1,index);else if(index<(int)entries.size()){SendMessageW(list,LB_SETCURSEL,index,0);SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(List,LBN_SELCHANGE),(LPARAM)list);}invalidate();return;}
    if(id==MaterialBox){choose(material,Material,id);return;}if(id==CornerBox){choose(corners,Corners,id);return;}
    if(id==TrayButton){if(appReviewMode){statusText=tr(L"界面评审窗口不注册托盘；正式运行沿用原有托盘逻辑。",L"Review mode does not register a tray icon.");actionResult=true;invalidate();}else ShowWindow(mainWindow,SW_HIDE);return;}
    if(appReviewMode){if(id==RefreshButton)sample=true;if(id==ApplyButton||id==RestoreButton){statusText=tr(L"界面评审使用示例数据，未修改任何目标窗口。",L"Review uses sample data and leaves target windows unchanged.");actionResult=true;}invalidate();return;}
    int command=id==RefreshButton?Refresh:id==RestoreButton?Restore:id==ApplyButton?Apply:0;
    if(command){actionResult=id==ApplyButton||id==RestoreButton;auto previousLanguage=l10n::current;l10n::current=l10n::Simplified;SendMessageW(mainWindow,WM_COMMAND,MAKEWPARAM(command,BN_CLICKED),(LPARAM)GetDlgItem(mainWindow,command));l10n::current=previousLanguage;}invalidate();
}
bool key(WPARAM key){
    if(!active())return false;
    if(key==VK_TAB){keyboard=true;int index=0;for(size_t i=0;i<hits().size();++i)if(hits()[i].id==focus)index=(int)i;index=(index+(GetKeyState(VK_SHIFT)<0?-1:1)+(int)hits().size())%(int)hits().size();focus=hits()[index].id;invalidate();return true;}
    if(key==VK_RETURN||key==VK_SPACE){keyboard=true;invoke(focus);return true;}
    if((key==VK_UP||key==VK_DOWN)&&(focus==Row0||focus==Row1)){keyboard=true;focus=key==VK_UP?Row0:Row1;invoke(focus);return true;}
    if(key==VK_F5&&currentPage==0){invoke(RefreshButton);return true;}
    if(focus>=Slider&&focus<Slider+1000&&(key==VK_LEFT||key==VK_RIGHT||key==VK_HOME||key==VK_END)){HWND h=GetDlgItem(mainWindow,focus-Slider);int lo=(int)SendMessageW(h,TBM_GETRANGEMIN,0,0),hi=(int)SendMessageW(h,TBM_GETRANGEMAX,0,0),v=(int)SendMessageW(h,TBM_GETPOS,0,0);v=key==VK_HOME?lo:key==VK_END?hi:std::clamp(v+(key==VK_LEFT?-1:1),lo,hi);SendMessageW(h,TBM_SETPOS,TRUE,v);SendMessageW(mainWindow,WM_HSCROLL,TB_ENDTRACK,(LPARAM)h);invalidate();return true;}
    if((key==VK_DOWN&&GetKeyState(VK_MENU)<0)||key==VK_F4){if(focus==MaterialBox||focus==CornerBox||(focus>=Combo&&focus<Combo+1000)){invoke(focus);return true;}}
    return false;
}
void dragSlider(int id,LPARAM l,bool end){if(id<Slider||id>=Slider+1000)return;for(auto&item:hits())if(item.id==id){float x=((short)LOWORD(l)-originX)/scale;float ratio=std::clamp((x-item.r.left-9)/(item.r.right-item.r.left-18),0.f,1.f);HWND h=GetDlgItem(mainWindow,id-Slider);int lo=(int)SendMessageW(h,TBM_GETRANGEMIN,0,0),hi=(int)SendMessageW(h,TBM_GETRANGEMAX,0,0);SendMessageW(h,TBM_SETPOS,TRUE,lo+(int)std::lround(ratio*(hi-lo)));SendMessageW(mainWindow,WM_HSCROLL,end?TB_ENDTRACK:TB_THUMBTRACK,(LPARAM)h);invalidate();break;}}
bool mouse(UINT message,WPARAM w,LPARAM l){
    if(!active()&&hit(l)!=NavApp&&hit(l)!=NavSystem&&hit(l)!=NavSettings&&hit(l)!=TrayButton&&GetCapture()!=mainWindow)return false;
    if(message==WM_LBUTTONDOWN){keyboard=false;pressed=hit(l);focus=pressed?pressed:focus;SetFocus(mainWindow);SetCapture(mainWindow);dragSlider(pressed,l,false);invalidate();return true;}
    if(message==WM_LBUTTONUP){int id=hit(l),old=pressed;dragSlider(old,l,true);pressed=0;if(GetCapture()==mainWindow)ReleaseCapture();if(id&&id==old)invoke(id);invalidate();return true;}
    if(message==WM_MOUSEMOVE){if(GetCapture()==mainWindow)dragSlider(pressed,l,false);int id=hit(l);if(id!=hot){hot=id;invalidate();}TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,mainWindow,0};TrackMouseEvent(&track);SetCursor(LoadCursorW(nullptr,id?IDC_HAND:IDC_ARROW));return true;}
    if(message==WM_MOUSELEAVE){hot=0;if(GetCapture()!=mainWindow)pressed=0;invalidate();return true;}
    if(message==WM_CAPTURECHANGED||message==WM_CANCELMODE){pressed=0;if(message==WM_CANCELMODE&&GetCapture()==mainWindow)ReleaseCapture();invalidate();return true;}
    if(message==WM_MOUSEWHEEL){if(currentPage==0){int count=sample?2:(int)entries.size();firstRow=std::clamp(firstRow+(GET_WHEEL_DELTA_WPARAM(w)>0?-1:1),0,std::max(0,count-2));invalidate();}return true;}
    return false;
}
}
