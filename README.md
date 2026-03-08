# KeyCastOW

[中文说明](./README_zh.md)

A keystroke and mouse action visualizer for Windows. Shows your keystrokes and mouse clicks on screen while recording screencasts or giving presentations.

## Features

- Single portable executable, no installation required, depends only on Windows system DLLs
- Keyboard input display with modifier key combinations (Ctrl, Alt, Shift, Win)
- Mouse click visualization with optional click animations (left/right/middle/scroll/X buttons)
- Customizable appearance: font, text/background/border color, opacity, rounded corners, alignment
- Multi-monitor support with automatic monitor detection
- Configurable display timing: keystroke delay, linger time, fade duration
- Global hotkey to toggle capturing on/off (default: Alt+B)
- System tray integration with settings dialog
- Draggable label positioning
- Single-instance enforcement

## Build

Requires MSBuild (Visual Studio Build Tools).

```pwsh
msbuild /p:Platform=Win32 /p:Configuration=Release
msbuild /p:Platform=x64 /p:Configuration=Release
```

## Usage

1. Run `keycastow_x64.exe` (or `keycastow_x86.exe` for 32-bit systems).
2. Press `Alt+B` to toggle keystroke capturing on/off.
3. Double-click the display label or right-click the tray icon to open settings.

## Localization

KeyCastOW loads UI strings from `.ini` files. It automatically selects the language file based on the system locale (e.g., `keycastow_zh.ini` for Chinese), falling back to `keycastow_en.ini`.

To add a new language:

1. Copy `resources/keycastow_en.ini` to a new file (e.g., `keycastow_fr.ini`).
2. Translate the values in the new file.
3. Submit a Pull Request.

## License

[MIT License](LICENSE)
