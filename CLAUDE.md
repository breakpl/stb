# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

wxWidgets and cmake are required. On this machine they are installed at `~/local` (wxWidgets built from source) and `~/Library/Python/3.9/bin/cmake` (via pip3). Adjust `CMAKE_PREFIX_PATH` if your wxWidgets is elsewhere.

```bash
export PATH="$HOME/Library/Python/3.9/bin:$HOME/local/bin:$PATH"

mkdir -p build-arm64 && cd build-arm64
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DwxWidgets_CONFIG_EXECUTABLE="$HOME/local/bin/wx-config" \
  -DwxWidgets_ROOT_DIR="$HOME/local"
cmake --build . -j$(sysctl -n hw.logicalcpu)
```

The post-build step automatically copies `SprintToolBox.ini` into the app bundle's `Resources/`.

To produce a signed DMG for distribution: `./package_mac.sh` (requires Homebrew at `/opt/homebrew`).

## Run tests

```bash
cd build-arm64
ctest --output-on-failure
# or a single suite:
./tests/test_timestamp
./tests/test_sprint
./tests/test_update
```

Tests use Catch2 (auto-fetched via FetchContent if not found on the system).

## Testing config changes

The app reads `~/SprintToolBox.ini` first; on first launch it copies the bundle's `SprintToolBox.ini` there. To pick up changes to the project's ini:

```bash
pkill -x SprintToolBox; rm ~/SprintToolBox.ini
open build-arm64/SprintToolBox.app
```

The running app polls `~/SprintToolBox.ini` every 10 seconds and hot-reloads it — no restart needed for routine edits.

## Architecture

**Entry point & tray lifecycle** — `SprintToolBoxApp` (`include/SprintToolBoxApp.h`, `src/SprintToolBoxApp.cpp`) is the central wxTaskBarIcon subclass. It owns all dialogs, drives the menu-build loop, handles every menu event, and runs three timers: sprint refresh, config-file watcher, and update check.

**Config** — `Config` is a singleton that parses `~/SprintToolBox.ini`. `DisplayFlags` controls which built-in menu items are visible. `MenuItem` represents URL links and submenus. `Config::SaveDisplayFlags` / `SaveMenuOrder` / `SaveSubmenuOrder` rewrite the INI in-place and call `Reload()`.

**Menu building** — `BuildPopupMenu()` in SprintToolBoxApp assembles the tray menu on each click: built-in items first (timestamps, converters), then dynamic items from config. Dynamic items get IDs from `ID_DYNAMIC_MENU_START` upward and are dispatched via `OnDynamicMenuClick`.

**Tool dialogs** — Each converter is a standalone `wxDialog` subclass that hides on close (never destroyed until app exit) so it can be re-shown cheaply. All follow the same pattern: two `wxTextCtrl` fields, a reentrancy guard `m_updating`, live-update via `wxEVT_TEXT`. Current dialogs: `ConverterDialog` (hex↔dec), `TimeConverterDialog`, `Base64Dialog`, `UrlEncoderDialog`.

**Adding a new built-in tool** — follow this checklist:
1. Add `show<Name>` to `DisplayFlags` in `Config.h` and wire it in `Config.cpp` (constructor default, INI key parser, `GetDisplayFlags`, `SaveDisplayFlags`).
2. Create `include/<Name>Dialog.h` + `src/<Name>Dialog.cpp` modelled on `ConverterDialog`.
3. Add source to `CMakeLists.txt` SOURCES list.
4. In `SprintToolBoxApp.h`: forward-declare, add member pointer, add event handler, add ID to the enum.
5. In `SprintToolBoxApp.cpp`: include header, add `EVT_MENU` entry, init member to `nullptr` in constructor, destroy in destructor, add menu item in `BuildPopupMenu`, implement handler.
6. In `CustomizeMenuDialog.h` / `.cpp`: add checkbox member, wire to `DisplayFlags`.

**Sprint display** — `JiraService::ParsePublicSprintJson` parses a GitHub-hosted JSON. `SprintInfo::GetDaysPassed` counts only business days. The tray icon text is rendered as a bitmap via `UpdateTrayIcon`.

**Auto-updater** — `UpdateService` checks the GitHub Releases API, compares semver with the compiled-in `APP_VERSION`, and downloads the matching platform asset. Network calls are synchronous and expected to run off the UI thread.
