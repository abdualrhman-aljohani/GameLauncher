# \# 🎮 GameLauncher

# 

# <div align="center">

# 

# !\[Version](https://img.shields.io/badge/version-1.7.0-a855f7)

# !\[Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078D4)

# !\[C++](https://img.shields.io/badge/C%2B%2B-17-00599C)

# !\[Win32](https://img.shields.io/badge/Win32-native-00A4EF)

# !\[License](https://img.shields.io/badge/license-PolyForm%20NC%201.0.0-f59e0b)

# 

# \*\*A native Win32 game launcher — elegant radial menu + professional performance monitoring + smart audio mute.\*\*

# 

# </div>

# 

# \---

# 

# \## ✨ Features

# 

# \### 🎯 Game Circle

# 

# \- \*\*13 drawing styles\*\* — Outline, Neon, Gradient, Minimal, Hex, Comet, Burst, Sakura, Glass, Vortex, Simple, Aurora, Halo

# \- \*\*33 ready themes\*\* — Purple, Cyber, Ember, Forest, Dracula, Nord, Tokyo Night, Catppuccin, Synthwave, Gruvbox

# \- \*\*Customizable hotkey\*\* — default `Ctrl+Alt+G`

# \- \*\*Xbox Controller support\*\* — open the circle with a controller button

# \- \*\*Quick Slots 1-9\*\* — launch your favorite game with a single number key

# \- \*\*Clean icons\*\* — fully hide frames and halos

# \- \*\*Full support\*\* — `exe`, `lnk`, `url`, `UWP`, `Steam`

# 

# \### 🎛️ Control Panel

# 

# \- Modern \*\*WebView2\*\* interface

# \- \*\*33 instant themes\*\* with live preview

# \- \*\*Custom backgrounds\*\* for panel and game circle

# \- Real \*\*Acrylic glass effect\*\*

# \- \*\*Drag \& drop\*\* files and folders

# \- \*\*Backup\*\* and import in JSON format

# \- \*\*Restart as Administrator\*\* with one click

# 

# \### 📊 Performance Black Box

# 

# \- \*\*FPS + Frame Time\*\* via ETW/DXGI

# \- \*\*GPU Temp / Power / Usage / VRAM\*\* via NVML

# \- \*\*CPU Usage + RAM\*\*

# \- Automatic \*\*Stutter Detection\*\*

# \- \*\*1% Low / 0.1% Low\*\* FPS

# \- \*\*Interactive charts\*\*

# \- \*\*CSV export\*\* for every session

# \- \*\*30-session limit\*\* per game

# 

# \### 🔇 Smart Mute

# 

# \- \*\*Ctrl+Alt+M\*\* to instantly mute the active app

# \- Official \*\*Core Audio API\*\* — no hooks

# \- List of currently muted applications

# \- \*\*Unmute all\*\* with one click

# 

# \### 🎮 Live Overlay

# 

# \- \*\*Ctrl+Shift+O\*\* to show/hide

# \- FPS + GPU + CPU + RAM + playtime

# \- \*\*Updates every 500ms\*\*

# \- Customizable transparency

# \- Auto-shows on game launch (optional)

# 

# \---

# 

# \## 🛡️ Safety First

# 

# GameLauncher is built on the principle of \*\*"Don't touch the game"\*\*.

# 

# \*\*Strictly forbidden:\*\*

# 

# \- DLL injection

# \- Reading game memory

# \- Hooks on DirectX / OpenGL / Vulkan

# \- Modifying game files

# \- Modifying network connections

# \- Overlay injection

# 

# \*\*Allowed — official Windows APIs only:\*\*

# 

# \- ETW (Event Tracing for Windows)

# \- NVML (NVIDIA Management Library)

# \- Core Audio API

# \- D3DKMT

# \- PDH

# \- GDI+

# 

# \### 🎯 Anti-Cheat Compatible?

# 

# \- \*\*Without Black Box\*\* — 100% safe in all competitive games

# \- \*\*With Black Box\*\* — theoretically safe because all APIs are Microsoft-signed

# 

# \---

# 

# \## 📦 Requirements

# 

# \*\*Mandatory:\*\*

# 

# \- Windows 10 (2004+) or Windows 11

# \- WebView2 Runtime — pre-installed on Windows 11

# \- x64 processor

# 

# \*\*To build from source:\*\*

# 

# \- Visual Studio 2022 with `Desktop development with C++`

# \- NuGet Package Manager

# 

# \*\*Optional:\*\*

# 

# \- Administrator rights — only required for ETW FPS on Windows older than 10 2004

# \- NVML — only required for reading GPU temperature

# 

# \---

# 

# \## 📖 Quick Start

# 

# \*\*1)\*\* Run `GameLauncher.exe` — it runs in the System Tray.

# 

# \*\*2)\*\* Open the game circle with `Ctrl+Alt+G` or by double-clicking the Tray icon.

# 

# \*\*3)\*\* Click the \*\*Hub\*\* button in the center to open the control panel.

# 

# \*\*4)\*\* Add your games — drag \& drop, or the "Add Game" button.

# 

# \*\*5)\*\* Enable \*\*Black Box\*\* from any game's details tab to monitor its performance.

# 

# \---

# 

# \## 📂 Data Folder

# 

# Path: `%APPDATA%\\GameLauncher\\`

# 

# | File | Description |

# |------|-------------|

# | `games.cfg` | Games list and all settings |

# | `performance/<hash>/\*.glperf` | Performance reports |

# | `launcher.log` | Diagnostic log |

# | `etw\_diag.log` | Detailed ETW log |

# | `panel\_ui.html/css/js` | Extracted UI |

# | `WebView2Data/` | WebView2 data |

# | `\*.cfg` | Individual settings |

# 

# \---

# 

# \## 🐛 Troubleshooting

# 

# \### FPS not working?

# 

# \- Run GameLauncher \*\*as Administrator\*\*

# \- Or ensure Windows 10 2004+

# 

# \### CPU temperature not showing?

# 

# \- Feature is deferred — pending PawnIO support

# \- Alternative — use HWiNFO in parallel

# 

# \### Panel won't open?

# 

# \- Ensure \*\*WebView2 Runtime\*\* is installed

# 

# \### Hotkeys not working?

# 

# \- May be reserved by another program

# \- Change from Settings

# 

# \---

# 

# \## 🔒 License

# 

# This project is licensed under the \*\*PolyForm Noncommercial License 1.0.0\*\*.

# 

# \*\*Permitted:\*\*

# 

# \- Personal use

# \- Personal modification

# \- Sharing with license preservation

# 

# \*\*Forbidden:\*\*

# 

# \- Commercial use

# \- Selling

# \- Redistribution as part of a paid product

# 

# For commercial licensing, contact the owner.

# 

# See `LICENSE.txt` for full details.

# 

# \---

# 

# \## 👤 Author

# 

# \*\*Abdualrhman Aljohani\*\*

# 

# \- GitHub: \[@abdualrhman-aljohani](https://github.com/abdualrhman-aljohani)

# \- Project: \[GameLauncher](https://github.com/abdualrhman-aljohani/GameLauncher)

# 

# \---

# 

# <div align="center">

# 

# \*\*Made with ❤️ in Saudi Arabia\*\*

# 

# Copyright © 2026 Abdualrhman Aljohani

# 

# </div>

