# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

Requires Visual Studio 2022 (platform toolset v145), CMake 3.20+, and Ninja.

### VS Code (CMake Tools extension)

Open the project, select a configure preset from [CMakePresets.json](CMakePresets.json), and build:

- `Ninja Debug (x86)` / `Ninja Release (x86)` — 32-bit
- `Ninja Debug (x64)` / `Ninja Release (x64)` — 64-bit

### Command line

```pwsh
# Configure
cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake -B build/release-x64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_GENERATOR_PLATFORM=x64

# Build
cmake --build build/release
```

### Output

Binaries: `keycastow_x86.exe` / `keycastow_x64.exe` in `Debug\` or `Release\` (at project root).

Post-build steps: copy `*.ini` files to output directory, embed `keycastow.exe.manifest` for Common Controls v6 theming.

### Build matrix

| Config  | CRT   | Optimization        | Defines        |
|---------|-------|---------------------|----------------|
| Debug   | /MDd  | /Od /Ob0            | _DEBUG         |
| Release | /MT   | /O2 /Oi /GL /Gy     | NDEBUG         |

CRT mismatch with original vcxproj is intentional: Release uses static linking for portability, Debug uses dynamic for faster linking.

## Architecture

A portable Windows keystroke/mouse visualizer — single executable, no installer needed, depends only on Windows system DLLs (GDI+, Comctl32).

### Source layout

| File                               | Purpose                                                                                                                                                                                   |
| ---------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [src/keycast.cpp](src/keycast.cpp) | Application entry point (`WinMain`), main overlay window, settings dialog, GDI+ label rendering, settings load/save, drag positioning, click animation rendering, crash dump handler      |
| [src/keylog.cpp](src/keylog.cpp)   | Low-level keyboard hook (`LLKeyboardProc`) and mouse hook (`LLMouseProc`), key-to-symbol translation via `ToUnicodeEx`, modifier state tracking, mouse click merge/double-click detection |
| [include/timer.h](include/timer.h) | `CTimer` — thin wrapper around Windows `CreateTimerQueueTimer` with a callback function pointer (`OnTimedEvent`)                                                                          |

The project builds via [CMakeLists.txt](CMakeLists.txt). Source files are listed there; headers in `include/` are on the target's include path.

**Important**: [keycast.cpp](src/keycast.cpp) uses an unconventional include pattern — it `#include "keycast.h"` and `#include "keylog.h"` inline within the file body (line 107-108), not at the top. This is intentional to give the included code access to the global variables and helper functions defined above the include point. Do not refactor this to top-of-file includes without understanding the data flow.

### Key data flow

1. **Hooks**: `LLKeyboardProc` / `LLMouseProc` (in keylog.cpp) receive raw input events via `SetWindowsHookEx`
2. **Label creation**: hooks call `showText()` to push formatted text into the `keyLabels[]` ring buffer
3. **Rendering**: `CTimer showTimer` fires `startFade()` every 40ms, which updates label opacity/fade and calls `updateLayeredWindow()` to paint the GDI+ canvas
4. **Settings**: Dialog in `SettingsWndProc` reads/writes an INI file beside the exe via `WritePrivateProfileString`/`GetPrivateProfileInt`

### Global state

All app state is module-level globals shared between keycast.cpp and keylog.cpp (declared `extern` where needed):

- `keyLabels[MAXLABELS]` — ring buffer of displayed text labels, each with position, timing, fade state
- `labelSettings` / `labelCount` / `maximumLines` — label configuration and count
- `modifierState` (in keylog.cpp) — tracks current Ctrl/Alt/Shift/Win/AltGr down state
- `kbdhook` / `moshook` — hook handles; setting to NULL disables capturing
- `positioning` — flag for drag-to-reposition mode
- `mouseClickAnimation` / `clickAnimRadius` / `clickAnims[]` — click animation state
- `tcModifiers` / `tcKey` — global hotkey combo for toggle (default Alt+B)

### Windows

- **Main window** (`hMainWnd`): layered, transparent, topmost popup. Hosts the GDI+ overlay canvas. Covers the current monitor's work area.
- **Stamp window** (`hWndStamp`): separate layered window for the branding message shown via `DraggableWndProc` (supports double-click → settings).
- **Click animation windows**: one per active animation slot (up to `CLICK_ANIM_MAX`=5), each a tiny layered topmost popup.
- **Settings dialog**: modeless dialog created from `IDD_DLGSETTINGS` resource.

### Localization

The `I18N(section, key, default)` macro (in [locale_manager.h](include/locale_manager.h)) resolves UI strings at runtime. Language `.ini` files are UTF-8 with BOM, parsed by `LocaleManager`. The app auto-detects language via `GetUserDefaultUILanguage()` and falls back to `keycastow_en.ini`. Embedded resources (`IDR_INI_DEFAULT`, `IDR_INI_EN`, `IDR_INI_ZH`) are extracted to disk on first run if missing.

## Linting / Formatting

A `.clang-format` file is present (BasedOnStyle: LLVM, SortIncludes: false). There are no tests in this repository.
