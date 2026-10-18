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
    if(exitAfterRestore)DestroyWindow(mainWindow);
}
inline DWORD desiredGlassFeatures(){return (volumeOn?glass::Volume:0)|(brightnessOn?glass::Brightness:0)|(shellOn[0]?(glass::Taskbar|glass::Capsule):0)|(shellOn[1]?glass::Start:0)|(shellOn[2]?glass::Notifications:0)|(explorerOn?glass::Explorer:0)|(dockOn?glass::Dock:0)|(keyboardOn?glass::Keyboard:0);}
void queueGlass(bool discover){
    glassConfig.features=desiredGlassFeatures();confirmedMaterials&=glassConfig.features;failedMaterials&=glassConfig.features;fallbackMaterials&=glassConfig.features;
    glassPendingDiscover=glassPendingDiscover||discover;updateNativeInfo();
    // Short bursts share one connection pass; disabling everything restores immediately.
    if(!glassConfig.features||!SetTimer(mainWindow,14,180,nullptr)){bool pending=glassPendingDiscover;applyGlass(pending);}
}
void applyGlass(bool discover){
    syncOpticalPreview();
    KillTimer(mainWindow,14);glassPendingDiscover=false;
    if(selfTestMode||migrationJob||migrationNeeded()){updateNativeInfo();return;}
    glassConfig.features=desiredGlassFeatures();
    if(glassConfig.features&&!ensureEngine()){updateNativeInfo();return;}
    fallbackMaterials&=glassConfig.features;confirmedMaterials&=glassConfig.features;failedMaterials=0;KillTimer(mainWindow,6);
    // A rediscovery re-enumerates shell windows, so earlier success reports may describe surfaces that no longer exist (restarted Explorer, closed flyouts). Clear them until the engine reports again.
    if(discover)confirmedMaterials=fallbackMaterials=0;
    SYSTEM_POWER_STATUS power{};GetSystemPowerStatus(&power);glassConfig.economy=batteryFriendly&&(power.ACLineStatus==0||power.SystemStatusFlag==1);
    auto effective=glassConfig;HIGHCONTRASTW high{};high.cbSize=sizeof(high);if(SystemParametersInfoW(SPI_GETHIGHCONTRAST,sizeof(high),&high,0)&&(high.dwFlags&HCF_HIGHCONTRASTON))effective.features=0;
    if(!effective.tint){DWORD light=1,size=4;RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",L"SystemUsesLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&size);effective.tint=light?1:2;}
    BOOL systemMotion=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&systemMotion,0);if(!systemMotion)effective.features&=~glass::Dock;
    pausedFeatures=glassConfig.features&~effective.features;if(effective.economy)pausedFeatures|=glassConfig.features&glass::Dock;confirmedMaterials&=~pausedFeatures;
    if(engineReady){engine.publish(effective);if(discover)engine.discover();else engine.broadcast();}
    if(engineReady&&effective.features)SetTimer(mainWindow,20,1000,nullptr);else KillTimer(mainWindow,20);
    observeWindows();
    WritePrivateProfileStringW(L"Glass",L"Features",std::to_wstring(glassConfig.features).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Opacity",std::to_wstring(glassConfig.opacity).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Blur",std::to_wstring(glassConfig.blur).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Radius",std::to_wstring(glassConfig.radius).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"Tint",std::to_wstring(glassConfig.tint).c_str(),iniPath.c_str());
    WritePrivateProfileStringW(L"Glass",L"BatteryFriendly",batteryFriendly?L"1":L"0",iniPath.c_str());
    updateNativeInfo();updateEffectLabels();
    if(glassConfig.features&~pausedFeatures){if(glassConfig.features&~pausedFeatures&~confirmedMaterials)SetTimer(mainWindow,6,3500,nullptr);message(tr(L"正在连接系统材质；各项会显示连接结果。请打开对应区域。",L"Connecting materials; each item shows its result. Open the selected surface."));}
    else if(pausedFeatures)message(tr(L"效果已按省电或系统辅助功能设置暂停。",L"Effects paused by battery saving or system accessibility settings."));
    else message(tr(L"独立材质效果已关闭。",L"Standalone materials turned off."));
}

void resetGlassDefaults(){
    glass::Config defaults;glassConfig.opacity=defaults.opacity;glassConfig.blur=defaults.blur;glassConfig.radius=defaults.radius;
    SendMessageW(GetDlgItem(mainWindow,OpacitySlider),TBM_SETPOS,TRUE,glassConfig.opacity);SendMessageW(GetDlgItem(mainWindow,BlurSlider),TBM_SETPOS,TRUE,glassConfig.blur);SendMessageW(GetDlgItem(mainWindow,RadiusSlider),TBM_SETPOS,TRUE,glassConfig.radius);

    for(auto entry:{std::pair<const wchar_t*,DWORD>{L"Opacity",glassConfig.opacity},{L"Blur",glassConfig.blur},{L"Radius",glassConfig.radius}})WritePrivateProfileStringW(L"Glass",entry.first,std::to_wstring(entry.second).c_str(),iniPath.c_str());
    applyGlass(false);updateEffectLabels();RedrawWindow(mainWindow,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN);
}
