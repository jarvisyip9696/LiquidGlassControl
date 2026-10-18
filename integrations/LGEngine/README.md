# LGEngine 评估与本地修复

曾试验任务栏烘焙高光，但因重复胶囊与尺寸问题已停用；清理时移除了这条未发布路径。独立 LGEngine 光学预览继续保留。

评估日期：2026-10-17。上游：https://github.com/domk6966/LGEngine

评估版本：`b6e93053b3eb001b310273b1ece1e58ec448106d`。
下载并修改的工作副本位于 `build/LGEngine-upstream`（不进入本项目发行包）。

## 第一阶段适配

主程序的 **设置 → 玻璃效果 → LGEngine 光学预览** 会启动独立 C# 适配器，使用真实的 `GlassService` / `GlassSurface` API 绘制光学玻璃。

- 着色、模糊、圆角、浅色/深色、语言和省电选项通过有版本与长度检查的 `WM_COPYDATA` 同步。模糊 0–40 映射为 LGEngine 的 0–100。
- 预览内可调折射、边缘光、悬浮高度、截面预设，并重播动画；这些设置目前只影响预览。
- 浮层属于预览窗口并取消全局置顶。移动同步位置，最小化与高对比度隐藏光学层，减少动画设置关闭动画和悬停。
- 控制器校验回执窗口属于实际子进程；未变化的参数不重复发送，启动等待最多 15 秒，之后停止轮询并报告失败。
- “还原”和退出关闭预览。子进程在带 `KILL_ON_JOB_CLOSE` 的 Job 中启动，控制器意外退出也会清理子进程。
- **系统界面仍由原引擎负责，此阶段尚未完成系统界面的光学后端替换。**

本地构建（已有打过补丁的工作副本）：

```powershell
./integrations/LGEngine/build-adapter.ps1
./build.ps1 -BuildTests -OutputDirectory build/candidates/lgengine
```

也可运行 `./integrations/LGEngine/build-candidate.ps1 -Validate` 一次构建并验证。脚本锁定评估提交，检测并应用补丁，不覆盖冲突的本地修改。构建后的 `validate.ps1` 为每次验证保存 `validation.json`，记录真实退出码与产物 SHA256；不读取前一次运行遗留的报告。原生恢复测试使用临时 HKCU 注册表夹具，需要正常用户权限，受限沙箱可能拒绝该夹具。

打开 `build/candidates/lgengine/LiquidGlassControl.exe`，在设置中打开光学预览。
若主程序已运行，请先从托盘“还原并退出”，否则单实例逻辑会打开原程序。
候选版使用同目录的设置，没有复制个人设置或恢复文件。
也可运行 `build/candidates/lgengine/lgengine/LiquidGlass.LGPreview.exe --standalone` 单独预览。

适配器单独引用 `lgengine/LGEngine.dll`，没有向 Explorer 加载托管代码。
本地构建脚本不下载上游、不修改安全策略、不进入默认发布包，原单 EXE 构建方式保持可用。
补丁新增 `SetAppearance(-1/0/1)`（自适应/深色/浅色，仅进程内），正确衔接主程序的色调选项；宿主协议未扩展。

## 评估结论

暂不替换 LiquidGlassControl 的系统界面引擎。LGEngine 值得作为未来自绘浮层的候选后端，但不是当前引擎的直接替代品。

| 项目 | LiquidGlassControl | LGEngine |
| --- | --- | --- |
| 集成方式 | C++ DLL、XAML diagnostics、系统原有界面属性恢复 | C#/.NET Framework、独立 layered window，可选管道宿主 |
| 玻璃实现 | Windows Composition 模糊与着色 | CPU 烘焙光线追踪、折射、色散、边缘光和动画 |
| 系统界面内容 | 保留系统任务栏、菜单及其原有内容 | 调用方需提供像素内容、位置和生命周期；本仓库没有系统界面接入实现 |
| GPU 状态 | Composition 效果图 | `gpu/` 为暂停的原型，未接入正式服务 |
| 发布条件 | GPL-3.0-only 与第三方声明 | README 写明 License 尚未选定，未找到许可证文件 |

依据：上游 `README.md`、`docs/CLIENTS.md`、`gpu/PROGRESS.md`，以及本项目 `src/glass_controller.h`、`src/composition_glass.h`、`docs/ARCHITECTURE.md`。
尚未完成双方同场景画质、CPU、内存和功耗对比，不能据此宣称 LGEngine 整体更快或更省电。

## 已修复的问题

`fixes.patch` 已应用于上述工作副本，包含三项修复及上述显式色调接口：

1. `GlassButton.Copy` 对 `Mask` 深拷贝，避免 `SetButtons` 发布后，调用方修改数组影响渲染线程的快照。
2. `GlassService.Connect` 的 HostPath 校验拒绝 `C:relative.exe` 和 `\relative.exe`。原先仅使用 `Path.IsPathRooted`，这些依赖当前目录/驱动器的路径也会通过“完整路径”检查。
3. `PipeTransport.StartDelayMs` 在加随机抖动后再次限制为 60000 ms。原实现先限基值再加抖动，实际最大接近 75 秒，与一分钟上限的文档承诺不符。

没有改动上游宿主协议、系统外观或安全策略。没有推送修改到上游。

## 验证记录

- 未修改版本：engine、host、bench 与 client 均编译成功；client 有既存的未赋值字段警告。
- 未修改版本：`LGBench -contract` 通过 **103/103** 检查（进程内渲染线程与 UI 线程）。这是离屏接口检查，不代表真实桌面上的画质或性能。
- 新增定向回归：未修改版本 **7/14** 通过，7 项失败分别复现上述三个问题。
- 修改后：engine、host、bench 重新编译成功；回归程序编译成功，但运行被 Windows 应用程序控制策略阻止。没有禁用或绕过策略，**修复后运行验证未完成**。
- 未修改版本管道测试：`LGBench -contract -loopback` 仅 **40/66** 通过，出现 host unavailable 和 TimeoutException。尚未区分环境权限、应用控制和程序缺陷，不能认定宿主模式已经验证通过。
- 补丁反向应用检查与忽略 CRLF 的 whitespace 检查通过。

适配阶段补充验证：主程序、原生自测程序、托管适配器和修改后的 LGEngine 均编译成功；99 条翻译、88 个界面键的覆盖检查通过。
适配器 `--self-test` 的参数边界与映射检查已实现，但 EXE 运行被 Windows 应用控制阻止，不能声称通过。
原生 `LiquidGlassTests --self-test` 运行返回 2，报告 GPU composition `0x80004002`，总体 FAIL；没有把这次结果记为通过，也尚未隔离其他检查项。
新增浅色/深色回归检查需要修改后的 LGEngine；所有新增运行检查、真实桌面画质、多显示器/DPI、窗口层级和父进程退出行为仍待允许执行的环境验证。

### 后续验证结果（2026-10-17，以上失败为此前的历史记录）

- 系统 CodeIntegrity 日志确认，早期测试程序、适配器及提取出的引擎 DLL 曾被签名策略拦截。未关闭、修改或绕过该策略。不同产物后续获准运行，不能将早期拦截误判为 GPU 不兼容。
- 修复回归已运行，**17/17 通过**，包括显式浅色/深色模式。
- 适配器 `--self-test` **通过，退出码 0**。
- 原生自测在正常用户权限下 **通过，退出码 0**：引擎加载、COM、Composition、配置连接、界面与恢复夹具均通过，Composition HRESULT 为 `S_OK`。受限沙箱下临时注册表恢复夹具失败；独立分组报告已能区分这个结果。
- 真实预览在 **192 DPI** 启动，收到 `surface: shown`，通过 Windows UI Automation 将折射从 80 改为 30，控件和标签同步变化。
- 以 Alt+F4 关闭预览，记录 `lifecycle: closing`，进程正常退出 0。尚未实测主程序 Job 在崩溃时清理子进程。
- 使用修改后的 LGEngine 离屏渲染成功，图像位于候选目录 `render-proof/card_1_grid.png`。这是引擎渲染结果，非桌面截图；现场截图受玻璃层捕获排除设置影响，尚不能凭截图确认视觉质量。
- 新增按实际 DPI 缩放控件、字体、玻璃像素和内容；跨显示器 DPI 改变时重建表面。192 DPI 启动已验证，跨显示器切换仍待实测。截面预设已补五语言显示，异常写入有大小上限的 `lgengine/preview.log`。
- 最新一次启动候选 `LiquidGlassControl.exe` 仍被应用控制拦截；因此**主程序入口到子进程的完整握手、同步和 Job 清理尚未完成实测**，不能将独立预览的通过等同于完整集成通过。

最新验证记录以候选目录中的 `validation.json` 和本次 `self-test.txt` 为准；验证脚本只运行自测，不启动系统效果。

### 桥接完整性与参数保存

- 桥接配置协议现为 **v2**。适配器返回已解析设置的完整校验值，原生端核对后才标记同步成功。主程序对启动和后续更新的超时做有界重试，不再把“收到窗口句柄”当成“设置已经生效”。v1 适配器不兼容，请一起构建 EXE 与适配器。
- `LGEngineBridgeTests.exe` 使用真实 `lgpreview::Controller` 和真实托管适配器，**20/20 通过**：启动握手、配置确认、语言变更、重复打开复用、错误发送窗口拒绝、无效设置拒绝与恢复、正常退出、绕过控制器析构的父进程退出均有检查。后者验证没有残留子进程，未单独区分 Job 与托管父进程监听各自的贡献。
- 主程序的“还原”会取消未完成的同步重试；正常关闭先给托管端一个有界清理机会，再释放 Job。
- 折射、边缘光、悬浮高度和截面预设会保存至适配器旁的 `optical-preview.ini`，后续打开恢复。只有值改变时才写入，通过同目录临时文件原子替换；损坏、缺失或越界配置使用默认值，并写日志。保存及新建/替换文件的回归已包含在适配器自测中，运行通过。
- 可运行 `./integrations/LGEngine/validate.ps1 -Integration` 检查原生、适配器和真实桥接。该选项会短暂打开测试预览窗口，不修改系统材质；输出 `bridge-test.txt` 及带文件 SHA256 的 `validation.json`。

独立桥接测试已覆盖实际进程通信和生命周期。任务栏、开始菜单等系统界面仍使用原引擎，没有将这些测试描述为系统界面光学迁移已完成。

### 实际主窗口检查

随后候选主程序获准启动，但首次立即退出，原因是后台旧版占用了单实例入口。通过程序自带 `--close-running` 恢复退出旧版后，已启动候选版（使用候选目录设置，没有迁移个人恢复文件）。

- 从主程序 **Settings → Glass → LGEngine optical preview** 打开真实托管预览成功；主程序显示 `LGEngine preview connected`，确认收到配置校验回执。
- 主程序通过原生键盘选择简体中文后，预览窗口标题同步为“LGEngine 光学玻璃预览”；随后恢复了测试前的英语设置。
- 从预览将折射 80 改为 30，关闭后 `optical-preview.ini` 保留了 30，其余值不变。检查结束已将这个候选目录的测试值恢复为 80。重开恢复逻辑的编解码与文件替换通过适配器自测；本轮没有完成重开后的人工读取，因为 UI 自动化后续报告 `coordinate input geometry is unavailable`。
- 最终原生自测、适配器自测及 17 项 LGEngine 修复回归均通过。20 项真实桥接检查曾通过；最后仅修订测试说明文字后的新测试 EXE 被应用控制拒绝启动，最新 `validation.json` 如实标记 bridge 为 `not_started`，没有冒称最终这一份二进制已运行通过。

主程序入口与语言同步已实测成功，不再将它们一概标记为“受策略阻挡未测”。跨显示器外观、系统界面光学迁移及发布签名仍未完成。

## 重现

在受信任且允许执行这些构建产物的 Windows 环境，针对全新副本：

```powershell
git clone https://github.com/domk6966/LGEngine.git build/LGEngine-upstream
git -C build/LGEngine-upstream checkout b6e93053b3eb001b310273b1ece1e58ec448106d
git -C build/LGEngine-upstream apply ../../integrations/LGEngine/fixes.patch
./integrations/LGEngine/test.ps1
./build/LGEngine-upstream/build.ps1
./build/LGEngine-upstream/build-client.ps1
./build/LGEngine-upstream/bin/LGBench.exe -contract
./build/LGEngine-upstream/bin/LGBench.exe -contract -loopback
```

当前工作副本已打补丁，不要再次应用。`test.ps1 -SourceDirectory <目录>` 可选择另一份源码。
补丁及测试用于本地评估，不代表已取得上游代码的再分发许可。

未来接入需先明确上游许可证，并验证管道宿主。若要替代系统界面材料，还需实现系统内容、位置、输入与无障碍接入，以及多显示器/DPI、置顶顺序、截图反馈和退出恢复测试；仅更换 DLL 文件无法完成这些能力。
