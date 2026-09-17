// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// Config.h
#pragma once

#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <string>
#include <vector>

struct CompanionEntry {
    std::wstring path;
    bool runAsAdmin = false;
    std::wstring launchArgs;
    bool showInRadial = true;
};

enum class ProcessPriority : int {
    Normal      = 0,
    Idle        = 1,
    BelowNormal = 2,
    AboveNormal = 3,
    High        = 4,
    Realtime    = 5,
};

struct GameEntry {
    std::wstring name;
    std::wstring exePath;
    std::vector<CompanionEntry> companions;
    COLORREF color = RGB(46, 166, 218);
    std::wstring iconPath;
    int iconIndex = 0;
    bool autoLangSwitch = true;  // legacy — غير مستخدم
    bool runAsAdmin = false;
    std::wstring launchArgs;
    bool showInRadial = true;
    bool favorite = false;
    unsigned long long totalPlaySeconds = 0;
    DWORD lastPlayedUnix = 0;
    unsigned int playCount = 0;
    std::wstring radialBgPath;
    bool hideOriginalIcon = false;
    bool transparentIcon = false;
    ProcessPriority processPriority = ProcessPriority::Normal;
    bool applyAffinity = false;
    unsigned long long affinityMask = 0;
    int quickSlot = 0;

    // ✅ مراقبة الأداء (Performance Black Box)
    bool performanceMonitor = false;
    bool performanceCpuTemp = false;

    // ✅ لغة اللعبة: 0 = بدون تبديل، 1 = عربي، 2 = إنجليزي
    int gameLanguage = 0;
};

struct HotkeySettings {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT vk = 'G';
};

struct MuteSettings {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT vk = 'M';
    bool enabled = true;
};

struct ControllerSettings {
    bool enabled = false;
    DWORD openRadialButton = 0x0010;
    bool toggleMode = true;
    bool allowControllerDuringGame = false;
};

struct OverlaySettings {
    bool enabled = false;
    UINT hotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    UINT hotkeyVk = 'O';
    bool showOnGameLaunch = false;
    int posX = 20;
    int posY = 20;
    float opacity = 0.85f;
};

enum class Lang { AR, EN };

static const wchar_t* const SINGLE_INSTANCE_MUTEX_NAME = L"GameLauncher_SingleInstance_Mutex_v1";
static const wchar_t* const HIDDEN_WINDOW_CLASS_NAME = L"GameLauncherHiddenWnd";
static const UINT WM_APP_RELOAD_HOTKEY = WM_APP + 200;
static const UINT WM_APP_RELOAD_MUTE_HOTKEY = WM_APP + 201;
static const UINT WM_APP_RELOAD_CONTROLLER = WM_APP + 202;
static const UINT WM_APP_RELOAD_OVERLAY = WM_APP + 203;
static const UINT WM_APP_RESTART_AS_ADMIN = WM_APP + 204;

static const wchar_t* const MUTE_REQUEST_EVENT_NAME = L"GameLauncher_MuteRequest_Event_v1";

std::wstring GetExeDir();
std::wstring GetAppDataDir();
std::wstring ConfigPath();
std::wstring SettingsPath();
std::wstring LanguagePath();

void LoadGames(std::vector<GameEntry>& games);
void SaveGames(const std::vector<GameEntry>& games);

HotkeySettings LoadHotkey();
void SaveHotkey(const HotkeySettings& hk);
std::wstring HotkeyToString(const HotkeySettings& hk);

std::wstring MuteSettingsPath();
MuteSettings LoadMuteSettings();
void SaveMuteSettings(const MuteSettings& ms);
std::wstring MuteHotkeyToString(const MuteSettings& ms);

std::wstring ControllerSettingsPath();
ControllerSettings LoadControllerSettings();
void SaveControllerSettings(const ControllerSettings& cs);
std::wstring ControllerButtonName(DWORD button, Lang lang);

std::wstring OverlaySettingsPath();
OverlaySettings LoadOverlaySettings();
void SaveOverlaySettings(const OverlaySettings& os);
std::wstring OverlayHotkeyToString(const OverlaySettings& os);

std::wstring ProcessPriorityName(ProcessPriority p, Lang lang);
int ProcessPriorityCount();
ProcessPriority ProcessPriorityFromIndex(int idx);

Lang LoadLanguage();
void SaveLanguage(Lang lang);
bool IsEngineRunning();

std::wstring Escape(const std::wstring& in);
std::vector<std::wstring> SplitEscaped(const std::wstring& in, wchar_t delim);

std::wstring ResolveShortcut(const std::wstring& path);
bool IsExecutablePath(const std::wstring& path);
bool IsUriLike(const std::wstring& path);
std::wstring TrimString(const std::wstring& in);

HICON ExtractHighQualityIcon(const std::wstring& path, int index, int desiredSizePx);

struct ShortcutInfo {
    std::wstring target;
    std::wstring arguments;
    std::wstring iconPath;
    int iconIndex = 0;
    bool ok = false;
    std::wstring aumid;
};
ShortcutInfo ResolveShortcutFull(const std::wstring& lnkPath);

struct ResolvedDrop {
    std::wstring launchPath;
    std::wstring iconPath;
    int iconIndex = 0;
    bool valid = false;
};
ResolvedDrop ResolveDroppedPath(const std::wstring& path);

// ✅ 20 نمط رسم (13 قديمة + 7 جديدة)
enum class RadialStyle {
    Outline = 0, Neon = 1, Gradient = 2, Minimal = 3, Hex = 4, Comet = 5,
    Burst = 6, Sakura = 7, Glass = 8, Vortex = 9,
    Simple = 10, Aurora = 11, Halo = 12,
    Ripple = 13, Crystal = 14, Plasma = 15, Orbit = 16,
    Pixel = 17, Circuit = 18, Flame = 19,
};

std::wstring ThemePath();
RadialStyle LoadRadialStyle();
void SaveRadialStyle(RadialStyle style);
int RadialStyleCount();
std::wstring RadialStyleName(RadialStyle style, Lang lang);

std::wstring RadialTransparencyPath();
float LoadRadialTransparency();
void SaveRadialTransparency(float t);

std::wstring PanelPresetPath();
std::wstring LoadPanelPreset();
void SavePanelPreset(const std::wstring& preset);

bool IsProcessRunningByExeName(const std::wstring& exeName);
std::wstring GetFileNameFromPath(const std::wstring& path);
bool PathExistsOnDisk(const std::wstring& path);

bool IsAutoStartEnabled();
void SetAutoStartEnabled(bool enabled);
bool TestAutoStart(std::wstring& outDiagnostic);

std::wstring AutoLayoutSwitchPath();
bool LoadAutoLayoutSwitch();
void SaveAutoLayoutSwitch(bool enabled);

struct FolderExeSuggestion {
    std::wstring exePath;
    bool found = false;
};
FolderExeSuggestion FindBestExeInFolder(const std::wstring& folderPath);

struct ExeScanResult {
    std::wstring path;
    ULONGLONG size = 0;
    bool looksLikeMain = false;
};
void FindAllExesInFolder(const std::wstring& folderPath,
                         std::vector<ExeScanResult>& out,
                         int maxDepth = 4,
                         int maxCount = 100);

bool IsUwpPath(const std::wstring& path);
std::wstring MakeUwpPath(const std::wstring& aumid);
std::wstring GetUwpAumid(const std::wstring& path);
bool ActivateUwpApp(const std::wstring& aumid);
HICON ExtractUwpIcon(const std::wstring& aumid, int sizePx);
std::wstring DefaultUwpName(const std::wstring& aumid);

std::wstring PanelBackgroundPath();
std::wstring RadialBackgroundPath();
std::wstring LoadPanelBackground();
std::wstring LoadRadialBackground();
void SavePanelBackground(const std::wstring& path);
void SaveRadialBackground(const std::wstring& path);

std::wstring FirstRunDonePath();
bool IsFirstRun();
void MarkFirstRunDone();

std::wstring HubAlwaysVisiblePath();
bool LoadHubAlwaysVisible();
void SaveHubAlwaysVisible(bool enabled);

std::wstring PanelGlassEffectPath();
bool LoadPanelGlassEffect();
void SavePanelGlassEffect(bool enabled);

std::wstring PerformanceStorageDir();
std::wstring PerformanceGameDir(const std::wstring& gamePath);
std::wstring ComputeGameStorageHash(const std::wstring& gamePath);

std::wstring RadialNoGlowPath();
bool LoadRadialNoGlow();
void SaveRadialNoGlow(bool enabled);

std::wstring PerformanceSettingsPath();
bool LoadPerformanceGlobalEnabled();
void SavePerformanceGlobalEnabled(bool enabled);

// ✅ اسم اللغة داخل اللعبة (0=بدون، 1=AR، 2=EN)
std::wstring LangCodeToLayoutName(int gameLanguage);