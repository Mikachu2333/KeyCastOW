# KeyCastOW

A keystroke visualizer for Windows. Lets you easily display your keystrokes while recording screencasts.

- Small footprint (one 400kb executable file).
- Green and portable, only depends on Windows system DLLs.
- Plenty of settings for keystroke display.
- Hotkey to turn on/off.

## build

`msbuild /p:platform=win32 /p:Configuration=Release`
`msbuild /p:platform=x64 /p:Configuration=Release`

## License

MIT License

## Localization

KeyCastOW supports localization via `.ini` files.
The program will automatically load `keycastow_zh.ini` if the system language is Chinese, otherwise it defaults to `keycastow_en.ini`.

To contribute a new translation:

1. Copy `keycastow_en.ini` to a new file, e.g., `keycastow_fr.ini`.
2. Translate the values in the new file.
3. Submit a Pull Request with the new file.
