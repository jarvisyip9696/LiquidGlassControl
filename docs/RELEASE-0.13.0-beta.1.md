# Liquid Glass Control 0.13.0-beta.1

## 中文

本次将 DK 的 LGEngine 更新整合到主程序，并提供包含配套宿主的完整便携包。

- 新增 **系统玻璃 → LGEngine** 分页，统一配色、按钮和下拉框，取消独立白色控制窗口。
- 卡片支持无、淡入、滑入、缩放、滑入并缩放、落下、弹出、融化、涟漪九种入场样式；指示器可单独选择入场样式。
- 新增音量／亮度玻璃 OSD：图标、轨道、填充、数值和圆形／胶囊／透镜旋钮，包含滑动、抬升、回落以及倾斜／弹跳／脉冲效果。
- 开启指示器后，切换分页或隐藏至托盘仍会响应变化；后台 OSD 显示在当前显示器底部。提供重播和不改变系统数值的示例。
- 三个预览玻璃按钮支持悬停发光。动画和指示器设置自动保存。
- 保留 DK 的 Dock 合成器动画优化、局部像素更新、原地尺寸／外观更新和断线恢复改进。
- 同步 DK 最新的 LGEngine 2.7.12.0 / 协议 1.1，固定宿主源码至 `dabe4cb`，不再使用早期本地宿主协议补丁。
- 补齐五语言翻译、完整便携打包、源码包及自动发布构建。

**下载完整的 win11-x64.zip 并全部解压**，双击其中的 `LiquidGlassControl.exe`。
ZIP 包含控制器、适配器、LGEngineHost、独立客户端示例、使用说明和对应源码。
单独 EXE 是控制器，不含 LGEngine 配套文件。无需 Windhawk；宿主使用 Windows 自带的 .NET Framework 4.x。

验证：主程序、适配器、独立双客户端及桥接验证；后台 OSD 显示／禁用／恢复检查；17 项宿主客户端检查。桥接 28/28、宿主客户端 17/17 检查通过，完整 ZIP 的干净目录自检通过。

边界：这仍是未签名的实验版。入场动画用于 LGEngine 卡片和指示器；不接管普通 Windows 通知，也不屏蔽系统原生音量提示。亮度事件取决于显示器和 Windows 支持。系统界面的完整 LGEngine 光学迁移尚未完成，真实系统材质、弹出菜单和恢复效果仍取决于 Windows 版本及系统策略。个人设置、日志和恢复记录不包含在下载中。

LGEngine 源码及 EXE 的分发授权由仓库所有者确认；上游声明和授权范围见包内 `LGEngine Distribution.txt` 及项目许可说明。

## English

This prerelease integrates DK's LGEngine updates into the controller and ships a complete portable runtime.

- A themed **System glass > LGEngine** page replaces the floating control window.
- Nine card arrival styles, independent indicator arrival, replay, slosh, ripple kicks, three knob styles and three hover-glow preview chips.
- Volume/brightness OSD with glyph, track, fill, animated knob and level number. It continues on other pages and in the tray, near the bottom of the active monitor. Settings persist automatically.
- DK's compositor-driven Dock animation, dirty bitmap regions, in-place updates and recovery improvements.
- Official LGEngine 2.7.12.0 / protocol 1.1 pinned to `dabe4cb`; the earlier local protocol patch is no longer applied.
- Complete portable packaging, corresponding controller/host source, five-language coverage and updated CI.

Download and fully extract the **win11-x64.zip**. It includes the controller, adapter, host, example and source. The separate EXE is controller-only. No Windhawk is required; the host uses Windows .NET Framework 4.x.

Unsigned experimental build. Arrival animations apply to LGEngine cards and indicators. Windows toasts and the native volume flyout are not replaced. Brightness reporting depends on hardware/Windows support; full optical migration of Windows shell surfaces is not implemented. No personal settings, logs or recovery journals are shipped. Preserve recovery files when upgrading an existing installation.

The owner confirmed DK's authorization to distribute LGEngine source and EXE; see the included distribution notice for scope.
