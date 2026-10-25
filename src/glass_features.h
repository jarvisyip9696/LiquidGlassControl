#pragma once
void updateEffectLabels(){
    syncOpticalPreview();
    std::wstring value=std::to_wstring(glassConfig.opacity)+L"%  ·  "+std::to_wstring(glassConfig.radius)+tr(L" 圆角",L" corner radius");
    SetWindowTextW(GetDlgItem(mainWindow,EffectValue),value.c_str());std::wstring blur=tr(L"模糊强度：",L"Blur strength: ")+std::to_wstring(glassConfig.blur);SetWindowTextW(GetDlgItem(mainWindow,BlurLabel),blur.c_str());
}
bool migrationNeeded(){return explorerBackupExists()||nativeBackupExists()||shellBackupExists();}
int restoreLegacy(const std::wstring&dir){
    auto ini=dir+L"settings.ini";
    WritePrivateProfileStringW(L"Native",L"VolumeOn",L"0",ini.c_str());WritePrivateProfileStringW(L"Native",L"BrightnessOn",L"0",ini.c_str());
    if(GetFileAttributesW((dir+L"native-osd-originals.bin").c_str())!=INVALID_FILE_ATTRIBUTES||GetFileAttributesW((dir+L"native-osd-switches.bin").c_str())!=INVALID_FILE_ATTRIBUTES){int code=full_themes::nativeOperation(0,false,dir,true);if(code)return code;}
    if(GetFileAttributesW((dir+L"explorer-background-originals.bin").c_str())!=INVALID_FILE_ATTRIBUTES){int code=explorer_background::restore(dir);if(code)return code;}
    for(int i=0;i<3;++i){int code=full_themes::operation(i,false,dir);if(code)return code;}
    return shell_materials::cleanupTaskbar(dir);
}
void migrateLegacy(){
    if(migrationJob||!migrationNeeded())return;
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);SHELLEXECUTEINFOW info{};info.cbSize=sizeof(info);info.fMask=SEE_MASK_NOCLOSEPROCESS;info.hwnd=mainWindow;info.lpVerb=L"runas";info.lpFile=exe;info.lpParameters=L"--legacy-restore-all";info.nShow=SW_HIDE;
    if(!ShellExecuteExW(&info)){message(tr(L"旧主题尚未还原。点击「还原」重试；恢复文件已保留。",L"Old theme not yet restored. Click Restore to retry; recovery files retained."));exitAfterRestore=false;return;}
    migrationJob=info.hProcess;updateNativeInfo();SetTimer(mainWindow,2,200,nullptr);
}
void pollMigration(){
    if(!migrationJob||WaitForSingleObject(migrationJob,0)!=WAIT_OBJECT_0)return;
    DWORD code=1;GetExitCodeProcess(migrationJob,&code);CloseHandle(migrationJob);migrationJob=nullptr;KillTimer(mainWindow,2);updateNativeInfo();
    if(code||migrationNeeded()){message(std::wstring(tr(L"旧主题恢复未完成，错误码：",L"Previous theme restore incomplete, code: "))+std::to_wstring(code));exitAfterRestore=false;return;}
    message(tr(L"旧版主题已还原，任务栏使用系统原有布局。",L"Previous theme restored. Taskbar uses the original system layout."));
    if(exitAfterRestore){DestroyWindow(mainWindow);return;}
    // Startup cleared the switches while the old theme was present: the saved ones come back now, and their glass starts without a restart.
    DWORD saved=glass::unifiedTaskbar((DWORD)GetPrivateProfileIntW(L"Glass",L"Features",0,iniPath.c_str()));
    keyboardOn=false;dockOn=(saved&glass::Dock)!=0;explorerOn=false;for(int i=0;i<3;++i)shellOn[i]=(saved&(glass::Taskbar<<i))!=0;
    updateNativeInfo();if(glassExtraFeatures||keyboardOn||dockOn||explorerOn||shellOn[0]||shellOn[1]||shellOn[2])applyGlass(true);
}
// Calendar has its own switch; Toasts has none yet. Both bits are carried from the ini so saving the other switches keeps them.
// A family stays linked to the shared opacity/blur/radius/tint until its own look is saved (or its [Glass.<Family>] section exists), so the existing sliders keep driving every surface.
bool glassLookLinked[glass::FamilyCount]={true,true,true,true,true};
// Times the dock was held back because Windows animation effects are off (shown in the paused message and the log).
unsigned dockMotionOffCount=0;
const wchar_t*const glassLookSections[glass::FamilyCount]={L"Glass.Taskbar",L"Glass.Start",L"Glass.Notifications",L"Glass.Calendar",L"Glass.Explorer"};
// Volume and Brightness drive the LGEngine indicator (Controls.osd); only the native probe still asks the XAML engine for them.
inline DWORD desiredGlassFeatures(){return glassExtraFeatures|((nativeProbeMode&&volumeOn)?glass::Volume:0)|((nativeProbeMode&&brightnessOn)?glass::Brightness:0)|(shellOn[0]?(glass::Taskbar|glass::Capsule):0)|(shellOn[1]?glass::Start:0)|(shellOn[2]?glass::Notifications:0)|(dockOn?glass::Dock:0);}
void queueGlass(bool discover){
    glassConfig.features=desiredGlassFeatures();confirmedMaterials&=glassConfig.features;failedMaterials&=glassConfig.features;fallbackMaterials&=glassConfig.features;
    glassPendingDiscover=glassPendingDiscover||discover;updateNativeInfo();
    // Short bursts share one connection pass; disabling everything restores immediately.
    if(!glassConfig.features||!SetTimer(mainWindow,14,180,nullptr)){bool pending=glassPendingDiscover;applyGlass(pending);}
}
void saveGlassSettings(){
    // While an old theme is being restored the switches are cleared in memory only: pollMigration reads the saved ones back.
    if(!migrationJob&&!migrationNeeded())WritePrivateProfileStringW(L"Glass",L"Features",std::to_wstring(desiredGlassFeatures()).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Opacity",std::to_wstring(glassConfig.opacity).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Blur",std::to_wstring(glassConfig.blur).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Radius",std::to_wstring(glassConfig.radius).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Tint",std::to_wstring(glassConfig.tint).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"BatteryFriendly",batteryFriendly?L"1":L"0",iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Calendar",(glassExtraFeatures&glass::Calendar)?L"1":L"0",iniPath.c_str());
}
// Per-family look: [Glass.<Family>] Opacity, Blur, Radius, Tint. Saving unlinks the family from the shared sliders.
void saveGlassLook(glass::Family family,const glass::Look&look){
    auto&l=glassConfig.looks[family];l=glass::normalize(look);glassLookLinked[family]=false;auto section=glassLookSections[family];
    WritePrivateProfileStringW(section,L"Opacity",std::to_wstring(l.opacity).c_str(),iniPath.c_str());WritePrivateProfileStringW(section,L"Blur",std::to_wstring(l.blur).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(section,L"Radius",std::to_wstring(l.radius).c_str(),iniPath.c_str());WritePrivateProfileStringW(section,L"Tint",std::to_wstring(l.tint).c_str(),iniPath.c_str());
}
// Dock look: [Glass.Dock] Scale (percent, 126), Spread (percent of icon width, 135), Speed (0..100, 50), Bounce (0..100, 30).
void saveDock(DWORD scale,DWORD spread,DWORD speed,DWORD bounce){
    glass::Config c=glassConfig;c.dockScale=scale;c.dockSpread=spread;c.dockSpeed=speed;c.dockBounce=bounce;c=glass::normalize(c);
    glassConfig.dockScale=c.dockScale;glassConfig.dockSpread=c.dockSpread;glassConfig.dockSpeed=c.dockSpeed;glassConfig.dockBounce=c.dockBounce;
    WritePrivateProfileStringW(L"Glass.Dock",L"Scale",std::to_wstring(c.dockScale).c_str(),iniPath.c_str());WritePrivateProfileStringW(L"Glass.Dock",L"Spread",std::to_wstring(c.dockSpread).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass.Dock",L"Speed",std::to_wstring(c.dockSpeed).c_str(),iniPath.c_str());WritePrivateProfileStringW(L"Glass.Dock",L"Bounce",std::to_wstring(c.dockBounce).c_str(),iniPath.c_str());
}
// Called once from the ini-load site after the shared [Glass] keys are read: per-family looks fall back to those shared keys, the dock to today's defaults, and the one-time Calendar migration runs.
void loadGlassSettings(){
    glassConfig.features&=~DWORD(glass::Explorer);explorerOn=explorerByApp=false;
    for(int f=0;f<glass::FamilyCount;++f){auto section=glassLookSections[f];auto read=[&](const wchar_t*key,DWORD shared){return (DWORD)GetPrivateProfileIntW(section,key,(int)shared,iniPath.c_str());};
        glassLookLinked[f]=true;for(auto key:{L"Opacity",L"Blur",L"Radius",L"Tint"})if((int)GetPrivateProfileIntW(section,key,-1,iniPath.c_str())>=0)glassLookLinked[f]=false; // UINT: cast, or a missing key (-1) reads as huge
        glass::Look l;l.opacity=read(L"Opacity",glassConfig.opacity);l.blur=read(L"Blur",glassConfig.blur);l.radius=read(L"Radius",glassConfig.radius);l.tint=read(L"Tint",glassConfig.tint);glassConfig.looks[f]=glass::normalize(l);}
    glass::Config d;glassConfig.dockScale=GetPrivateProfileIntW(L"Glass.Dock",L"Scale",d.dockScale,iniPath.c_str());glassConfig.dockSpread=GetPrivateProfileIntW(L"Glass.Dock",L"Spread",d.dockSpread,iniPath.c_str());
    glassConfig.dockSpeed=GetPrivateProfileIntW(L"Glass.Dock",L"Speed",d.dockSpeed,iniPath.c_str());glassConfig.dockBounce=GetPrivateProfileIntW(L"Glass.Dock",L"Bounce",d.dockBounce,iniPath.c_str());
    // Migration, once: the Notification center used to include the calendar flyout, so an existing Notifications switch also turns Calendar on. Toasts are never migrated.
    if((int)GetPrivateProfileIntW(L"Glass",L"Calendar",-1,iniPath.c_str())<0&&(glassConfig.features&glass::Notifications)){glassConfig.features|=glass::Calendar;WritePrivateProfileStringW(L"Glass",L"Calendar",L"1",iniPath.c_str());}
    glassConfig=glass::normalize(glassConfig);glassExtraFeatures=glassConfig.features&(glass::Calendar|glass::Toasts);
    unifiedSystemGlass=GetPrivateProfileIntW(L"Glass",L"Unified",selfTestMode?0:1,iniPath.c_str())!=0;if(unifiedSystemGlass){auto common=glassConfig.looks[glass::TaskbarLook];glassConfig.opacity=common.opacity;glassConfig.blur=common.blur;glassConfig.radius=common.radius;glassConfig.tint=common.tint;for(int f=0;f<glass::FamilyCount;++f){glassConfig.looks[f]=common;glassLookLinked[f]=true;}}
}
// Volume/Brightness switches now belong to the LGEngine indicator. One-time migration: the old XAML Volume/Brightness bits and IndicatorOn (default on) become VolumeOn/BrightnessOn.
// Pure decision: marker set = use the stored switches; otherwise IndicatorOn=0 starts both off (the old XAML bits alone never turn the lens on), anything else starts both on.
struct OsdSwitches{bool volume,brightness;};
inline OsdSwitches osdMigration(bool migrated,int indicatorOn,DWORD oldBits,bool storedVolume,bool storedBrightness){
    (void)oldBits;
    if(migrated)return {storedVolume,storedBrightness};
    bool on=indicatorOn!=0;return {on,on};
}
void loadOsdSettings(){
    DWORD old=(DWORD)GetPrivateProfileIntW(L"Glass",L"Features",0,iniPath.c_str())&(glass::Volume|glass::Brightness);glassConfig.features&=~(DWORD)(glass::Volume|glass::Brightness);
    bool migrated=GetPrivateProfileIntW(L"LGEngine",L"OsdMigrated",0,iniPath.c_str())!=0;
    auto r=osdMigration(migrated,GetPrivateProfileIntW(L"LGEngine",L"IndicatorOn",1,iniPath.c_str()),old,GetPrivateProfileIntW(L"LGEngine",L"VolumeOn",0,iniPath.c_str())!=0,GetPrivateProfileIntW(L"LGEngine",L"BrightnessOn",0,iniPath.c_str())!=0);
    volumeOn=r.volume;brightnessOn=r.brightness;
    if(!migrated&&persistent()){WritePrivateProfileStringW(L"LGEngine",L"VolumeOn",volumeOn?L"1":L"0",iniPath.c_str());WritePrivateProfileStringW(L"LGEngine",L"BrightnessOn",brightnessOn?L"1":L"0",iniPath.c_str());WritePrivateProfileStringW(L"LGEngine",L"OsdMigrated",L"1",iniPath.c_str());}
}
bool glassPreviewOnly=false; // set around a slider drag's apply: the engine gets the config, the ini waits for savePanel on release
void applyGlass(bool discover){
    syncOpticalPreview();
    for(int f=0;f<glass::FamilyCount;++f)if(glassLookLinked[f]){auto&l=glassConfig.looks[f];l.opacity=glassConfig.opacity;l.blur=glassConfig.blur;l.radius=glassConfig.radius;l.tint=glassConfig.tint;}
    KillTimer(mainWindow,14);glassPendingDiscover=false;
    if(!selfTestMode&&!glassPreviewOnly)saveGlassSettings();
    if(selfTestMode||migrationJob||migrationNeeded()){updateNativeInfo();if(!selfTestMode&&desiredGlassFeatures())message(tr(L"旧版主题还原后，系统玻璃会继续。",L"System glass resumes after the old theme is restored."));return;}
    glassConfig.features=desiredGlassFeatures();
    if(glassConfig.features&&!ensureEngine()){KillTimer(mainWindow,6);failedMaterials|=glassConfig.features;confirmedMaterials&=~glassConfig.features;fallbackMaterials&=~glassConfig.features;updateNativeInfo();return;}
    fallbackMaterials&=glassConfig.features;confirmedMaterials&=glassConfig.features;failedMaterials=0;KillTimer(mainWindow,6);
    // A rediscovery re-enumerates shell windows, so earlier success reports may describe surfaces that no longer exist (restarted Explorer, closed flyouts). Clear them until the engine reports again.
    if(discover)confirmedMaterials=fallbackMaterials=0;
    SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);glassConfig.economy=batteryFriendly&&power.SystemStatusFlag==1; // battery saver only: on plain battery the flat fallback looked exactly like stock Windows
    auto effective=glassConfig;HIGHCONTRASTW high{};high.cbSize=sizeof(high);if(SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0)&&(high.dwFlags&HCF_HIGHCONTRASTON))effective.features=0;
    {DWORD light=1,size=4;RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);DWORD autoTint=light?1:2;if(!effective.tint)effective.tint=autoTint;for(auto&look:effective.looks)if(!look.tint)look.tint=autoTint;}
    // The dock keeps running on battery; only the system's "animation effects off" holds it back, and that is counted so it is visible rather than silent.
    BOOL systemMotion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&systemMotion,0);if(!systemMotion&&(effective.features&glass::Dock)){effective.features&=~glass::Dock;++dockMotionOffCount;}
    pausedFeatures=glassConfig.features&~effective.features;confirmedMaterials&=~pausedFeatures;
    if(engineReady){engine.publish(effective);if(discover)engine.discover();else engine.broadcast();}
    if(engineReady&&effective.features)SetTimer(mainWindow,20,2000,nullptr);else{KillTimer(mainWindow,20);diagText.clear();refreshStatus();}
    observeWindows();
    updateNativeInfo();updateEffectLabels();
    if(glassConfig.features&~pausedFeatures){if(glassConfig.features&~pausedFeatures&~confirmedMaterials)SetTimer(mainWindow,6,3500,nullptr);message(tr(L"正在连接系统材质；各项会显示连接结果。请打开对应区域。",L"Connecting materials; each item shows its result. Open the selected surface."));}
    else if(pausedFeatures)message(tr(L"效果已按省电或系统辅助功能设置暂停。",L"Effects paused by battery saving or system accessibility settings."));
    else message(tr(L"独立材质效果已关闭。",L"Standalone materials turned off."));
}

void resetGlassDefaults(){
    glass::Config defaults;glassConfig.opacity=defaults.opacity;glassConfig.blur=defaults.blur;glassConfig.radius=defaults.radius;
    // Per-family looks and the dock go back to the defaults too: drop their ini sections and relink the families to the shared values.
    for(int f=0;f<glass::FamilyCount;++f){WritePrivateProfileStringW(glassLookSections[f],nullptr,nullptr,iniPath.c_str());glassLookLinked[f]=true;glassConfig.looks[f]=glass::Look{defaults.opacity,defaults.radius,glassConfig.tint,defaults.blur};}
    WritePrivateProfileStringW(L"Glass.Dock",nullptr,nullptr,iniPath.c_str());glassConfig.dockScale=defaults.dockScale;glassConfig.dockSpread=defaults.dockSpread;glassConfig.dockSpeed=defaults.dockSpeed;glassConfig.dockBounce=defaults.dockBounce;
    SendMessageW(GetDlgItem(mainWindow,OpacitySlider),TBM_SETPOS,TRUE,glassConfig.opacity);SendMessageW(GetDlgItem(mainWindow,BlurSlider),TBM_SETPOS,TRUE,glassConfig.blur);SendMessageW(GetDlgItem(mainWindow,RadiusSlider),TBM_SETPOS,TRUE,glassConfig.radius);

    for(auto entry:{std::pair<const wchar_t*,DWORD>{L"Opacity",glassConfig.opacity},{L"Blur",glassConfig.blur},{L"Radius",glassConfig.radius}})WritePrivateProfileStringW(L"Glass",entry.first,std::to_wstring(entry.second).c_str(),iniPath.c_str());
    applyGlass(false);updateEffectLabels();RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}

bool allSystemGlassOn(){return volumeOn&&brightnessOn&&shellOn[0]&&shellOn[1]&&shellOn[2]&&dockOn&&(glassExtraFeatures&glass::Calendar);}
void saveUnifiedLook(const glass::Look& value){auto look=glass::normalize(value);glassConfig.opacity=look.opacity;glassConfig.blur=look.blur;glassConfig.radius=look.radius;glassConfig.tint=look.tint;for(int f=0;f<glass::FamilyCount;++f){saveGlassLook((glass::Family)f,look);glassLookLinked[f]=true;}WritePrivateProfileStringW(L"Glass",L"Unified",L"1",iniPath.c_str());saveGlassSettings();}
void toggleUnifiedSystemGlass(){bool on=!allSystemGlassOn();for(bool& state:shellOn)state=on;explorerOn=false;dockOn=on;keyboardOn=false;explorerByApp=false;if(on)glassExtraFeatures|=glass::Calendar;else glassExtraFeatures&=~(DWORD)(glass::Calendar|glass::Toasts);volumeOn=brightnessOn=on;osdSwitched(on);queueGlass(true);updateNativeInfo();}