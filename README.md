<div align="center">

<img src="docs/images/icon.png" width="120" alt="GameLauncher icon">

# GameLauncher

**A lightweight native Win32 game launcher – an elegant radial menu, a modern control panel, performance monitoring and smart audio mute.**

[![Version](https://img.shields.io/badge/version-1.8.0-a855f7)](https://github.com/abdualrhman-aljohani/GameLauncher/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D4)](https://github.com/abdualrhman-aljohani/GameLauncher)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)](https://github.com/abdualrhman-aljohani/GameLauncher)
[![Win32](https://img.shields.io/badge/Win32-native-00A4EF)](https://github.com/abdualrhman-aljohani/GameLauncher)
[![License](https://img.shields.io/badge/license-PolyForm%20NC%201.0.0-f59e0b)](https://github.com/abdualrhman-aljohani/GameLauncher/blob/main/LICENSE)

<p align="center">
  <a href="https://github.com/abdualrhman-aljohani/GameLauncher/releases/latest/download/GameLauncher.exe">
    <img src="https://img.shields.io/badge/📥%20Download%20Latest%20Release-GameLauncher.exe-2ea44f?style=for-the-badge&logo=windows" alt="Download GameLauncher">
  </a>
</p>

([CHANGELOG.md](CHANGELOG.md) · [Changelog]) · ([README.ar.md](README.ar.md) · [العربية])

</div>

<!--
  Add your screenshots to docs/images/ and uncomment:
  <p align="center"><img src="docs/images/radial.png" width="45%"> <img src="docs/images/panel.png" width="54%"></p>
  Tip: use a neutral background image so the screenshots don't include third-party artwork.
-->

---

## ✨ What's new in 1.8.0

- **Import your installed games in one click** — Steam, Epic Games, GOG Galaxy, Ubisoft Connect and your `Games` folders.
- **Scan any folder** — including a whole `steamapps\common`; every game inside is found, not just the first few.
- **Much clearer drag & drop** for new users (full-window drop overlay + empty-state guide) and a welcome screen on the very first run.
- **Fixes** for Unicode (Arabic/Chinese/… game names and paths), backup & restore (now includes play sessions), browsers / launchers that failed to open, Boost restore feedback, memory usage, and more.
- **New icon**, dark title bar, tooltips on every icon, offline UI mode, an installer, and an optional sharp mode for scaled displays.

See the full [CHANGELOG](CHANGELOG.md).

---

## ✨ Features

### 🎯 Game Circle

- **6 drawing styles** — Ripple, Flame, Neon, Gradient, Aurora, plus a fully **Custom** style you design yourself
- **6 ready themes** — Ember, Nord, Obsidian, Royal, Cherry, Nebula, plus a fully **Custom** theme
- **Customizable hotkey** — default `Ctrl+Alt+G`
- **Xbox Controller support** — open the circle with a controller button
- **Quick Slots 1-9** — launch your favorite game with a single number key
- **Clean icons** — fully hide frames and halos; optional **custom circular background** per game
- **Everything launches** — `exe`, `lnk`, `url`, UWP / Start-menu apps, Steam, Epic, Ubisoft (through the store, so login and updates just work)

### 🎛️ Control Panel

- Modern **WebView2** interface, **English and Arabic** (English by default for new users)
- **Import installed games** from Steam, Epic, GOG, Ubisoft and common `Games` folders — already-added games are detected
- **Drag & drop** files, shortcuts, links or whole folders
- **Scan folder** button — finds one game per subfolder and names it from its executable
- **Companion programs** launched together with a game, per-game launch arguments and *run as administrator*
- **Backup & restore** (JSON): games, per-game settings, play time, launch counts and session history
- **Custom themes and backgrounds**, real Acrylic glass effect
- **Restart as Administrator** with one click

### 📊 Performance Black Box

- **FPS + Frame Time** via ETW/DXGI
- **GPU Temp / Power / Usage / VRAM** via NVML
- **CPU Usage + RAM**, automatic **stutter detection**, **1% / 0.1% Low** FPS
- **Interactive charts** and **CSV export** for every session (30 sessions per game)

### 🚀 Boost (optional, per game)

Process priority, CPU affinity, MMCSS, system responsiveness and timer resolution — applied while the game runs and **restored automatically** afterwards. A **Restore Boost defaults** button in Settings reports whether anything needed restoring.

### 🔇 Smart Mute

- **Ctrl+Alt+M** mutes the active app instantly — official **Core Audio API**, no hooks
- List of muted apps and **Unmute all**

### 🎮 Live Overlay

- **Ctrl+Shift+O** to show/hide — FPS + GPU + CPU + RAM + playtime, updated every 500 ms
- Adjustable transparency, optional auto-show on game launch

---

## 📥 Install

**Installer (recommended):** download `GameLauncher-Setup-<version>.exe` from the [Releases](../../releases) page. It installs to *Program Files*, adds a Start-menu shortcut, supports in-place upgrades, and on uninstall it restores any Windows settings that Boost changed.

**Build it yourself:** see [Build from source](#-build-from-source).

> ⚠️ **About Windows SmartScreen** — release builds are not yet code-signed, so Windows may show *"Unknown publisher"*. Click **More info → Run anyway**. You can verify the download against the SHA-256 listed in the release notes:
> `certutil -hashfile GameLauncher-Setup-1.8.0.exe SHA256`

### Requirements

- Windows 10 (2004+) or Windows 11, x64
- WebView2 Runtime — pre-installed on Windows 11 ([download](https://developer.microsoft.com/microsoft-edge/webview2/) for Windows 10)
- The control panel loads its UI libraries (Tailwind, Font Awesome, Google Fonts, ApexCharts) from public CDNs. To work **fully offline**, run `tools\build-ui-assets.ps1` once — it creates a `ui_assets` folder next to the exe. The radial menu itself always works offline.
- **Administrator rights** are needed only for: Black Box FPS (ETW), Boost registry tweaks, and running a game as administrator

---

## 📖 Quick Start

1. Run `GameLauncher.exe` — it lives in the **system tray**. On the very first run the control panel and the circle open for you.
2. Open the game circle with `Ctrl+Alt+G` (or double-click the tray icon).
3. Add your games — any of these:
   - **Drag & drop** an exe, shortcut, link or a whole games folder onto the panel
   - **Import games** — Steam · Epic · GOG · Ubisoft · `Games` folders
   - **Scan folder** — pick a folder and choose from what's found
   - **Add game** — pick a file manually
4. Click the **Hub** in the center of the circle to open the control panel any time.
5. Enable **Black Box** from a game's details tab to monitor its performance.

> **Games with their own launcher** (Genshin Impact, Wuthering Waves, …): the importer looks in the usual places (`\Games`, `\HoYoPlay\games`, `\Genshin Impact`, `\Wuthering Waves`, …) and picks the game executable rather than `launcher.exe`. Anything else: use **Scan folder** or drag the game folder in.

---

## 🛡️ Safety First

GameLauncher is built on the principle of **"don't touch the game"**.

**Never done:** DLL injection · reading game memory · hooks on DirectX / OpenGL / Vulkan · modifying game files or network connections · overlay injection.

**Only official Windows APIs:** ETW · NVML · Core Audio · D3DKMT · PDH · GDI+.

**Anti-cheat:** the launcher never touches a game's memory, files or rendering, but no tool can promise compatibility with every anti-cheat — use it at your own risk in competitive games. **Boost** changes the game's process priority/affinity and a few Windows registry values (restored automatically afterwards).

**Privacy:** no telemetry, no accounts. Game import only reads local store files (Steam / Epic manifests, GOG and Ubisoft registry entries). The only network access is the panel loading its UI libraries from CDNs, which the offline mode removes.

---

## 🔧 Build from source

**Requirements:** Visual Studio 2022 or newer with *Desktop development with C++* · NuGet (restores the WebView2 SDK).

```bat
nuget restore GameLauncher.sln
msbuild GameLauncher.sln /p:Configuration=Release /p:Platform=x64
:: output: bin\Release\GameLauncher.exe
```

| Task | How |
|------|-----|
| Unit tests (JSON + store parsers) | see [`tests/README.md`](tests/README.md) |
| Installer | build Release, then compile [`installer/GameLauncher.iss`](installer/README.md) with Inno Setup 6.3+ |
| Offline UI assets | `powershell -ExecutionPolicy Bypass -File tools\build-ui-assets.ps1` |
| Regenerate the icon | `python tools/make_icon.py` (needs Pillow) — alternative styles are in `Common/icon_alternatives/` |
| Sign a release | `tools\sign-release.ps1` — see [`docs/PUBLISHING.ar.md`](docs/PUBLISHING.ar.md) |

Pushing a tag such as `v1.8.0` runs the release workflow (`.github/workflows/release.yml`) that builds the exe and the installer and attaches them to a GitHub Release.

---

## 📂 Data folder

Path: `%APPDATA%\GameLauncher\`

| File | Description |
|------|-------------|
| `games.cfg` | Games list and per-game settings |
| `games.cfg.bak` | Copy of the previous good `games.cfg` (kept on every save) |
| `games.cfg.before-import.bak` | Your list right before the last backup import |
| `language.cfg` | UI language |
| `performance/<hash>/*.glperf` | Performance reports |
| `launcher.log` | Diagnostic log |
| `panel_ui.html/css/js` | Extracted UI |
| `WebView2Data/` | WebView2 data |
| `*.cfg` | Individual settings |

---

## 🐛 Troubleshooting

- **FPS not showing** — run GameLauncher **as Administrator** (ETW needs it).
- **CPU temperature** — intentionally not supported (it needs kernel drivers that can conflict with anti-cheat). Use HWiNFO alongside if you need it.
- **Panel won't open** — make sure the **WebView2 Runtime** is installed.
- **Hotkeys don't work** — another program may have reserved them; change them in Settings.
- **A game or browser won't open from the circle** — send the `Launch FAILED` line from `%APPDATA%\GameLauncher\launcher.log` when reporting an issue.
- **Scaled display looks blurry** — enable the experimental **Sharp mode** in Settings (restart required).

---

## 🔒 License

Licensed under the **PolyForm Noncommercial License 1.0.0** — personal use, personal modification and sharing (with the license preserved) are allowed; commercial use, selling and redistribution as part of a paid product are not. For a commercial license, contact the author. See [`LICENSE.txt`](LICENSE.txt).

## 👤 Author

**Abdualrhman Aljohani** — [@abdualrhman-aljohani](https://github.com/abdualrhman-aljohani) · [GameLauncher](https://github.com/abdualrhman-aljohani/GameLauncher)

<div align="center">

**Made with ❤️ in Saudi Arabia** · Copyright © 2026 Abdualrhman Aljohani

</div>
