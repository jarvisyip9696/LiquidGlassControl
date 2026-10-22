# Liquid Glass Control

轻量 Windows 11 玻璃效果工具 · Windows 11 x64

**[下载完整便携 ZIP](https://github.com/jarvisyip9696/LiquidGlassControl/releases/download/v0.13.1/LiquidGlassControl-0.13.1-win11-x64.zip) · [发布说明](https://github.com/jarvisyip9696/LiquidGlassControl/releases/tag/v0.13.1) · [全部版本](https://github.com/jarvisyip9696/LiquidGlassControl/releases)**

- 系统玻璃效果（含键盘布局切换面板、任务栏胶囊外观）和 Dock 悬停放大动画（任务栏图标最多放大约 1.26 倍；系统关闭动画效果或节电时自动暂停）。
- 可调模糊、着色和圆角，一键重置三个滑块，支持五种语言与设置保存。
- 无需 Windhawk；关闭至托盘，可还原并退出。
- 后台按需加载、图标缓存释放；[内存对比与节能说明](docs/POWER-MEMORY.md)。

当前版本：**0.13.1**。下载仅包含发布文件，不包含开发缓存或个人设置。

新增 **系统玻璃 → LGEngine** 分页：九种入场样式、独立指示器动画、三种旋钮、晃动和重播。音量／亮度 OSD 在其他分页和托盘中继续工作，设置自动保存。完整 ZIP 包含 LGEngine 2.7.16.0 宿主、适配器、示例及源码。请完整解压。普通 Windows 通知和原生音量提示不会被替换。

> 实验版，当前未签名。通知中心效果尚未确认；胶囊托盘、弹出菜单及完整恢复仍需实测。实际功耗、开启效果时的内存尚未测量；托盘关闭效果时的应用内存对比见优化说明。

<details>
<summary>English</summary>

**[Full portable ZIP](https://github.com/jarvisyip9696/LiquidGlassControl/releases/download/v0.13.1/LiquidGlassControl-0.13.1-win11-x64.zip) · [Release notes](https://github.com/jarvisyip9696/LiquidGlassControl/releases/tag/v0.13.1) · [All releases](https://github.com/jarvisyip9696/LiquidGlassControl/releases)**

Lightweight glass controls for Windows 11 x64, with adjustable blur, tint and corners, five languages, a capsule appearance built into the taskbar switch, and Dock hover magnification (taskbar icons scale up to about 1.26x via one compositor expression animation, paused when system animations are off or on battery saving). No Windhawk runtime required. Close to tray; restore and exit to revert effects. The LGEngine page adds arrival animations, independent indicator animation, knob styles, slosh and replay. Volume/brightness OSD remains active on other pages and in the tray. Extract the full ZIP for the bundled LGEngine 2.7.16.0 host, adapter, example and source. Windows toast notifications and the native volume flyout are not replaced.

Experimental, unsigned build. Notification styling is unconfirmed; capsule tray appearance, popups and complete restoration need live testing. Effects-off tray memory was sampled locally; active-effect memory and hardware energy savings have not been benchmarked.

</details>

<details>
<summary>开发文档 / Development</summary>

[参与开发](CONTRIBUTING.md) · [架构](docs/ARCHITECTURE.md) · [构建发布](docs/RELEASING.md) · [版本规则](docs/VERSIONING.md)

</details>

[GPL-3.0-only](LICENSE) · [许可范围与第三方声明](docs/LICENSING.md) · [签名政策](docs/CODE-SIGNING.md) · 贡献者：[DK](https://github.com/domk6966)
