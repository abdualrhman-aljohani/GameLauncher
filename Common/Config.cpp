// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// Config.cpp
#include "Config.h"
#include <fstream>
#include <cwctype>
#include <algorithm>
#include <shobjidl.h>
#include <objbase.h>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <taskschd.h>
#include <comdef.h>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "propsys.lib")
#pragma comment(lib, "taskschd.lib")

static const wchar_t* GAMES_FILE        = L"games.cfg";
static const wchar_t* SETTINGS_FILE     = L"hotkey.cfg";
static const wchar_t* LANGUAGE_FILE     = L"language.cfg";
static const wchar_t* THEME_FILE        = L"theme.cfg";
static const wchar_t* PRESET_FILE       = L"panel_theme.cfg";
static const wchar_t* PANEL_BG_FILE     = L"panel_bg.cfg";
static const wchar_t* RADIAL_BG_FILE    = L"radial_bg.cfg";
static const wchar_t* MUTE_SETTINGS_FILE = L"mute_hotkey.cfg";
static const wchar_t* CONTROLLER_FILE   = L"controller.cfg";
static const wchar_t* RADIAL_TRANSP_FILE = L"radial_transparency.cfg";
static const wchar_t* FIRST_RUN_FILE    = L"first_run_done.txt";
static const wchar_t* HUB_VISIBLE_FILE  = L"hub_always_visible.cfg";
static const wchar_t* PANEL_GLASS_FILE  = L"panel_glass.cfg";
static const wchar_t* OVERLAY_FILE      = L"overlay.cfg";
static const wchar_t* PERFORMANCE_GLOBAL_FILE = L"performance_global.cfg";
static const wchar_t* RADIAL_NOGLOW_FILE = L"radial_noglow.cfg";

static const wchar_t FIELD_SEP = L'\x1F';
static const wchar_t ITEM_SEP  = L'\x1E';
static const wchar_t SUB_SEP   = L'\x1D';

static const wchar_t* TASK_FOLDER   = L"\\GameLauncher";
static const wchar_t* TASK_NAME     = L"GameLauncher_AutoStart";
static const wchar_t* TASK_AUTHOR   = L"GameLauncher";

static HANDLE g_gamesCfgMutex = nullptr;
static void EnsureGamesCfgMutex() {
    if (!g_gamesCfgMutex) {
        g_gamesCfgMutex = CreateMutexW(nullptr, FALSE, L"GameLauncher_gamescfg_mutex_v1");
    }
}
class ScopedGamesCfgLock {
public:
    ScopedGamesCfgLock() {
        EnsureGamesCfgMutex();
        locked_ = false;
        if (g_gamesCfgMutex) {
            DWORD r = WaitForSingleObject(g_gamesCfgMutex, 5000);
            if (r == WAIT_OBJECT_0 || r == WAIT_ABANDONED) locked_ = true;
        }
    }
    ~ScopedGamesCfgLock() {
        if (locked_ && g_gamesCfgMutex) ReleaseMutex(g_gamesCfgMutex);
    }
private:
    bool locked_;
};

std::wstring GetExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring s(path);
    size_t pos = s.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : s.substr(0, pos);
}

std::wstring GetAppDataDir() {
    static std::wstring cached;
    if (!cached.empty()) return cached;
    std::wstring dir;
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appData)) && appData) {
        dir = std::wstring(appData) + L"\\GameLauncher";
        CoTaskMemFree(appData);
    } else dir = GetExeDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    cached = dir;
    return cached;
}

static void MigrateLegacy(const wchar_t* fileName) {
    std::wstring oldPath = GetExeDir() + L"\\" + fileName;
    std::wstring newPath = GetAppDataDir() + L"\\" + fileName;
    if (GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) return;
    if (GetFileAttributesW(oldPath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    CopyFileW(oldPath.c_str(), newPath.c_str(), TRUE);
}

std::wstring ConfigPath() {
    static bool m = false;
    if (!m) { MigrateLegacy(GAMES_FILE); m = true; }
    return GetAppDataDir() + L"\\" + GAMES_FILE;
}
std::wstring SettingsPath() {
    static bool m = false;
    if (!m) { MigrateLegacy(SETTINGS_FILE); m = true; }
    return GetAppDataDir() + L"\\" + SETTINGS_FILE;
}
std::wstring LanguagePath() {
    static bool m = false;
    if (!m) { MigrateLegacy(LANGUAGE_FILE); m = true; }
    return GetAppDataDir() + L"\\" + LANGUAGE_FILE;
}
std::wstring ThemePath() {
    static bool m = false;
    if (!m) { MigrateLegacy(THEME_FILE); m = true; }
    return GetAppDataDir() + L"\\" + THEME_FILE;
}
std::wstring MuteSettingsPath() {
    static bool m = false;
    if (!m) { MigrateLegacy(MUTE_SETTINGS_FILE); m = true; }
    return GetAppDataDir() + L"\\" + MUTE_SETTINGS_FILE;
}
std::wstring ControllerSettingsPath() { return GetAppDataDir() + L"\\" + CONTROLLER_FILE; }
std::wstring RadialTransparencyPath() { return GetAppDataDir() + L"\\" + RADIAL_TRANSP_FILE; }
std::wstring FirstRunDonePath()       { return GetAppDataDir() + L"\\" + FIRST_RUN_FILE; }
std::wstring HubAlwaysVisiblePath()   { return GetAppDataDir() + L"\\" + HUB_VISIBLE_FILE; }
std::wstring PanelPresetPath() { return GetAppDataDir() + L"\\" + PRESET_FILE; }
std::wstring PanelBackgroundPath()  { return GetAppDataDir() + L"\\" + PANEL_BG_FILE; }
std::wstring RadialBackgroundPath() { return GetAppDataDir() + L"\\" + RADIAL_BG_FILE; }
std::wstring PanelGlassEffectPath() { return GetAppDataDir() + L"\\" + PANEL_GLASS_FILE; }
std::wstring OverlaySettingsPath()  { return GetAppDataDir() + L"\\" + OVERLAY_FILE; }
std::wstring PerformanceSettingsPath() { return GetAppDataDir() + L"\\" + PERFORMANCE_GLOBAL_FILE; }
std::wstring RadialNoGlowPath() { return GetAppDataDir() + L"\\" + RADIAL_NOGLOW_FILE; }

static std::wstring ToLowerCopy(const std::wstring& in) {
    std::wstring s = in;
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}
static size_t FindCaseInsensitive(const std::wstring& haystack,
                                  const std::wstring& needle, size_t from = 0) {
    if (needle.empty() || haystack.size() < needle.size()) return std::wstring::npos;
    std::wstring h = ToLowerCopy(haystack);
    std::wstring n = ToLowerCopy(needle);
    return h.find(n, from);
}

static std::vector<std::wstring> Split(const std::wstring& in, wchar_t delim) {
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : in) {
        if (c == delim) { parts.push_back(cur); cur.clear(); }
        else cur += c;
    }
    parts.push_back(cur);
    return parts;
}
std::wstring Escape(const std::wstring& in) { return in; }
std::vector<std::wstring> SplitEscaped(const std::wstring& in, wchar_t delim) { return Split(in, delim); }

static std::wstring SerializeCompanion(const CompanionEntry& c) {
    if (!c.runAsAdmin && c.launchArgs.empty() && c.showInRadial) return c.path;
    return c.path + SUB_SEP + (c.runAsAdmin ? L"1" : L"0") + SUB_SEP +
           c.launchArgs + SUB_SEP + (c.showInRadial ? L"1" : L"0");
}
static CompanionEntry DeserializeCompanion(const std::wstring& raw) {
    CompanionEntry c;
    auto p = Split(raw, SUB_SEP);
    c.path = p.empty() ? raw : p[0];
    if (p.size() >= 2) c.runAsAdmin = (p[1] == L"1");
    if (p.size() >= 3) c.launchArgs = p[2];
    if (p.size() >= 4) c.showInRadial = (p[3] != L"0");
    return c;
}

void LoadGames(std::vector<GameEntry>& games) {
    ScopedGamesCfgLock lock;
    games.clear();
    std::wifstream f(ConfigPath().c_str());
    if (!f.is_open()) return;
    std::wstring line;
    bool legacy = false;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        bool isNew = line.find(FIELD_SEP) != std::wstring::npos;
        auto fields = isNew ? Split(line, FIELD_SEP) : Split(line, L'|');
        if (!isNew) legacy = true;
        if (fields.size() < 3) continue;

        GameEntry g;
        g.name = fields[0];
        g.exePath = fields[1];
        g.color = (COLORREF)wcstoul(fields[2].c_str(), nullptr, 10);
        if (fields.size() >= 4 && !fields[3].empty()) {
            auto comps = isNew ? Split(fields[3], ITEM_SEP) : Split(fields[3], L';');
            for (auto& c : comps) if (!c.empty()) g.companions.push_back(DeserializeCompanion(c));
        }
        if (fields.size() >= 5) g.iconPath = fields[4];
        if (fields.size() >= 6 && !fields[5].empty()) g.iconIndex = _wtoi(fields[5].c_str());
        if (fields.size() >= 7 && !fields[6].empty()) g.autoLangSwitch = (fields[6] != L"0");
        if (fields.size() >= 8 && !fields[7].empty()) g.runAsAdmin = (fields[7] != L"0");
        if (fields.size() >= 9) g.launchArgs = fields[8];
        if (fields.size() >= 10 && !fields[9].empty()) g.showInRadial = (fields[9] != L"0");
        if (fields.size() >= 11 && !fields[10].empty()) g.favorite = (fields[10] != L"0");
        if (fields.size() >= 12 && !fields[11].empty()) g.totalPlaySeconds = _wcstoui64(fields[11].c_str(), nullptr, 10);
        if (fields.size() >= 13 && !fields[12].empty()) g.lastPlayedUnix = (DWORD)wcstoul(fields[12].c_str(), nullptr, 10);
        if (fields.size() >= 14 && !fields[13].empty()) g.playCount = (unsigned int)wcstoul(fields[13].c_str(), nullptr, 10);
        if (fields.size() >= 15) g.radialBgPath = fields[14];
        if (fields.size() >= 16 && !fields[15].empty()) g.hideOriginalIcon = (fields[15] != L"0");
        if (fields.size() >= 17 && !fields[16].empty()) g.transparentIcon = (fields[16] != L"0");
        if (fields.size() >= 18 && !fields[17].empty()) {
            int prio = _wtoi(fields[17].c_str());
            if (prio >= 0 && prio <= 5) g.processPriority = (ProcessPriority)prio;
        }
        if (fields.size() >= 19 && !fields[18].empty()) g.applyAffinity = (fields[18] != L"0");
        if (fields.size() >= 20 && !fields[19].empty()) g.affinityMask = _wcstoui64(fields[19].c_str(), nullptr, 10);
        if (fields.size() >= 21 && !fields[20].empty()) {
            int slot = _wtoi(fields[20].c_str());
            if (slot >= 0 && slot <= 9) g.quickSlot = slot;
        }
        if (fields.size() >= 22 && !fields[21].empty()) g.performanceMonitor = (fields[21] != L"0");
        if (fields.size() >= 23 && !fields[22].empty()) g.performanceCpuTemp = (fields[22] != L"0");
        // ✅ gameLanguage (0=بدون، 1=AR، 2=EN)
        if (fields.size() >= 24 && !fields[23].empty()) {
            int gl = _wtoi(fields[23].c_str());
            if (gl >= 0 && gl <= 2) g.gameLanguage = gl;
        }

        games.push_back(g);
    }
    f.close();
    if (legacy) SaveGames(games);
}

void SaveGames(const std::vector<GameEntry>& games) {
    ScopedGamesCfgLock lock;
    std::wstring target = ConfigPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        for (auto& g : games) {
            std::wstring comp;
            for (size_t i = 0; i < g.companions.size(); i++) {
                if (i) comp += ITEM_SEP;
                comp += SerializeCompanion(g.companions[i]);
            }
            f << g.name << FIELD_SEP << g.exePath << FIELD_SEP
              << (unsigned long)g.color << FIELD_SEP << comp << FIELD_SEP
              << g.iconPath << FIELD_SEP << g.iconIndex << FIELD_SEP
              << (g.autoLangSwitch ? 1 : 0) << FIELD_SEP
              << (g.runAsAdmin ? 1 : 0) << FIELD_SEP
              << g.launchArgs << FIELD_SEP
              << (g.showInRadial ? 1 : 0) << FIELD_SEP
              << (g.favorite ? 1 : 0) << FIELD_SEP
              << g.totalPlaySeconds << FIELD_SEP
              << (unsigned long)g.lastPlayedUnix << FIELD_SEP
              << g.playCount << FIELD_SEP
              << g.radialBgPath << FIELD_SEP
              << (g.hideOriginalIcon ? 1 : 0) << FIELD_SEP
              << (g.transparentIcon ? 1 : 0) << FIELD_SEP
              << (int)g.processPriority << FIELD_SEP
              << (g.applyAffinity ? 1 : 0) << FIELD_SEP
              << g.affinityMask << FIELD_SEP
              << g.quickSlot << FIELD_SEP
              << (g.performanceMonitor ? 1 : 0) << FIELD_SEP
              << (g.performanceCpuTemp ? 1 : 0) << FIELD_SEP
              << g.gameLanguage << L"\n";
        }
        f.flush();
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

HotkeySettings LoadHotkey() {
    HotkeySettings hk;
    std::wifstream f(SettingsPath().c_str());
    if (!f.is_open()) return hk;
    unsigned long mods = 0, vk = 0;
    if (f >> mods >> vk && vk != 0) { hk.modifiers = (UINT)mods; hk.vk = (UINT)vk; }
    return hk;
}
void SaveHotkey(const HotkeySettings& hk) {
    std::wstring target = SettingsPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (unsigned long)hk.modifiers << L" " << (unsigned long)hk.vk << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring HotkeyToString(const HotkeySettings& hk) {
    std::wstring s;
    if (hk.modifiers & MOD_CONTROL) s += L"Ctrl+";
    if (hk.modifiers & MOD_ALT)     s += L"Alt+";
    if (hk.modifiers & MOD_SHIFT)   s += L"Shift+";
    if (hk.modifiers & MOD_WIN)     s += L"Win+";
    UINT scan = MapVirtualKeyW(hk.vk, MAPVK_VK_TO_VSC);
    wchar_t name[64] = L"";
    LONG lparam = (LONG)(scan << 16);
    if (GetKeyNameTextW(lparam, name, 64) > 0) s += name;
    else { wchar_t buf[8]; wsprintfW(buf, L"%c", (wchar_t)hk.vk); s += buf; }
    return s;
}

MuteSettings LoadMuteSettings() {
    MuteSettings ms;
    std::wifstream f(MuteSettingsPath().c_str());
    if (!f.is_open()) return ms;
    unsigned long mods = 0, vk = 0, enabled = 1;
    if (f >> mods >> vk >> enabled) {
        if (vk != 0) { ms.modifiers = (UINT)mods; ms.vk = (UINT)vk; }
        ms.enabled = (enabled != 0);
    }
    return ms;
}
void SaveMuteSettings(const MuteSettings& ms) {
    std::wstring target = MuteSettingsPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (unsigned long)ms.modifiers << L" "
          << (unsigned long)ms.vk << L" "
          << (ms.enabled ? 1 : 0) << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring MuteHotkeyToString(const MuteSettings& ms) {
    HotkeySettings hk;
    hk.modifiers = ms.modifiers;
    hk.vk = ms.vk;
    return HotkeyToString(hk);
}

ControllerSettings LoadControllerSettings() {
    ControllerSettings cs;
    std::wifstream f(ControllerSettingsPath().c_str());
    if (!f.is_open()) return cs;
    unsigned long enabled = 0, btn = 0, toggle = 1;
    if (f >> enabled >> btn >> toggle) {
        cs.enabled = (enabled != 0);
        if (btn != 0) cs.openRadialButton = (DWORD)btn;
        cs.toggleMode = (toggle != 0);
        unsigned long allowDuringGame = 0;
        if (f >> allowDuringGame) {
            cs.allowControllerDuringGame = (allowDuringGame != 0);
        }
    }
    return cs;
}
void SaveControllerSettings(const ControllerSettings& cs) {
    std::wstring target = ControllerSettingsPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (cs.enabled ? 1 : 0) << L" "
          << (unsigned long)cs.openRadialButton << L" "
          << (cs.toggleMode ? 1 : 0) << L" "
          << (cs.allowControllerDuringGame ? 1 : 0) << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring ControllerButtonName(DWORD button, Lang lang) {
    bool en = (lang == Lang::EN);
    switch (button) {
        case 0x1000: return en ? L"A"              : L"زر A";
        case 0x2000: return en ? L"B"              : L"زر B";
        case 0x4000: return en ? L"X"              : L"زر X";
        case 0x8000: return en ? L"Y"              : L"زر Y";
        case 0x0010: return en ? L"Start"          : L"Start";
        case 0x0020: return en ? L"Back"           : L"Back";
        case 0x0100: return en ? L"Left Shoulder"  : L"LB";
        case 0x0200: return en ? L"Right Shoulder" : L"RB";
        case 0x0040: return en ? L"Left Thumb"     : L"LS";
        case 0x0080: return en ? L"Right Thumb"    : L"RS";
        case 0x0001: return en ? L"D-Pad Up"       : L"↑ D-Pad";
        case 0x0002: return en ? L"D-Pad Down"     : L"↓ D-Pad";
        case 0x0004: return en ? L"D-Pad Left"     : L"← D-Pad";
        case 0x0008: return en ? L"D-Pad Right"    : L"→ D-Pad";
        case 0x0400: return en ? L"Xbox (Guide)"   : L"زر Xbox (الشعار)";
        default:     return L"?";
    }
}

OverlaySettings LoadOverlaySettings() {
    OverlaySettings os;
    std::wifstream f(OverlaySettingsPath().c_str());
    if (!f.is_open()) return os;
    unsigned long enabled = 0, mods = 0, vk = 0, showOnLaunch = 0, px = 20, py = 20;
    float op = 0.85f;
    if (f >> enabled >> mods >> vk >> showOnLaunch >> px >> py >> op) {
        os.enabled = (enabled != 0);
        if (mods != 0 || vk != 0) { os.hotkeyModifiers = (UINT)mods; os.hotkeyVk = (UINT)vk; }
        os.showOnGameLaunch = (showOnLaunch != 0);
        os.posX = (int)px;
        os.posY = (int)py;
        if (op < 0.2f) op = 0.2f;
        if (op > 1.0f) op = 1.0f;
        os.opacity = op;
    }
    return os;
}
void SaveOverlaySettings(const OverlaySettings& os) {
    std::wstring target = OverlaySettingsPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (os.enabled ? 1 : 0) << L" "
          << (unsigned long)os.hotkeyModifiers << L" "
          << (unsigned long)os.hotkeyVk << L" "
          << (os.showOnGameLaunch ? 1 : 0) << L" "
          << os.posX << L" "
          << os.posY << L" "
          << os.opacity << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring OverlayHotkeyToString(const OverlaySettings& os) {
    HotkeySettings hk;
    hk.modifiers = os.hotkeyModifiers;
    hk.vk = os.hotkeyVk;
    return HotkeyToString(hk);
}

std::wstring PerformanceStorageDir() {
    std::wstring dir = GetAppDataDir() + L"\\performance";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

std::wstring ComputeGameStorageHash(const std::wstring& gamePath) {
    ULONGLONG hash = 14695981039346656037ULL;
    for (wchar_t c : gamePath) {
        wchar_t lc = (wchar_t)towlower(c);
        hash ^= (ULONGLONG)(lc & 0xFF);
        hash *= 1099511628211ULL;
        hash ^= (ULONGLONG)((lc >> 8) & 0xFF);
        hash *= 1099511628211ULL;
    }
    wchar_t buf[32];
    wsprintfW(buf, L"%016llX", hash);
    return buf;
}

std::wstring PerformanceGameDir(const std::wstring& gamePath) {
    std::wstring dir = PerformanceStorageDir() + L"\\" + ComputeGameStorageHash(gamePath);
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

bool LoadPerformanceGlobalEnabled() {
    std::wifstream f(PerformanceSettingsPath().c_str());
    if (!f.is_open()) return false;
    std::wstring line; std::getline(f, line);
    return TrimString(line) == L"1";
}

void SavePerformanceGlobalEnabled(bool enabled) {
    std::wstring target = PerformanceSettingsPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (enabled ? L"1" : L"0") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

int ProcessPriorityCount() { return 6; }
ProcessPriority ProcessPriorityFromIndex(int idx) {
    if (idx < 0 || idx > 5) return ProcessPriority::Normal;
    return (ProcessPriority)idx;
}
std::wstring ProcessPriorityName(ProcessPriority p, Lang lang) {
    bool en = (lang == Lang::EN);
    switch (p) {
        case ProcessPriority::Idle:        return en ? L"Idle"          : L"خامل (Idle)";
        case ProcessPriority::BelowNormal: return en ? L"Below Normal"  : L"أقل من الطبيعي";
        case ProcessPriority::Normal:      return en ? L"Normal"        : L"طبيعي";
        case ProcessPriority::AboveNormal: return en ? L"Above Normal"  : L"أعلى من الطبيعي";
        case ProcessPriority::High:        return en ? L"High"          : L"عالية";
        case ProcessPriority::Realtime:    return en ? L"Realtime"      : L"زمن حقيقي";
        default: return L"";
    }
}

bool IsExecutablePath(const std::wstring& path) {
    if (path.size() < 4) return false;
    std::wstring ext = path.substr(path.size() - 4);
    for (auto& c : ext) c = (wchar_t)towlower(c);
    return ext == L".exe";
}
bool IsUriLike(const std::wstring& path) { return path.find(L"://") != std::wstring::npos; }

std::wstring TrimString(const std::wstring& in) {
    size_t start = in.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = in.find_last_not_of(L" \t\r\n");
    return in.substr(start, end - start + 1);
}

Lang LoadLanguage() {
    std::wifstream f(LanguagePath().c_str());
    if (!f.is_open()) return Lang::AR;
    std::wstring line; std::getline(f, line);
    return (TrimString(line) == L"en") ? Lang::EN : Lang::AR;
}
void SaveLanguage(Lang lang) {
    std::wstring target = LanguagePath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (lang == Lang::EN ? L"en" : L"ar") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

std::wstring AutoLayoutSwitchPath() { return GetAppDataDir() + L"\\autolayout.cfg"; }
bool LoadAutoLayoutSwitch() {
    std::wifstream f(AutoLayoutSwitchPath().c_str());
    if (!f.is_open()) return false;
    std::wstring line; std::getline(f, line);
    return TrimString(line) == L"1";
}
void SaveAutoLayoutSwitch(bool enabled) {
    std::wstring target = AutoLayoutSwitchPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (enabled ? L"1" : L"0") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool IsEngineRunning() {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
    if (h) { CloseHandle(h); return true; }
    return false;
}

bool IsUwpPath(const std::wstring& path) {
    return path.size() >= 4 && path.compare(0, 4, L"uwp:") == 0;
}
std::wstring MakeUwpPath(const std::wstring& aumid) { return L"uwp:" + aumid; }
std::wstring GetUwpAumid(const std::wstring& path) {
    return IsUwpPath(path) ? path.substr(4) : L"";
}

bool ActivateUwpApp(const std::wstring& aumid) {
    if (aumid.empty()) return false;
    IApplicationActivationManager* mgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                   CLSCTX_LOCAL_SERVER, IID_PPV_ARGS(&mgr));
    if (FAILED(hr) || !mgr) return false;
    DWORD pid = 0;
    hr = mgr->ActivateApplication(aumid.c_str(), nullptr, AO_NONE, &pid);
    mgr->Release();
    return SUCCEEDED(hr);
}

HICON ExtractUwpIcon(const std::wstring& aumid, int sizePx) {
    if (aumid.empty() || sizePx <= 0) return nullptr;
    std::wstring shellPath = L"shell:AppsFolder\\" + aumid;

    auto extractFromFactory = [&](IShellItemImageFactory* factory) -> HICON {
        if (!factory) return nullptr;
        HBITMAP hbmp = nullptr;
        SIZE sz = { sizePx, sizePx };
        HRESULT hr = factory->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
        if (FAILED(hr) || !hbmp) hr = factory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hbmp);
        if (FAILED(hr) || !hbmp) return nullptr;
        ICONINFO ii = {};
        ii.fIcon = TRUE;
        ii.hbmColor = hbmp;
        ii.hbmMask = CreateBitmap(sizePx, sizePx, 1, 1, nullptr);
        HICON hicon = CreateIconIndirect(&ii);
        DeleteObject(hbmp);
        if (ii.hbmMask) DeleteObject(ii.hbmMask);
        return hicon;
    };

    {
        PIDLIST_ABSOLUTE pidl = nullptr;
        SFGAOF attrs = 0;
        HRESULT hr = SHParseDisplayName(shellPath.c_str(), nullptr, &pidl, 0, &attrs);
        if (SUCCEEDED(hr) && pidl) {
            IShellItemImageFactory* factory = nullptr;
            hr = SHCreateItemFromIDList(pidl, IID_PPV_ARGS(&factory));
            CoTaskMemFree(pidl);
            if (SUCCEEDED(hr) && factory) {
                HICON h = extractFromFactory(factory);
                factory->Release();
                if (h) return h;
            }
        }
    }
    {
        IShellItemImageFactory* factory = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(shellPath.c_str(), nullptr, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr) && factory) {
            HICON h = extractFromFactory(factory);
            factory->Release();
            if (h) return h;
        }
    }
    return nullptr;
}

std::wstring DefaultUwpName(const std::wstring& aumid) {
    std::wstring base = aumid;
    size_t under = base.find(L'_');
    if (under != std::wstring::npos) base = base.substr(0, under);
    size_t bang = base.find(L'!');
    if (bang != std::wstring::npos) base = base.substr(0, bang);
    std::wstring out;
    bool first = true;
    for (size_t i = 0; i < base.size(); i++) {
        if (base[i] == L'.') {
            if (first) { first = false; continue; }
            out += L' ';
        } else out += base[i];
    }
    return out.empty() ? base : out;
}

HICON ExtractHighQualityIcon(const std::wstring& path, int index, int desiredSizePx) {
    if (IsUwpPath(path)) return ExtractUwpIcon(GetUwpAumid(path), desiredSizePx);
    if (path.empty() || IsUriLike(path)) return nullptr;
    HICON hicon = nullptr;
    UINT n = PrivateExtractIconsW(path.c_str(), index, desiredSizePx, desiredSizePx,
                                  &hicon, nullptr, 1, 0);
    if (n == 0 || n == (UINT)-1 || !hicon) {
        HICON hLarge = nullptr, hSmall = nullptr;
        ExtractIconExW(path.c_str(), index, &hLarge, &hSmall, 1);
        if (hSmall) DestroyIcon(hSmall);
        return hLarge;
    }
    return hicon;
}

ShortcutInfo ResolveShortcutFull(const std::wstring& lnkPath) {
    ShortcutInfo info;
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hrInit == S_OK || hrInit == S_FALSE);
    IShellLinkW* psl = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_IShellLinkW, (void**)&psl))) {
        IPersistFile* ppf = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf))) {
            if (SUCCEEDED(ppf->Load(lnkPath.c_str(), STGM_READ))) {
                wchar_t buf[MAX_PATH * 4] = L"";
                if (SUCCEEDED(psl->GetPath(buf, ARRAYSIZE(buf), nullptr, 0)))
                    info.target = buf;
                if (info.target.empty()) {
                    wchar_t raw[MAX_PATH * 4] = L"";
                    if (SUCCEEDED(psl->GetPath(raw, ARRAYSIZE(raw), nullptr, SLGP_RAWPATH)))
                        info.target = raw;
                }
                wchar_t argsBuf[4096] = L"";
                if (SUCCEEDED(psl->GetArguments(argsBuf, ARRAYSIZE(argsBuf))))
                    info.arguments = argsBuf;
                wchar_t iconBuf[MAX_PATH * 2] = L""; int idx = 0;
                if (SUCCEEDED(psl->GetIconLocation(iconBuf, ARRAYSIZE(iconBuf), &idx))) {
                    info.iconPath = iconBuf; info.iconIndex = idx;
                }
                info.ok = true;
                IPropertyStore* pps = nullptr;
                if (SUCCEEDED(psl->QueryInterface(IID_PPV_ARGS(&pps))) && pps) {
                    PROPVARIANT pv;
                    PropVariantInit(&pv);
                    if (SUCCEEDED(pps->GetValue(PKEY_AppUserModel_ID, &pv))) {
                        wchar_t idBuf[512] = L"";
                        if (SUCCEEDED(PropVariantToString(pv, idBuf, 512)) && idBuf[0] != L'\0')
                            info.aumid = idBuf;
                    }
                    PropVariantClear(&pv);
                    pps->Release();
                }
            }
            ppf->Release();
        }
        psl->Release();
    }
    if (info.aumid.empty() && !lnkPath.empty()) {
        IShellItem* psi = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(lnkPath.c_str(), nullptr,
                                                   IID_PPV_ARGS(&psi))) && psi) {
            const SIGDN types[] = { SIGDN_DESKTOPABSOLUTEPARSING, SIGDN_PARENTRELATIVEPARSING, SIGDN_NORMALDISPLAY };
            for (auto t : types) {
                if (!info.aumid.empty()) break;
                LPWSTR dn = nullptr;
                if (SUCCEEDED(psi->GetDisplayName(t, &dn)) && dn) {
                    std::wstring s = dn;
                    CoTaskMemFree(dn);
                    size_t af = FindCaseInsensitive(s, L"AppsFolder\\");
                    if (af != std::wstring::npos) {
                        af += 11;
                        size_t end = s.find_first_of(L"\\/\"", af);
                        std::wstring cand = (end == std::wstring::npos) ? s.substr(af) : s.substr(af, end - af);
                        if (!cand.empty() && cand.find(L'!') != std::wstring::npos) { info.aumid = cand; break; }
                    }
                    size_t bang = s.find(L"!App");
                    if (bang != std::wstring::npos) {
                        size_t start = bang;
                        while (start > 0 && s[start - 1] != L' ' && s[start - 1] != L'"' && s[start - 1] != L'\\') start--;
                        std::wstring cand = s.substr(start, bang + 4 - start);
                        if (cand.find(L'_') != std::wstring::npos) { info.aumid = cand; break; }
                    }
                }
            }
            psi->Release();
        }
    }
    if (needUninit) CoUninitialize();
    return info;
}

std::wstring ResolveShortcut(const std::wstring& path) {
    if (path.size() < 4) return path;
    std::wstring ext = path.substr(path.size() - 4);
    for (auto& c : ext) c = (wchar_t)towlower(c);
    if (ext != L".lnk") return path;
    ShortcutInfo info = ResolveShortcutFull(path);
    return (info.ok && !info.target.empty()) ? info.target : path;
}

static std::wstring ExtractProtocolUri(const std::wstring& s) {
    std::wstring t = TrimString(s);
    if (t.size() >= 2 && t.front() == L'"' && t.back() == L'"')
        t = t.substr(1, t.size() - 2);
    size_t search = 0;
    while (search < t.size()) {
        size_t p = t.find(L"://", search);
        if (p == std::wstring::npos) return L"";
        if (p == 0) { search = p + 3; continue; }
        size_t start = p;
        while (start > 0 && t[start - 1] != L' ' && t[start - 1] != L'\t' && t[start - 1] != L'"') start--;
        if (start < t.size() && t[start] == L'"') start++;
        std::wstring proto = t.substr(start, p - start);
        bool valid = !proto.empty();
        for (wchar_t c : proto) {
            if (!iswalnum(c) && c != L'+' && c != L'-' && c != L'.') { valid = false; break; }
        }
        if (valid) {
            std::wstring lower = ToLowerCopy(proto);
            if (lower != L"file") {
                size_t end = p + 3;
                while (end < t.size() && t[end] != L' ' && t[end] != L'\t' && t[end] != L'"') end++;
                return t.substr(start, end - start);
            }
        }
        search = p + 3;
    }
    return L"";
}

std::wstring GetFileNameFromPath(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

static std::wstring ExtractSteamUrlFromArgs(const std::wstring& args) {
    size_t p = args.find(L"steam://");
    if (p != std::wstring::npos) {
        size_t end = args.find(L' ', p);
        return (end == std::wstring::npos) ? args.substr(p) : args.substr(p, end - p);
    }
    size_t ap = args.find(L"-applaunch");
    if (ap != std::wstring::npos) {
        size_t numStart = ap + 10;
        while (numStart < args.size() && args[numStart] == L' ') numStart++;
        size_t numEnd = numStart;
        while (numEnd < args.size() && iswdigit(args[numEnd])) numEnd++;
        if (numEnd > numStart) return L"steam://rungameid/" + args.substr(numStart, numEnd - numStart);
    }
    return L"";
}

static std::wstring TryExtractAumidFromShortcut(const ShortcutInfo& si) {
    if (!si.aumid.empty()) return si.aumid;
    std::wstring combined = si.target + L" " + si.arguments;
    size_t p = FindCaseInsensitive(combined, L"shell:AppsFolder\\");
    if (p != std::wstring::npos) {
        p += 17;
        size_t end = combined.find_first_of(L" \"", p);
        std::wstring cand = TrimString((end == std::wstring::npos) ? combined.substr(p) : combined.substr(p, end - p));
        if (!cand.empty()) return cand;
    }
    size_t bang = combined.find(L"!App");
    if (bang != std::wstring::npos) {
        size_t start = bang;
        while (start > 0 && combined[start - 1] != L' ' && combined[start - 1] != L'"' && combined[start - 1] != L'\\') start--;
        std::wstring candidate = combined.substr(start, bang + 4 - start);
        if (candidate.find(L'_') != std::wstring::npos) return candidate;
    }
    return L"";
}

ResolvedDrop ResolveDroppedPath(const std::wstring& rawPath) {
    ResolvedDrop r;
    std::wstring path = TrimString(rawPath);
    if (path.size() >= 2 && path.front() == L'"' && path.back() == L'"')
        path = path.substr(1, path.size() - 2);
    std::wstring lowerExt = path.size() >= 4 ? ToLowerCopy(path.substr(path.size() - 4)) : L"";

    if (lowerExt == L".exe") {
        r.launchPath = path; r.iconPath = path; r.iconIndex = 0; r.valid = true; return r;
    }
    if (lowerExt == L".lnk") {
        ShortcutInfo si = ResolveShortcutFull(path);
        if (!si.ok) return r;
        std::wstring aumid = TryExtractAumidFromShortcut(si);
        if (!aumid.empty()) {
            r.launchPath = MakeUwpPath(aumid);
            r.iconPath = !si.iconPath.empty() ? si.iconPath : path;
            r.iconIndex = si.iconIndex;
            r.valid = true;
            return r;
        }
        std::wstring protoUri = ExtractProtocolUri(si.target);
        if (protoUri.empty()) protoUri = ExtractProtocolUri(si.arguments);
        if (!protoUri.empty()) {
            r.launchPath = protoUri;
            r.iconPath = !si.iconPath.empty() ? si.iconPath : path;
            r.iconIndex = si.iconIndex;
            r.valid = true;
            return r;
        }
        std::wstring lowerTarget = ToLowerCopy(si.target);
        if (lowerTarget.find(L"steam.exe") != std::wstring::npos && !si.arguments.empty()) {
            std::wstring steamUrl = ExtractSteamUrlFromArgs(si.arguments);
            if (!steamUrl.empty()) {
                r.launchPath = steamUrl;
                r.iconPath = !si.iconPath.empty() ? si.iconPath : si.target;
                r.iconIndex = si.iconIndex;
                r.valid = true;
                return r;
            }
        }
        if (!si.target.empty() && lowerTarget.find(L"explorer.exe") == std::wstring::npos) {
            r.launchPath = si.target;
            r.iconPath = !si.iconPath.empty() ? si.iconPath : si.target;
            r.iconIndex = si.iconIndex;
            r.valid = true;
            return r;
        }
        r.launchPath = path; r.iconPath = path; r.iconIndex = 0; r.valid = true;
        return r;
    }
    if (lowerExt == L".url") {
        std::wifstream f(path.c_str());
        if (!f.is_open()) return r;
        std::wstring line, url, iconFile;
        int iconIndex = 0;
        while (std::getline(f, line)) {
            line = TrimString(line);
            if (line.size() >= 4 && ToLowerCopy(line.substr(0, 4)) == L"url=") url = TrimString(line.substr(4));
            else if (line.size() >= 9 && ToLowerCopy(line.substr(0, 9)) == L"iconfile=") iconFile = TrimString(line.substr(9));
            else if (line.size() >= 10 && ToLowerCopy(line.substr(0, 10)) == L"iconindex=") iconIndex = _wtoi(TrimString(line.substr(10)).c_str());
        }
        if (url.empty()) return r;
        r.launchPath = url; r.iconPath = iconFile; r.iconIndex = iconIndex; r.valid = true;
        return r;
    }
    return r;
}

static bool IsNoiseExe(const std::wstring& lowerBase) {
    static const wchar_t* kNoise[] = {
        L"unins", L"setup", L"install", L"vcredist", L"redist",
        L"crashhandler", L"crashpad", L"crashreport", L"helper",
        L"updater", L"bugreport", L"diagnostic", L"uninstall",
        L"vc_redist", L"directx", L"dxsetup", L"dotnet"
    };
    for (auto n : kNoise) if (lowerBase.find(n) != std::wstring::npos) return true;
    return false;
}
static bool IsNoiseFolder(const std::wstring& lowerName) {
    static const wchar_t* kNoise[] = {
        L"node_modules", L"redist", L"vcredist", L"__redist", L"directx",
        L"dotnet", L"windowsapps", L"$recycle.bin", L"system volume information",
        L"temp", L"tmp", L"cache", L"backup"
    };
    for (auto n : kNoise) if (lowerName == n) return true;
    return false;
}

static void WalkDir(const std::wstring& dir, int depth, const std::wstring& folderLower,
                    std::vector<ExeScanResult>& out, int maxCount) {
    if (depth < 0 || (int)out.size() >= maxCount) return;
    WIN32_FIND_DATAW fd;
    std::wstring pattern = dir + L"\\*";
    HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    std::vector<std::wstring> subDirs;
    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring full = dir + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            std::wstring sub = ToLowerCopy(fd.cFileName);
            if (IsNoiseFolder(sub)) continue;
            subDirs.push_back(full);
        } else {
            std::wstring name = fd.cFileName;
            if (name.size() >= 4 && ToLowerCopy(name.substr(name.size() - 4)) == L".exe") {
                std::wstring base = name.substr(0, name.size() - 4);
                std::wstring baseLower = ToLowerCopy(base);
                if (IsNoiseExe(baseLower)) continue;
                ExeScanResult r;
                r.path = full;
                r.size = (((ULONGLONG)fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                r.looksLikeMain = !folderLower.empty() &&
                    (baseLower == folderLower ||
                     baseLower.find(folderLower) != std::wstring::npos ||
                     folderLower.find(baseLower) != std::wstring::npos);
                out.push_back(r);
                if ((int)out.size() >= maxCount) break;
            }
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (auto& sub : subDirs) {
        if ((int)out.size() >= maxCount) break;
        WalkDir(sub, depth - 1, folderLower, out, maxCount);
    }
}

void FindAllExesInFolder(const std::wstring& folderPath, std::vector<ExeScanResult>& out,
                         int maxDepth, int maxCount) {
    out.clear();
    std::wstring folderName = GetFileNameFromPath(folderPath);
    std::wstring folderLower = ToLowerCopy(folderName);
    WalkDir(folderPath, maxDepth, folderLower, out, maxCount);
    std::sort(out.begin(), out.end(), [](const ExeScanResult& a, const ExeScanResult& b) {
        if (a.looksLikeMain != b.looksLikeMain) return a.looksLikeMain;
        return a.size > b.size;
    });
}

FolderExeSuggestion FindBestExeInFolder(const std::wstring& folderPath) {
    FolderExeSuggestion result;
    std::vector<ExeScanResult> all;
    FindAllExesInFolder(folderPath, all, 4, 100);
    if (!all.empty()) { result.exePath = all[0].path; result.found = true; }
    return result;
}

// ✅ 20 نمط رسم
int RadialStyleCount() { return 20; }
RadialStyle LoadRadialStyle() {
    std::wifstream f(ThemePath().c_str());
    if (!f.is_open()) return RadialStyle::Outline;
    int val = -1;
    if (f >> val && val >= 0 && val < RadialStyleCount()) return (RadialStyle)val;
    return RadialStyle::Outline;
}
void SaveRadialStyle(RadialStyle style) {
    std::wstring target = ThemePath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (int)style << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring RadialStyleName(RadialStyle style, Lang lang) {
    bool en = (lang == Lang::EN);
    switch (style) {
        case RadialStyle::Outline:  return en ? L"Outline Ring"     : L"حلقة محيطية";
        case RadialStyle::Neon:     return en ? L"Neon Glow"        : L"توهّج نيون";
        case RadialStyle::Gradient: return en ? L"Gradient Ring"    : L"حلقة متدرجة";
        case RadialStyle::Minimal:  return en ? L"Minimal"          : L"بسيط";
        case RadialStyle::Hex:      return en ? L"Hex Frame"        : L"إطار سداسي";
        case RadialStyle::Comet:    return en ? L"Comet Arc"        : L"قوس مذنّب";
        case RadialStyle::Burst:    return en ? L"Burst (Action)"   : L"انفجاري (أكشن)";
        case RadialStyle::Sakura:   return en ? L"Sakura (Anime)"   : L"ساكورا (أنمي)";
        case RadialStyle::Glass:    return en ? L"Frosted Glass"    : L"زجاج مصنفر";
        case RadialStyle::Vortex:   return en ? L"Vortex"           : L"دوامة";
        case RadialStyle::Simple:   return en ? L"Clean Ring"       : L"حلقة نظيفة";
        case RadialStyle::Aurora:   return en ? L"Aurora Glow"      : L"توهّج الشفق";
        case RadialStyle::Halo:     return en ? L"HUD Halo"         : L"هالة تقنية";
        case RadialStyle::Ripple:   return en ? L"Ripple Waves"     : L"موجات متداخلة";
        case RadialStyle::Crystal:  return en ? L"Crystal Facet"    : L"بلّورة";
        case RadialStyle::Plasma:   return en ? L"Plasma Field"     : L"حقل بلازما";
        case RadialStyle::Orbit:    return en ? L"Orbit Ring"       : L"حلقة مدارية";
        case RadialStyle::Pixel:    return en ? L"Pixel Frame"      : L"إطار بكسل";
        case RadialStyle::Circuit:  return en ? L"Circuit Board"    : L"لوحة إلكترونية";
        case RadialStyle::Flame:    return en ? L"Flame"            : L"لهب";
        default: return L"";
    }
}

float LoadRadialTransparency() {
    std::wifstream f(RadialTransparencyPath().c_str());
    if (!f.is_open()) return 0.72f;
    float v = 0.72f;
    if (f >> v) {
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        return v;
    }
    return 0.72f;
}
void SaveRadialTransparency(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    std::wstring target = RadialTransparencyPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << t << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

std::wstring LoadPanelPreset() {
    std::wifstream f(PanelPresetPath().c_str());
    if (!f.is_open()) return L"purple";
    std::wstring line; std::getline(f, line);
    line = TrimString(line);
    return line.empty() ? L"purple" : line;
}
void SavePanelPreset(const std::wstring& preset) {
    std::wstring target = PanelPresetPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << preset << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

static const wchar_t* AUTOSTART_RUN_KEY = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const wchar_t* AUTOSTART_VALUE_NAME = L"GameLauncher";

static bool RunKeyIsEnabled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS) return false;
    DWORD type = 0;
    LONG res = RegQueryValueExW(hKey, AUTOSTART_VALUE_NAME, nullptr, &type, nullptr, nullptr);
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS && type == REG_SZ;
}
static bool RunKeyEnable() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) return false;
    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) { RegCloseKey(hKey); return false; }
    std::wstring quoted = L"\"" + std::wstring(exePath) + L"\"";
    LONG res = RegSetValueExW(hKey, AUTOSTART_VALUE_NAME, 0, REG_SZ,
        (const BYTE*)quoted.c_str(), (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return res == ERROR_SUCCESS;
}
static void RunKeyDisable() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS) return;
    RegDeleteValueW(hKey, AUTOSTART_VALUE_NAME);
    RegCloseKey(hKey);
}

static HRESULT TaskSchedulerEnable() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HRESULT hr = S_OK;
    ITaskService* pService = nullptr;
    hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ITaskService, (void**)&pService);
    if (FAILED(hr) || !pService) return hr;

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) { pService->Release(); return hr; }

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) { pService->Release(); return hr; }

    {
        ITaskFolder* pSubFolder = nullptr;
        hr = pRootFolder->GetFolder(_bstr_t(TASK_FOLDER), &pSubFolder);
        if (FAILED(hr) || !pSubFolder) {
            VARIANT vEmpty;
            VariantInit(&vEmpty);
            hr = pRootFolder->CreateFolder(_bstr_t(TASK_FOLDER), vEmpty, &pSubFolder);
            if (FAILED(hr)) { pRootFolder->Release(); pService->Release(); return hr; }
        }
        if (pSubFolder) pSubFolder->Release();
    }

    {
        ITaskFolder* pSubFolder = nullptr;
        hr = pRootFolder->GetFolder(_bstr_t(TASK_FOLDER), &pSubFolder);
        if (SUCCEEDED(hr) && pSubFolder) {
            pSubFolder->DeleteTask(_bstr_t(TASK_NAME), 0);
            pSubFolder->Release();
        }
    }

    ITaskDefinition* pTask = nullptr;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) { pRootFolder->Release(); pService->Release(); return hr; }

    IRegistrationInfo* pRegInfo = nullptr;
    if (SUCCEEDED(pTask->get_RegistrationInfo(&pRegInfo)) && pRegInfo) {
        pRegInfo->put_Author(_bstr_t(TASK_AUTHOR));
        pRegInfo->put_Description(_bstr_t(L"Auto-start GameLauncher on user logon."));
        pRegInfo->Release();
    }

    ITaskSettings* pSettings = nullptr;
    if (SUCCEEDED(pTask->get_Settings(&pSettings)) && pSettings) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT0S"));
        pSettings->put_MultipleInstances(TASK_INSTANCES_IGNORE_NEW);
        pSettings->Release();
    }

    ITriggerCollection* pTriggers = nullptr;
    hr = pTask->get_Triggers(&pTriggers);
    if (FAILED(hr)) { pTask->Release(); pRootFolder->Release(); pService->Release(); return hr; }

    ITrigger* pTrigger = nullptr;
    hr = pTriggers->Create(TASK_TRIGGER_LOGON, &pTrigger);
    if (FAILED(hr)) { pTriggers->Release(); pTask->Release(); pRootFolder->Release(); pService->Release(); return hr; }

    ILogonTrigger* pLogonTrigger = nullptr;
    if (SUCCEEDED(pTrigger->QueryInterface(IID_ILogonTrigger, (void**)&pLogonTrigger)) && pLogonTrigger) {
        pLogonTrigger->put_Id(_bstr_t(L"LogonTrigger"));
        pLogonTrigger->put_Delay(_bstr_t(L"PT15S"));
        pLogonTrigger->Release();
    }
    pTrigger->Release();
    pTriggers->Release();

    IActionCollection* pActions = nullptr;
    hr = pTask->get_Actions(&pActions);
    if (FAILED(hr)) { pTask->Release(); pRootFolder->Release(); pService->Release(); return hr; }

    IAction* pAction = nullptr;
    hr = pActions->Create(TASK_ACTION_EXEC, &pAction);
    if (FAILED(hr)) { pActions->Release(); pTask->Release(); pRootFolder->Release(); pService->Release(); return hr; }

    IExecAction* pExecAction = nullptr;
    if (SUCCEEDED(pAction->QueryInterface(IID_IExecAction, (void**)&pExecAction)) && pExecAction) {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        pExecAction->put_Path(_bstr_t(exePath));
        pExecAction->Release();
    }
    pAction->Release();
    pActions->Release();

    ITaskFolder* pSubFolder = nullptr;
    hr = pRootFolder->GetFolder(_bstr_t(TASK_FOLDER), &pSubFolder);
    if (FAILED(hr)) { pTask->Release(); pRootFolder->Release(); pService->Release(); return hr; }

    IRegisteredTask* pRegistered = nullptr;
    VARIANT vEmpty; VariantInit(&vEmpty);
    hr = pSubFolder->RegisterTaskDefinition(
        _bstr_t(TASK_NAME),
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(),
        _variant_t(),
        TASK_LOGON_INTERACTIVE_TOKEN,
        vEmpty,
        &pRegistered);

    if (pRegistered) pRegistered->Release();
    pSubFolder->Release();
    pTask->Release();
    pRootFolder->Release();
    pService->Release();
    return hr;
}

static bool TaskSchedulerDisable() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ITaskService* pService = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITaskService, (void**)&pService);
    if (FAILED(hr) || !pService) return false;

    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) { pService->Release(); return false; }

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) { pService->Release(); return false; }

    ITaskFolder* pSubFolder = nullptr;
    hr = pRootFolder->GetFolder(_bstr_t(TASK_FOLDER), &pSubFolder);
    if (SUCCEEDED(hr) && pSubFolder) {
        pSubFolder->DeleteTask(_bstr_t(TASK_NAME), 0);
        pSubFolder->Release();
    }
    pRootFolder->Release();
    pService->Release();
    return true;
}

static bool TaskSchedulerIsEnabled() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ITaskService* pService = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ITaskService, (void**)&pService);
    if (FAILED(hr) || !pService) return false;
    hr = pService->Connect(_variant_t(), _variant_t(), _variant_t(), _variant_t());
    if (FAILED(hr)) { pService->Release(); return false; }
    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(L"\\"), &pRootFolder);
    if (FAILED(hr)) { pService->Release(); return false; }
    ITaskFolder* pSubFolder = nullptr;
    hr = pRootFolder->GetFolder(_bstr_t(TASK_FOLDER), &pSubFolder);
    if (FAILED(hr) || !pSubFolder) { pRootFolder->Release(); pService->Release(); return false; }
    IRegisteredTask* pTask = nullptr;
    hr = pSubFolder->GetTask(_bstr_t(TASK_NAME), &pTask);
    bool exists = SUCCEEDED(hr) && pTask != nullptr;
    if (pTask) pTask->Release();
    pSubFolder->Release();
    pRootFolder->Release();
    pService->Release();
    return exists;
}

bool IsAutoStartEnabled() {
    if (TaskSchedulerIsEnabled()) return true;
    return RunKeyIsEnabled();
}

void SetAutoStartEnabled(bool enabled) {
    if (enabled) {
        HRESULT hr = TaskSchedulerEnable();
        if (SUCCEEDED(hr)) {
            RunKeyDisable();
        } else {
            RunKeyEnable();
        }
    } else {
        TaskSchedulerDisable();
        RunKeyDisable();
    }
}

bool TestAutoStart(std::wstring& outDiagnostic) {
    std::wstring exePath;
    {
        wchar_t buf[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, buf, MAX_PATH)) {
            outDiagnostic = L"تعذر قراءة مسار البرنامج.";
            return false;
        }
        exePath = buf;
    }
    if (GetFileAttributesW(exePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        outDiagnostic = L"ملف البرنامج غير موجود على القرص!";
        return false;
    }

    bool taskExists = TaskSchedulerIsEnabled();
    bool runKeyExists = RunKeyIsEnabled();

    if (!taskExists && !runKeyExists) {
        outDiagnostic = L"الإعداد غير مُفعّل. فعّله أولاً.";
        return false;
    }

    if (taskExists) {
        outDiagnostic = L"✅ الإعداد سليم عبر Task Scheduler — سيعمل عند تسجيل الدخول التالي.";
        return true;
    }
    if (runKeyExists) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, AUTOSTART_RUN_KEY, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            wchar_t value[MAX_PATH * 2] = L"";
            DWORD size = sizeof(value);
            DWORD type = 0;
            LONG res = RegQueryValueExW(hKey, AUTOSTART_VALUE_NAME, nullptr, &type, (LPBYTE)value, &size);
            RegCloseKey(hKey);
            if (res != ERROR_SUCCESS) {
                outDiagnostic = L"⚠️ Run key موجود لكن قراءته فشلت.";
                return false;
            }
            std::wstring val = value;
            if (val.find(exePath) == std::wstring::npos) {
                outDiagnostic = L"⚠️ Run key موجود لكن المسار غير صحيح! فعّل الإعداد من جديد.";
                return false;
            }
            outDiagnostic = L"✅ الإعداد سليم عبر Run key (لكن قد يحجبه ويندوز أحياناً).";
            return true;
        }
    }
    outDiagnostic = L"غير معروف — حاول تفعيل/إلغاء الإعداد.";
    return false;
}

bool PathExistsOnDisk(const std::wstring& path) {
    if (path.empty()) return false;
    if (IsUwpPath(path)) return true;
    if (IsUriLike(path)) return true;
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool IsProcessRunningByExeName(const std::wstring& exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do { if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0) { found = true; break; } }
        while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

static std::wstring LoadSingleLineFile(const std::wstring& path) {
    std::wifstream f(path.c_str());
    if (!f.is_open()) return L"";
    std::wstring line; std::getline(f, line);
    return TrimString(line);
}
static void SaveSingleLineFile(const std::wstring& path, const std::wstring& value) {
    std::wstring tmp = path + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << value << L"\n";
    }
    MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}
std::wstring LoadPanelBackground() {
    std::wstring p = LoadSingleLineFile(PanelBackgroundPath());
    if (!p.empty() && GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) return L"";
    return p;
}
std::wstring LoadRadialBackground() {
    std::wstring p = LoadSingleLineFile(RadialBackgroundPath());
    if (!p.empty() && GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) return L"";
    return p;
}
void SavePanelBackground(const std::wstring& path)  { SaveSingleLineFile(PanelBackgroundPath(), path); }
void SaveRadialBackground(const std::wstring& path) { SaveSingleLineFile(RadialBackgroundPath(), path); }

bool IsFirstRun() {
    return GetFileAttributesW(FirstRunDonePath().c_str()) == INVALID_FILE_ATTRIBUTES;
}

void MarkFirstRunDone() {
    std::wstring target = FirstRunDonePath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (f.is_open()) f << L"1\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool LoadHubAlwaysVisible() {
    std::wifstream f(HubAlwaysVisiblePath().c_str());
    if (!f.is_open()) return false;
    std::wstring line; std::getline(f, line);
    return TrimString(line) == L"1";
}

void SaveHubAlwaysVisible(bool enabled) {
    std::wstring target = HubAlwaysVisiblePath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (enabled ? L"1" : L"0") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool LoadPanelGlassEffect() {
    std::wifstream f(PanelGlassEffectPath().c_str());
    if (!f.is_open()) return false;
    std::wstring line; std::getline(f, line);
    return TrimString(line) == L"1";
}

void SavePanelGlassEffect(bool enabled) {
    std::wstring target = PanelGlassEffectPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (enabled ? L"1" : L"0") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

bool LoadRadialNoGlow() {
    std::wifstream f(RadialNoGlowPath().c_str());
    if (!f.is_open()) return false;
    std::wstring line; std::getline(f, line);
    return TrimString(line) == L"1";
}

void SaveRadialNoGlow(bool enabled) {
    std::wstring target = RadialNoGlowPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        f << (enabled ? L"1" : L"0") << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}

// ✅ اسم اللغة داخل اللعبة
std::wstring LangCodeToLayoutName(int gameLanguage) {
    switch (gameLanguage) {
        case 1: return L"ar";  // العربية
        case 2: return L"en";  // English
        default: return L"";    // بدون
    }
}