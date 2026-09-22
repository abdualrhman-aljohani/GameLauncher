# Changelog

All notable changes to GameLauncher. Format based on [Keep a Changelog](https://keepachangelog.com/).

## [1.8.0] — 2026-09

### Added
- **Import installed games** from Steam (all libraries), Epic Games, GOG Galaxy, Ubisoft Connect and common `Games` folders (`\Games`, `\Game`, `\GOG Games`, HoYoPlay / Genshin / Star Rail / Wuthering Waves folders). Already-added games are marked and skipped. Steam/Epic/Ubisoft games launch through their store.
- **Scan folder** button and smarter folder drops: a games library (e.g. `steamapps\common`) yields one entry per game subfolder instead of stopping after the first few files.
- **Drag & drop guidance**: full-window drop overlay and an empty-state card; a **welcome screen** (control panel + circle) on the very first run only.
- **Restore Boost defaults** now reports the result (restored / nothing to restore / failed — run as administrator).
- **Tooltips** for every icon and badge in the games list; the Quick Access bar only shows when a game has companion programs.
- **Backup** now includes play-session history (up to 1000 sessions per game).
- **Installer** (Inno Setup) with upgrade support and a cleanup step on uninstall (restores Boost changes, removes autostart).
- **Offline UI mode** via `tools/build-ui-assets.ps1`, experimental **Sharp mode** for scaled displays, dark title bar, new app icon.
- English is now the default language for **new** users (existing users keep their language).
- Unit tests for the JSON reader and the store-file parsers (`tests/`).

### Fixed
- Unicode: game names/paths in any script are saved and restored correctly (files are UTF-8; `games.cfg` writes are verified and keep a `.bak`).
- The remove button failed for names containing an apostrophe (and allowed script injection through a crafted name).
- Browsers, Epic Launcher and other desktop apps saved as "UWP" now launch (falls back to `shell:AppsFolder`; shortcuts with a plain AppUserModel ID are treated as normal apps).
- Failed launches no longer count as a play session.
- Boost: MMCSS restore defaults, failure handling, and Realtime priority removed (it could freeze the system).
- Play statistics are no longer overwritten when the panel saves while the engine updates them.
- JSON reading: paths containing `]`, duplicate keys in nested objects, values that look like keys.
- Memory: the working set is trimmed after the circle closes / a game ends.
- Missing translation keys, hex/number direction glitches, various security hardening (DLL search paths, WebView2 navigation and message-source checks, `/GS`).

### Changed
- Removed the unsupported CPU-temperature notice.
- Repo hygiene: removed the dead `Panel.vcxproj`, unified version numbers and the license notice, fixed the English README.

## [1.7.0]
- Previous release.
