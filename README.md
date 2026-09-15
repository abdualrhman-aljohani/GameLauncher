# GameLauncher

**English | [العربية](README.ar.md)**

A lightweight, native Win32 radial launcher for your games — press a hotkey anywhere on Windows, pick a game from an interactive on-screen wheel, and jump straight in. No taskbar hunting, no separate library apps to open first.

## Why

Your games live scattered across the desktop, Steam, the Microsoft Store, and random folders. GameLauncher puts all of them one keypress away, no matter what you're currently doing — even mid-game.

## Highlights

- **Radial game wheel** — a circular, animated on-screen menu with **13 customizable visual themes** (Neon, Glass, Sakura, Vortex, and more)
- **Universal game support** — `.exe`, `.lnk` shortcuts, URL protocols, Microsoft Store (UWP) apps, Steam games, and Google Play Games emulators
- **Smart per-game audio muting** — automatically mutes other apps (Discord, browser, etc.) while you play, plus a dedicated hotkey to mute/unmute whatever app is currently focused
- **Performance controls** — set CPU process priority and core affinity per game
- **Xbox controller support** — open, navigate, and close the wheel entirely with a gamepad
- **Detailed play stats** — play count, total playtime, and last-played date per game
- **Automatic keyboard layout switching** — bind a language (e.g. Arabic/English) to each game
- **Backup & restore** — export/import your entire game list and settings in one click

## Built with

Pure native **C++ / Win32** — no .NET runtime, no Electron. The control panel UI runs on **WebView2** for a modern look while keeping the whole app lightweight on memory and CPU.

## Requirements

- Windows 10 (version 1809+) or Windows 11
- [WebView2 Runtime](https://developer.microsoft.com/microsoft-edge/webview2/) (preinstalled on Windows 11 and most Windows 10 updates)

## Getting started

1. Run `GameLauncher.exe` → it hides itself in the system tray
2. Press `Ctrl+Alt+G` to open the game wheel
3. From the tray icon → **"Control Panel..."** to add and manage your games

## Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+Alt+G` | Open the game wheel |
| `Ctrl+Alt+M` | Mute/unmute the currently focused app |

Both shortcuts are fully customizable from the control panel.

## Data

All settings and game data are stored locally at:
```
%APPDATA%\GameLauncher\
```
No cloud, no telemetry, no accounts.

## Building from source

Requires Visual Studio 2022 with the **Desktop development with C++** workload.

```
git clone https://github.com/abdualrhman-aljohani/GameLauncher.git
cd GameLauncher
```

Open `GameLauncher.sln`, select your platform/configuration, and build the solution.

## License

This project is licensed under the **[PolyForm Noncommercial License 1.0.0](LICENSE.txt)**.

In short: you're free to use, modify, and share this software for any **noncommercial purpose** (personal use, learning, experimenting, hobby projects). **Selling it, or using it as part of a commercial product or service, is not permitted** without prior written permission from the copyright holder.

Copyright © 2026 [Abdualrhman Aljohani](https://github.com/abdualrhman-aljohani)
