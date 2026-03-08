# KeyCastOW

[English](README.md)

Windows 按键与鼠标操作可视化工具。在录屏或演示时将键盘输入和鼠标点击显示在屏幕上。

## 功能

- 单文件便携运行，无需安装，仅依赖 Windows 系统 DLL
- 键盘输入显示，支持修饰键组合（Ctrl、Alt、Shift、Win）
- 鼠标点击可视化，可选点击动画（左键/右键/中键/滚轮/侧键）
- 可自定义外观：字体、文字/背景/边框颜色、透明度、圆角、对齐方式
- 多显示器支持，自动检测当前活动显示器
- 可配置显示时序：按键延迟、停留时间、淡出时长
- 全局热键切换捕获开关（默认：Alt+B）
- 系统托盘集成，提供设置对话框
- 可拖拽标签定位
- 单实例运行

## 构建

需要 MSBuild（Visual Studio 构建工具）。

```pwsh
msbuild /p:Platform=Win32 /p:Configuration=Release
msbuild /p:Platform=x64 /p:Configuration=Release
```

## 使用方法

1. 运行 `keycastow_x64.exe`（32 位系统使用 `keycastow_x86.exe`）。
2. 按 `Alt+B` 切换按键捕获的开启/关闭。
3. 双击显示标签或右键托盘图标打开设置。

## 本地化

KeyCastOW 通过 `.ini` 文件加载界面字符串，根据系统语言自动选择对应文件（如中文加载 `keycastow_zh.ini`），默认回退到 `keycastow_en.ini`。

添加新语言：

1. 复制 `resources/keycastow_en.ini` 为新文件（如 `keycastow_fr.ini`）。
2. 翻译文件中的值。
3. 提交 Pull Request。

## 许可证

[MIT License](LICENSE)
