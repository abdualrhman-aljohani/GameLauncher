# GameLauncher — Description

**English | [العربية](DESCRIPTION.ar.md)**

## Short (GitHub "About" field / social bio)

A lightweight radial game launcher for Windows — press a hotkey, pick your game from an on-screen wheel, jump straight in. Free & open-source (noncommercial).

## Medium (Twitter/X, video caption)

GameLauncher turns any spot on your Windows screen into a game hub. Press `Ctrl+Alt+G`, pick a game from an animated radial wheel with 13 themes, and play — works with Steam, the Microsoft Store, shortcuts, and more. Native C++, zero bloat.

## Full (video script / detailed explanation)

**The idea in one sentence:**
GameLauncher is a lightweight Windows game launcher that opens an interactive radial wheel anywhere on your screen with a single hotkey — you pick your game and it launches instantly, no digging through the desktop or separate library apps (Steam, Epic, etc.).

**How it works:**
1. Run it once — it disappears into the system tray, using almost no resources
2. Press **Ctrl+Alt+G** anytime, even while inside another game, and the game wheel pops up over everything
3. Click the icon of the game you want and it launches immediately
4. A separate control panel (opened from the tray) lets you add, edit, and reorder your games with a graphical UI

**Key features:**
- Supports basically every type of game shortcut: `.exe` files, `.lnk` shortcuts, URL protocols, Microsoft Store (UWP) apps, Steam games, and even Google Play Games emulators
- 13 different visual themes for the wheel (Neon, Glass, Sakura, Vortex, and more) — fully customizable look
- Smart per-game audio muting — automatically mutes apps like Discord while you play, plus a standalone hotkey (**Ctrl+Alt+M**) to instantly mute/unmute whatever app is currently focused
- Performance controls — set CPU priority and core affinity per game, useful on lower-end machines
- Full Xbox controller support to open, navigate, and close the wheel without touching a keyboard or mouse
- Detailed play statistics: play count, total playtime, and last-played date for every game
- Automatic keyboard layout switching — bind a language to each game so it switches automatically on launch
- One-click backup and restore of your entire game list and settings

**Under the hood:**
Built entirely in native **C++ / Win32** — no .NET, no Electron — so it's extremely light on memory and CPU. The control panel UI runs on WebView2 for a modern interface without sacrificing that lightness.

**Requirements:**
Windows 10 (1809+) or Windows 11, with WebView2 Runtime (already installed on most modern machines).
