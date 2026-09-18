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
    bool autoLangSwitch = true;
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
    bool performanceMonitor = false;
    bool performanceCpuTemp = false;
    int gameLanguage = 0;
    bool boostFps = false;
    bool boostDisableCore0 = true;
    bool boostHighPriority = true;
    bool boostStopStats = true;
    bool boostTimerResolution = false;
    bool boostSystemResponsiveness = false;
    bool boostMmcss = false;
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
static const UINT WM_APP_RESTORE_BOOST_DEFAULTS = WM_APP + 205;

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

// ✅ 20 نمط رسم (نعرض 5 فقط الآن)
enum class RadialStyle {
    Outline = 0, Neon = 1, Gradient = 2, Minimal = 3, Hex = 4, Comet = 5,
    Burst = 6, Sakura = 7, Glass = 8, Vortex = 9,
    Simple = 10, Aurora = 11, Halo = 12,
    Ripple = 13, Crystal = 14, Plasma = 15, Orbit = 16,
    Pixel = 17, Circuit = 18, Flame = 19,
    Custom = 20,  // ✅ نمط مخصص يبنيه المستخدم
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

// ═════════════════════════════════════════════════════════════
// ✅ الثيم المخصص للوحة (4 ألوان + 3 سلايدرز)
// ═════════════════════════════════════════════════════════════
struct CustomTheme {
    // 4 ألوان (hex strings بدون #)
    std::wstring accentHex    = L"7c3aed";  // اللون الأساسي
    std::wstring secondaryHex = L"a855f7";  // اللون الثانوي
    std::wstring bgHex        = L"150f2c";  // الخلفية
    std::wstring cardHex      = L"1e1636";  // البطاقات
    // 3 سلايدرز
    float cardOpacity   = 0.32f;  // شفافية البطاقات 0.10..0.85
    float accentStrength = 0.70f; // قوة التمييز 0..1
    float bgGlow        = 0.40f;  // توهج الخلفية 0..1
    bool isActive = false;         // هل الثيم المخصص هو المُفعّل حالياً؟
};

std::wstring CustomThemePath();
CustomTheme LoadCustomTheme();
void SaveCustomTheme(const CustomTheme& ct);

// ═════════════════════════════════════════════════════════════
// ✅ النمط المخصص للدائرة (5 تحكمات)
// ═════════════════════════════════════════════════════════════
struct CustomRadialStyle {
    int ringCount     = 2;    // عدد الحلقات 1..4
    int ringThickness = 2;    // سماكة الحلقة 1..8 px
    bool dashed       = false; // متصل / متقطع
    int glowLayers    = 1;    // طبقات التوهج 0..3
    float glowStrength = 0.60f; // شدة التوهج 0..1
    bool isActive     = false; // هل النمط المخصص مُفعّل حالياً؟
};

std::wstring CustomRadialStylePath();
CustomRadialStyle LoadCustomRadialStyle();
void SaveCustomRadialStyle(const CustomRadialStyle& cs);

// ═════════════════════════════════════════════════════════════
// ✅ مقياس الدائرة (3 سلايدرز)
// ═════════════════════════════════════════════════════════════
struct RadialScale {
    int iconSize  = 42;   // نصف قطر أيقونة اللعبة 30..60
    int hubSize   = 46;   // نصف قطر زر الـ Hub 30..60
    int orbitDist = 118;  // مسافة الألعاب من المركز 90..180
};

std::wstring RadialScalePath();
RadialScale LoadRadialScale();
void SaveRadialScale(const RadialScale& rs);

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

std::wstring LangCodeToLayoutName(int gameLanguage);

bool IsProcessElevated();

// ═════════════════════════════════════════════════════════════
// ✅ سجل جلسات اللعب (Play Sessions History)
// ═════════════════════════════════════════════════════════════
struct PlaySessionEntry {
    DWORD startUnix = 0;
    DWORD durationSec = 0;
};

std::wstring PlaySessionsDir();
void AppendPlaySession(const std::wstring& gamePath,
                       const std::wstring& gameName,
                       DWORD startUnix,
                       DWORD durationSec);
std::vector<PlaySessionEntry> LoadPlaySessions(const std::wstring& gamePath,
                                                std::wstring* outGameName = nullptr);
bool ClearPlaySessions(const std::wstring& gamePath);