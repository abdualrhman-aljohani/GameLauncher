// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// Launcher.cpp
#include "../Common/Config.h"
#include "../Common/Resource.h"
#include "../Common/MuteAudio.h"
#include "../Common/PerformanceMonitor.h"
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlobj.h>
#include <xinput.h>
#include <psapi.h>
#include <mmsystem.h>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <ctime>

static const double PI_CONST = 3.14159265358979323846;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "xinput.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "winmm.lib")

using namespace Gdiplus;

#ifndef XINPUT_GAMEPAD_GUIDE
#define XINPUT_GAMEPAD_GUIDE 0x0400
#endif

static const wchar_t* APP_VERSION = L"1.7.0";

// ✅ قيم مقياس الدائرة (يتم تحميلها من config عند كل فتح للدائرة)
static const int DEFAULT_HUB_RADIUS = 46;
static const int DEFAULT_GAME_RADIUS = 42;
static const int DEFAULT_BASE_ORBIT = 118;
static const int ORBIT_PER_EXTRA = 16;
static const int RADIAL_BG_CACHE_PX = 128;
static const int WINDOW_MARGIN = 70;

static int g_hubRadius = DEFAULT_HUB_RADIUS;
static int g_gameRadius = DEFAULT_GAME_RADIUS;
static int g_baseOrbit = DEFAULT_BASE_ORBIT;

static const wchar_t* HIDDEN_CLASS = HIDDEN_WINDOW_CLASS_NAME;
static const wchar_t* POPUP_CLASS = L"GameLauncherRadialWnd";
static const wchar_t* OVERLAY_CLASS = L"GameLauncherOverlayWnd";

static Lang g_lang = Lang::AR;
#define TXT(ar_text, en_text) (g_lang == Lang::EN ? (en_text) : (ar_text))

#define WM_TRAYICON     (WM_APP + 1)
#define HOTKEY_ID       1
#define MUTE_HOTKEY_ID  2
#define OVERLAY_HOTKEY_ID 3
#define ID_TRAY_SHOW    2001
#define ID_TRAY_PANEL   2002
#define ID_TRAY_EXIT    2003
#define ID_TRAY_MUTE    2004
#define ID_TRAY_OVERLAY 2005

#define ID_MUTED_BASE   3000
#define ID_MUTED_MAX    3099
#define ID_MUTED_ALL    3100

#define WM_APP_CONTROLLER_TOGGLE (WM_APP + 300)

static ULONG_PTR       g_gdiplusToken;
static HINSTANCE       g_hInst;
static HWND            g_hiddenWnd;
static HWND            g_popupWnd = nullptr;
static HWND            g_overlayWnd = nullptr;
static NOTIFYICONDATAW g_nid = {};
static std::vector<GameEntry> g_games;
static std::vector<int>       g_visibleGameIndices;
static HotkeySettings  g_hotkey;
static MuteSettings    g_muteSettings;
static ControllerSettings g_controllerSettings;
static OverlaySettings g_overlaySettings;
static int             g_hover = -2;
static POINT           g_center;
static int             g_orbitDistance = DEFAULT_BASE_ORBIT;
static bool            g_shouldClosePopup = false;

static HANDLE          g_muteEvent = nullptr;
static HANDLE          g_controllerThread = nullptr;
static volatile LONG   g_controllerThreadStop = 0;
static ULONGLONG       g_controllerSuppressUntil = 0;
static bool g_hubAlwaysVisible = false;
static bool g_isFirstRun = false;

static float g_radialTransparency = 0.72f;
static bool g_radialNoGlow = false;

static volatile LONG g_gameRunningCount = 0;

static std::map<std::wstring, bool> g_muteCache;
static std::set<std::wstring>       g_manualMuteUris;

struct CachedIconEntry  { HICON  icon = nullptr; ULONGLONG sig = 0; };
struct CachedRadialBg   { Bitmap* bmp  = nullptr; ULONGLONG sig = 0; };
static std::map<std::wstring, CachedIconEntry> g_iconCache;
static std::map<std::wstring, CachedRadialBg>  g_radialBgCache;

static std::wstring            g_ringBgSourcePath;
static ULONGLONG                g_ringBgSourceSig = 0;
static int                      g_ringBgCachedW = 0, g_ringBgCachedH = 0;
static Bitmap*                  g_ringBgCachedBmp = nullptr;

static std::vector<std::wstring> g_trayMutedExes;

// ============================================================
// ✅ Boost FPS — حالة النظام
// ============================================================
static volatile LONG g_timerResolutionRefCount = 0;
static volatile LONG g_sysRespRefCount = 0;
static volatile LONG g_mmcssRefCount = 0;

// ---- Backup structure ----
struct BoostBackup {
    bool hasSystemResp = false;
    DWORD originalSystemResp = 20;
    bool hasMmcss = false;
    DWORD mmcssGpuPriority = 8;
    DWORD mmcssPriority = 6;
    std::wstring mmcssSchedCategory = L"High";
    std::wstring mmcssSfioPriority = L"High";
};

static std::wstring BoostBackupPath() {
    return GetAppDataDir() + L"\\boost_backup.cfg";
}

static bool SaveBoostBackup(const BoostBackup& b) {
    std::wstring target = BoostBackupPath();
    std::wstring tmp = target + L".tmp";
    std::wofstream f(tmp.c_str(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << L"hasSystemResp=" << (b.hasSystemResp ? 1 : 0) << L"\n";
    f << L"originalSystemResp=" << b.originalSystemResp << L"\n";
    f << L"hasMmcss=" << (b.hasMmcss ? 1 : 0) << L"\n";
    f << L"mmcssGpuPriority=" << b.mmcssGpuPriority << L"\n";
    f << L"mmcssPriority=" << b.mmcssPriority << L"\n";
    f << L"mmcssSchedCategory=" << b.mmcssSchedCategory << L"\n";
    f << L"mmcssSfioPriority=" << b.mmcssSfioPriority << L"\n";
    f.close();
    return MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

static bool LoadBoostBackup(BoostBackup& b) {
    std::wifstream f(BoostBackupPath().c_str());
    if (!f.is_open()) return false;
    std::wstring line;
    while (std::getline(f, line)) {
        size_t eq = line.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring key = line.substr(0, eq);
        std::wstring val = line.substr(eq + 1);
        if (key == L"hasSystemResp") b.hasSystemResp = (val == L"1");
        else if (key == L"originalSystemResp") b.originalSystemResp = (DWORD)_wtoi(val.c_str());
        else if (key == L"hasMmcss") b.hasMmcss = (val == L"1");
        else if (key == L"mmcssGpuPriority") b.mmcssGpuPriority = (DWORD)_wtoi(val.c_str());
        else if (key == L"mmcssPriority") b.mmcssPriority = (DWORD)_wtoi(val.c_str());
        else if (key == L"mmcssSchedCategory") b.mmcssSchedCategory = val;
        else if (key == L"mmcssSfioPriority") b.mmcssSfioPriority = val;
    }
    return true;
}

static void DeleteBoostBackup() {
    DeleteFileW(BoostBackupPath().c_str());
}

typedef DWORD (WINAPI *XInputGetStateEx_t)(DWORD dwUserIndex, XINPUT_STATE* pState);
static XInputGetStateEx_t g_XInputGetStateEx = nullptr;

static void InitXInputEx() {
    HMODULE hXInput = LoadLibraryW(L"xinput1_4.dll");
    if (!hXInput) hXInput = LoadLibraryW(L"xinput1_3.dll");
    if (!hXInput) hXInput = LoadLibraryW(L"xinput9_1_0.dll");
    if (hXInput) {
        g_XInputGetStateEx = (XInputGetStateEx_t)GetProcAddress(hXInput, (LPCSTR)100);
    }
}

static DWORD MyXInputGetState(DWORD i, XINPUT_STATE* state) {
    if (g_XInputGetStateEx) return g_XInputGetStateEx(i, state);
    return XInputGetState(i, state);
}

static void LaunchGame(const GameEntry& g);
static void OpenPanel();
static int  KbNextIndex(int current, int dir);
static void PaintPopup();
static void ShowRadialPopup();
static void ShowOverlay();
static void HideOverlay();
static void UpdateOverlayContent();
static void RepaintOverlay();
static void ReloadOverlayNow();

// ----------------------------- تسجيل تشخيصي -----------------------------
static std::wstring LogPath() { return GetAppDataDir() + L"\\launcher.log"; }

static void WriteLog(const std::wstring& msg) {
    std::wofstream f(LogPath().c_str(), std::ios::app);
    if (!f.is_open()) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[32];
    wsprintfW(ts, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    f << ts << msg << L"\n";
}

static void StartFreshLog() {
    std::wofstream f(LogPath().c_str(), std::ios::trunc);
    if (!f.is_open()) return;
    f << L"=== GameLauncher Diagnostic Log ===\n";
    f << L"Version: " << APP_VERSION << L"\n";
}

static std::wstring GetExeSignature() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return L"unknown";
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad)) return L"unknown";
    wchar_t buf[64];
    wsprintfW(buf, L"%08X%08X-%08X%08X",
              fad.ftLastWriteTime.dwHighDateTime, fad.ftLastWriteTime.dwLowDateTime,
              fad.nFileSizeHigh, fad.nFileSizeLow);
    return buf;
}

static void AutoCleanupOnVersionChange() {
    std::wstring versionFile = GetAppDataDir() + L"\\version.txt";
    std::wstring currentVersion = GetExeSignature();
    std::wstring stored;
    {
        std::wifstream vf(versionFile.c_str());
        if (vf.is_open()) { std::getline(vf, stored); stored = TrimString(stored); }
    }
    if (stored == currentVersion) return;
    WriteLog(L"Version changed: '" + stored + L"' → '" + currentVersion + L"'");

    // ✅ احذف فقط ملفات الواجهة (لكي تُستخرج من الموارد الجديدة)
    // ❌ لا نحذف WebView2Data — لأن ذلك يمسح كاش CDN ويبطئ الفتحة الأولى بلا داعي
    DeleteFileW((GetAppDataDir() + L"\\panel_ui.html").c_str());
    DeleteFileW((GetAppDataDir() + L"\\panel_ui.css").c_str());
    DeleteFileW((GetAppDataDir() + L"\\panel_ui.js").c_str());

    // حدّث رقم الإصدار
    std::wstring tmp = versionFile + L".tmp";
    {
        std::wofstream vf(tmp.c_str(), std::ios::trunc);
        if (vf.is_open()) vf << currentVersion << L"\n";
    }
    MoveFileExW(tmp.c_str(), versionFile.c_str(), MOVEFILE_REPLACE_EXISTING);
}


// ============================================================
// ✅ Boost FPS — تنفيذ التحسينات (مع حفظ + استرجاع)
// ============================================================

// SystemResponsiveness: HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Multimedia\SystemProfile
static bool SetSystemResponsiveness(DWORD value) {
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;

    // ✅ احفظ القيمة الأصلية في الملف عند أول مرة
    BoostBackup backup;
    LoadBoostBackup(backup);
    if (!backup.hasSystemResp) {
        DWORD sz = sizeof(DWORD), type = 0;
        DWORD cur = 20;
        if (RegQueryValueExW(hKey, L"SystemResponsiveness", nullptr, &type,
                             (LPBYTE)&cur, &sz) == ERROR_SUCCESS) {
            backup.originalSystemResp = cur;
        } else {
            backup.originalSystemResp = 20;
        }
        backup.hasSystemResp = true;
        SaveBoostBackup(backup);
        WriteLog(L"[Boost] Saved original SystemResponsiveness=" +
                 std::to_wstring(backup.originalSystemResp));
    }

    DWORD v = value;
    r = RegSetValueExW(hKey, L"SystemResponsiveness", 0, REG_DWORD,
                       (const BYTE*)&v, sizeof(v));
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

static bool RestoreSystemResponsiveness() {
    BoostBackup backup;
    if (!LoadBoostBackup(backup) || !backup.hasSystemResp) return false;

    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile",
        0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;

    DWORD v = backup.originalSystemResp;
    r = RegSetValueExW(hKey, L"SystemResponsiveness", 0, REG_DWORD,
                       (const BYTE*)&v, sizeof(v));
    RegCloseKey(hKey);

    if (r == ERROR_SUCCESS) {
        backup.hasSystemResp = false;
        SaveBoostBackup(backup);
        WriteLog(L"[Boost] Restored SystemResponsiveness=" + std::to_wstring(v));
    }
    return r == ERROR_SUCCESS;
}

// MMCSS Game Task Priority
static bool SetMmcssGameTaskPriority() {
    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
        0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (r != ERROR_SUCCESS) {
        DWORD disp = 0;
        r = RegCreateKeyExW(HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
            0, nullptr, 0, KEY_SET_VALUE | KEY_QUERY_VALUE, nullptr, &hKey, &disp);
        if (r != ERROR_SUCCESS) return false;
    }

    // ✅ احفظ القيم الأصلية في الملف عند أول مرة
    BoostBackup backup;
    LoadBoostBackup(backup);
    if (!backup.hasMmcss) {
        DWORD type = 0, sz = sizeof(DWORD), cur = 0;
        if (RegQueryValueExW(hKey, L"GPU Priority", nullptr, &type,
                             (LPBYTE)&cur, &sz) == ERROR_SUCCESS && type == REG_DWORD)
            backup.mmcssGpuPriority = cur;
        sz = sizeof(DWORD);
        if (RegQueryValueExW(hKey, L"Priority", nullptr, &type,
                             (LPBYTE)&cur, &sz) == ERROR_SUCCESS && type == REG_DWORD)
            backup.mmcssPriority = cur;

        wchar_t buf[64]; sz = sizeof(buf);
        if (RegQueryValueExW(hKey, L"Scheduling Category", nullptr, &type,
                             (LPBYTE)buf, &sz) == ERROR_SUCCESS && type == REG_SZ)
            backup.mmcssSchedCategory = buf;
        sz = sizeof(buf);
        if (RegQueryValueExW(hKey, L"SFIO Priority", nullptr, &type,
                             (LPBYTE)buf, &sz) == ERROR_SUCCESS && type == REG_SZ)
            backup.mmcssSfioPriority = buf;

        backup.hasMmcss = true;
        SaveBoostBackup(backup);
        WriteLog(L"[Boost] Saved original MMCSS values");
    }

    DWORD gpuPriority = 8;
    DWORD priority = 6;
    const wchar_t* schedCat = L"High";
    const wchar_t* sfioPriority = L"High";

    bool ok = true;
    if (RegSetValueExW(hKey, L"GPU Priority", 0, REG_DWORD,
                       (const BYTE*)&gpuPriority, sizeof(gpuPriority)) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"Priority", 0, REG_DWORD,
                       (const BYTE*)&priority, sizeof(priority)) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"Scheduling Category", 0, REG_SZ,
                       (const BYTE*)schedCat,
                       (DWORD)((wcslen(schedCat) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) ok = false;
    if (RegSetValueExW(hKey, L"SFIO Priority", 0, REG_SZ,
                       (const BYTE*)sfioPriority,
                       (DWORD)((wcslen(sfioPriority) + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) ok = false;

    RegCloseKey(hKey);
    return ok;
}

static bool RestoreMmcssValues() {
    BoostBackup backup;
    if (!LoadBoostBackup(backup) || !backup.hasMmcss) return false;

    HKEY hKey = nullptr;
    LONG r = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Multimedia\\SystemProfile\\Tasks\\Games",
        0, KEY_SET_VALUE, &hKey);
    if (r != ERROR_SUCCESS) return false;

    DWORD v1 = backup.mmcssGpuPriority;
    DWORD v2 = backup.mmcssPriority;
    RegSetValueExW(hKey, L"GPU Priority", 0, REG_DWORD, (const BYTE*)&v1, sizeof(v1));
    RegSetValueExW(hKey, L"Priority", 0, REG_DWORD, (const BYTE*)&v2, sizeof(v2));
    RegSetValueExW(hKey, L"Scheduling Category", 0, REG_SZ,
                   (const BYTE*)backup.mmcssSchedCategory.c_str(),
                   (DWORD)((backup.mmcssSchedCategory.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"SFIO Priority", 0, REG_SZ,
                   (const BYTE*)backup.mmcssSfioPriority.c_str(),
                   (DWORD)((backup.mmcssSfioPriority.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    backup.hasMmcss = false;
    SaveBoostBackup(backup);
    WriteLog(L"[Boost] MMCSS values restored to defaults");
    return true;
}

// ✅ استرجاع شامل — يُستدعى يدوياً أو عند بدء تشغيل جديد
static void RestoreAllBoostDefaults() {
    // SystemResponsiveness
    if (g_sysRespRefCount > 0) {
        InterlockedExchange(&g_sysRespRefCount, 0);
    }
    RestoreSystemResponsiveness();

    // MMCSS
    if (g_mmcssRefCount > 0) {
        InterlockedExchange(&g_mmcssRefCount, 0);
    }
    RestoreMmcssValues();

    // Timer Resolution
    LONG cur = InterlockedExchange(&g_timerResolutionRefCount, 0);
    if (cur > 0) {
        timeEndPeriod(1);
        WriteLog(L"[Boost] Timer Resolution reverted (bulk)");
    }

    // امسح أي بقايا من ملف الـ backup
    BoostBackup backup;
    if (LoadBoostBackup(backup) && !backup.hasSystemResp && !backup.hasMmcss) {
        DeleteBoostBackup();
    }
}

// ✅ يُستدعى في بداية RunEngineApp — يسترجع أي تعديلات معلّقة من crash سابق
static void CheckAndRestoreStaleBoost() {
    BoostBackup backup;
    if (!LoadBoostBackup(backup)) return;

    if (backup.hasSystemResp || backup.hasMmcss) {
        WriteLog(L"[Boost] Detected stale Boost tweaks — restoring on startup");
        RestoreAllBoostDefaults();
    } else {
        // ملف فارغ — احذفه
        DeleteBoostBackup();
    }
}

// يستدعى عند تشغيل اللعبة
static void ApplyBoostFps(HANDLE hProcess, const GameEntry& g) {
    if (!g.boostFps) return;
    WriteLog(L"[Boost] Applying for: " + g.name);

    // 1) تعطيل Core 0
    if (g.boostDisableCore0 && hProcess && !g.applyAffinity) {
        SYSTEM_INFO si = {};
        GetSystemInfo(&si);
        DWORD_PTR mask = 0;
        DWORD n = si.dwNumberOfProcessors;
        if (n > 64) n = 64;
        for (DWORD i = 1; i < n; i++) {
            mask |= ((DWORD_PTR)1 << i);
        }
        if (mask && SetProcessAffinityMask(hProcess, mask)) {
            wchar_t buf[64];
            swprintf_s(buf, 64, L"[Boost] Core 0 disabled, mask=0x%llX", (unsigned long long)mask);
            WriteLog(buf);
        } else {
            WriteLog(L"[Boost] Core 0 disable FAILED");
        }
    }

    // 2) رفع الأولوية إلى High (فقط لو الحالية Normal)
    if (g.boostHighPriority && hProcess) {
        if (g.processPriority == ProcessPriority::Normal) {
            if (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS)) {
                WriteLog(L"[Boost] Priority set to High");
            } else {
                WriteLog(L"[Boost] Priority set FAILED");
            }
        } else {
            WriteLog(L"[Boost] Custom priority already set — skipping");
        }
    }

    // 3) Timer Resolution (reference counted)
    if (g.boostTimerResolution) {
        LONG prev = InterlockedIncrement(&g_timerResolutionRefCount);
        if (prev == 1) {
            if (timeBeginPeriod(1) == TIMERR_NOERROR) {
                WriteLog(L"[Boost] Timer Resolution set to 1ms");
            } else {
                InterlockedDecrement(&g_timerResolutionRefCount);
                WriteLog(L"[Boost] Timer Resolution FAILED");
            }
        }
    }

    // 4) SystemResponsiveness (Admin only، reference counted)
    if (g.boostSystemResponsiveness) {
        if (IsProcessElevated()) {
            LONG prev = InterlockedIncrement(&g_sysRespRefCount);
            if (prev == 1) {
                if (SetSystemResponsiveness(10)) {
                    WriteLog(L"[Boost] SystemResponsiveness set to 10");
                } else {
                    InterlockedDecrement(&g_sysRespRefCount);
                    WriteLog(L"[Boost] SystemResponsiveness FAILED");
                }
            }
        } else {
            WriteLog(L"[Boost] SystemResponsiveness skipped (not Admin)");
        }
    }

    // 5) MMCSS (Admin only، reference counted — نظامي على مستوى Windows)
    if (g.boostMmcss) {
        if (IsProcessElevated()) {
            LONG prev = InterlockedIncrement(&g_mmcssRefCount);
            if (prev == 1) {
                if (SetMmcssGameTaskPriority()) {
                    WriteLog(L"[Boost] MMCSS Game task priority applied");
                } else {
                    InterlockedDecrement(&g_mmcssRefCount);
                    WriteLog(L"[Boost] MMCSS FAILED");
                }
            }
        } else {
            WriteLog(L"[Boost] MMCSS skipped (not Admin)");
        }
    }
}

// ✅ كائن مؤقت لتوصيل حالة الـ Boost من RunningGame
struct RunningGame;

// يستدعى عند إغلاق اللعبة — نرجّع Timer Resolution + SystemResponsiveness + MMCSS
static void RevertBoostFps(bool boostFps, bool boostTimer, bool boostSysResp, bool boostMmcss) {
    if (!boostFps) return;

    // Timer Resolution
    if (boostTimer) {
        LONG cur = InterlockedCompareExchange(&g_timerResolutionRefCount, 0, 0);
        if (cur > 0) {
            LONG prev = InterlockedDecrement(&g_timerResolutionRefCount);
            if (prev == 0) {
                timeEndPeriod(1);
                WriteLog(L"[Boost] Timer Resolution reverted");
            }
        }
    }

    // SystemResponsiveness (reference counted)
    if (boostSysResp) {
        LONG cur = InterlockedCompareExchange(&g_sysRespRefCount, 0, 0);
        if (cur > 0) {
            LONG prev = InterlockedDecrement(&g_sysRespRefCount);
            if (prev == 0) {
                RestoreSystemResponsiveness();
            }
        }
    }

    // MMCSS (reference counted)
    if (boostMmcss) {
        LONG cur = InterlockedCompareExchange(&g_mmcssRefCount, 0, 0);
        if (cur > 0) {
            LONG prev = InterlockedDecrement(&g_mmcssRefCount);
            if (prev == 0) {
                RestoreMmcssValues();
            }
        }
    }
}

// ----------------------------- كاش الأيقونات -----------------------------
static ULONGLONG GetFileSignature(const std::wstring& path) {
    if (path.empty()) return 0;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return 0;
    ULONGLONG t = ((ULONGLONG)fad.ftLastWriteTime.dwHighDateTime << 32) | fad.ftLastWriteTime.dwLowDateTime;
    ULONGLONG s = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
    return t ^ (s * 0x9E3779B97F4A7C15ULL + 0x1000193ULL);
}

static void ClearIconCache() {
    for (auto& kv : g_iconCache) if (kv.second.icon) DestroyIcon(kv.second.icon);
    g_iconCache.clear();
    for (auto& kv : g_radialBgCache) if (kv.second.bmp) delete kv.second.bmp;
    g_radialBgCache.clear();
}

static void ClearRingBgCache() {
    if (g_ringBgCachedBmp) { delete g_ringBgCachedBmp; g_ringBgCachedBmp = nullptr; }
    g_ringBgSourcePath.clear();
    g_ringBgSourceSig = 0;
    g_ringBgCachedW = g_ringBgCachedH = 0;
}

static HICON ExtractShellItemIcon(const std::wstring& path, int sizePx) {
    if (path.empty() || sizePx <= 0) return nullptr;
    IShellItemImageFactory* factory = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return nullptr;
    HBITMAP hbmp = nullptr;
    SIZE sz = { sizePx, sizePx };
    hr = factory->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
    if (FAILED(hr) || !hbmp) hr = factory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hbmp);
    factory->Release();
    if (FAILED(hr) || !hbmp) return nullptr;
    ICONINFO ii = {};
    ii.fIcon = TRUE; ii.hbmColor = hbmp;
    ii.hbmMask = CreateBitmap(sizePx, sizePx, 1, 1, nullptr);
    HICON hicon = CreateIconIndirect(&ii);
    DeleteObject(hbmp);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return hicon;
}

static HICON ExtractUwpIconDirect(const std::wstring& aumid, int sizePx) {
    if (aumid.empty() || sizePx <= 0) return nullptr;
    std::wstring shellPath = L"shell:AppsFolder\\" + aumid;
    auto fromFactory = [&](IShellItemImageFactory* factory) -> HICON {
        if (!factory) return nullptr;
        HBITMAP hbmp = nullptr;
        SIZE sz = { sizePx, sizePx };
        HRESULT hr = factory->GetImage(sz, SIIGBF_ICONONLY | SIIGBF_BIGGERSIZEOK, &hbmp);
        if (FAILED(hr) || !hbmp) hr = factory->GetImage(sz, SIIGBF_BIGGERSIZEOK, &hbmp);
        if (FAILED(hr) || !hbmp) return nullptr;
        ICONINFO ii = {};
        ii.fIcon = TRUE; ii.hbmColor = hbmp;
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
                HICON h = fromFactory(factory);
                factory->Release();
                if (h) return h;
            }
        }
    }
    {
        IShellItemImageFactory* factory = nullptr;
        HRESULT hr = SHCreateItemFromParsingName(shellPath.c_str(), nullptr, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr) && factory) {
            HICON h = fromFactory(factory);
            factory->Release();
            if (h) return h;
        }
    }
    return nullptr;
}

static HICON ExtractExeIconRaw(const GameEntry& g) {
    if (IsUwpPath(g.exePath)) {
        HICON h = ExtractHighQualityIcon(g.exePath, 0, 96);
        if (h) return h;
        return ExtractUwpIconDirect(GetUwpAumid(g.exePath), 96);
    }
    std::wstring src = g.iconPath.empty() ? g.exePath : g.iconPath;
    std::wstring lower = src;
    for (auto& c : lower) c = (wchar_t)towlower(c);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == L".lnk") {
        HICON shellIcon = ExtractShellItemIcon(src, 96);
        if (shellIcon) return shellIcon;
    }
    return ExtractHighQualityIcon(src, g.iconIndex, 96);
}

static Bitmap* PrerenderRadialBgThumb(Bitmap* src) {
    REAL imgW = (REAL)src->GetWidth(), imgH = (REAL)src->GetHeight();
    if (imgW <= 0 || imgH <= 0) return nullptr;
    Bitmap* thumb = new Bitmap(RADIAL_BG_CACHE_PX, RADIAL_BG_CACHE_PX, PixelFormat32bppARGB);
    if (!thumb || thumb->GetLastStatus() != Ok) { delete thumb; return nullptr; }
    Graphics gfx(thumb);
    gfx.SetSmoothingMode(SmoothingModeAntiAlias);
    gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    gfx.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    gfx.Clear(Color(0, 0, 0, 0));
    REAL scale = max((REAL)RADIAL_BG_CACHE_PX / imgW, (REAL)RADIAL_BG_CACHE_PX / imgH);
    REAL dw = imgW * scale, dh = imgH * scale;
    REAL dx = ((REAL)RADIAL_BG_CACHE_PX - dw) / 2.0f;
    REAL dy = ((REAL)RADIAL_BG_CACHE_PX - dh) / 2.0f;
    gfx.DrawImage(src, RectF(dx, dy, dw, dh), 0.0f, 0.0f, imgW, imgH, UnitPixel);
    return thumb;
}

static void BuildIconCache() {
    std::map<std::wstring, CachedIconEntry> newIconCache;
    std::map<std::wstring, CachedRadialBg>  newRadialBgCache;
    for (auto& g : g_games) {
        {
            ULONGLONG sig = GetFileSignature(g.iconPath.empty() ? g.exePath : g.iconPath);
            auto it = g_iconCache.find(g.exePath);
            if (it != g_iconCache.end() && it->second.icon && it->second.sig == sig) {
                newIconCache[g.exePath] = it->second;
                g_iconCache.erase(it);
            } else {
                HICON ic = ExtractExeIconRaw(g);
                if (ic) newIconCache[g.exePath] = { ic, sig };
            }
        }
        if (!g.radialBgPath.empty()) {
            ULONGLONG sig = GetFileSignature(g.radialBgPath);
            auto it = g_radialBgCache.find(g.radialBgPath);
            if (it != g_radialBgCache.end() && it->second.bmp && it->second.sig == sig) {
                newRadialBgCache[g.radialBgPath] = it->second;
                g_radialBgCache.erase(it);
            } else {
                Bitmap* src = Bitmap::FromFile(g.radialBgPath.c_str());
                if (src && src->GetLastStatus() == Ok) {
                    Bitmap* thumb = PrerenderRadialBgThumb(src);
                    if (thumb) newRadialBgCache[g.radialBgPath] = { thumb, sig };
                }
                if (src) delete src;
            }
        }
    }
    for (auto& kv : g_iconCache) if (kv.second.icon) DestroyIcon(kv.second.icon);
    for (auto& kv : g_radialBgCache) if (kv.second.bmp) delete kv.second.bmp;
    g_iconCache.swap(newIconCache);
    g_radialBgCache.swap(newRadialBgCache);
}

static HICON GetCachedIcon(const std::wstring& path) {
    auto it = g_iconCache.find(path);
    return (it == g_iconCache.end()) ? nullptr : it->second.icon;
}
static Bitmap* GetCachedRadialBg(const std::wstring& path) {
    if (path.empty()) return nullptr;
    auto it = g_radialBgCache.find(path);
    return (it == g_radialBgCache.end()) ? nullptr : it->second.bmp;
}

struct RunningGame {
    HANDLE hProcess;
    DWORD pid;
    std::wstring gamePath;
    std::wstring exeName;
    std::wstring resolvedExeName;
    std::wstring displayName;
    ULONGLONG startTick;
    ULONGLONG totalSeconds;
    bool boostFps = false;
    bool boostTimerResolution = false;
    bool boostSystemResponsiveness = false;
    bool boostMmcss = false;
};
static std::vector<RunningGame> g_runningGames;

static const UINT_PTR PLAY_TIME_TIMER_ID = 3010;
static const UINT_PTR LANG_WATCH_TIMER_ID = 3001;
static const UINT_PTR OVERLAY_TIMER_ID    = 3020;
static bool   g_langWatchActive  = false;
static bool   g_langForcedNow    = false;
static HANDLE g_langWatchProcess = nullptr;
static DWORD  g_langWatchPid     = 0;
static HKL    g_langSavedLayout  = nullptr;
static HKL    g_langWatchTargetLayout = nullptr;
static ULONGLONG g_lastPlayTimeSave = 0;

static DWORD NowUnix() { return (DWORD)time(nullptr); }
static std::wstring MuteRequestPath()  { return GetAppDataDir() + L"\\mute_request.txt"; }
static std::wstring MuteStatePath()    { return GetAppDataDir() + L"\\mute_state.txt"; }
static std::wstring MuteUrisPath()     { return GetAppDataDir() + L"\\mute_uris.txt"; }
static std::wstring RunningGamesPath() { return GetAppDataDir() + L"\\running_games.txt"; }

static void WriteRunningGamesFile() {
    std::wstring target = RunningGamesPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        for (auto& rg : g_runningGames) f << rg.gamePath << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
static void LoadManualMuteUris() {
    g_manualMuteUris.clear();
    std::wifstream f(MuteUrisPath().c_str());
    if (!f.is_open()) return;
    std::wstring line;
    while (std::getline(f, line)) {
        line = TrimString(line);
        if (!line.empty()) g_manualMuteUris.insert(line);
    }
}
static void SaveManualMuteUris() {
    std::wstring target = MuteUrisPath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        for (auto& p : g_manualMuteUris) f << p << L"\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
static void RebuildVisibleIndices() {
    g_visibleGameIndices.clear();
    for (size_t i = 0; i < g_games.size(); i++) {
        if (!g_games[i].showInRadial) continue;
        if (!PathExistsOnDisk(g_games[i].exePath)) continue;
        g_visibleGameIndices.push_back((int)i);
    }
}

static void ComputeOrbitDistance() {
    int n = (int)g_visibleGameIndices.size();
    int extra = (n > 6) ? (n - 6) : 0;
    g_orbitDistance = g_baseOrbit + extra * ORBIT_PER_EXTRA;
}


static bool IsAnyTrackedGameRunning() {
    return !g_runningGames.empty();
}

// ----------------------------- كشف عملية اللعبة (URI) -----------------------------
static bool IsNonGameProcess(const std::wstring& lower) {
    static const wchar_t* skip[] = {
        L"explorer.exe", L"searchhost.exe", L"startmenuexperiencehost.exe",
        L"shellexperiencehost.exe", L"textinputhost.exe", L"dwm.exe",
        L"runtimebroker.exe", L"applicationframehost.exe", L"systemsettings.exe",
        L"taskmgr.exe", L"cmd.exe", L"powershell.exe", L"windowsterminal.exe",
        L"conhost.exe", L"lockapp.exe", L"sihost.exe", L"ctfmon.exe",
        L"steam.exe", L"steamwebhelper.exe", L"steamservice.exe", L"gameoverlayui.exe",
        L"gamebar.exe", L"gamebarpresencewriter.exe", L"gamebarftserver.exe",
        L"chrome.exe", L"firefox.exe", L"msedge.exe", L"brave.exe", L"opera.exe",
        L"vivaldi.exe", L"iexplore.exe",
        L"discord.exe", L"slack.exe", L"telegram.exe", L"whatsapp.exe",
        L"zoom.exe", L"teams.exe", L"skype.exe", L"signal.exe", L"messenger.exe",
        L"spotify.exe", L"itunes.exe", L"vlc.exe", L"mpv.exe", L"wmplayer.exe",
        L"groove.exe", L"musicbee.exe", L"foobar2000.exe",
        L"code.exe", L"devenv.exe", L"sublime_text.exe", L"notepad++.exe",
        L"notepad.exe", L"intellij idea64.exe", L"pycharm64.exe",
        L"webstorm64.exe", L"goland64.exe", L"clion64.exe", L"rider64.exe",
        L"studio64.exe", L"eclipse.exe", L"netbeans64.exe",
        L"obs64.exe", L"obs32.exe", L"obs.exe",
        L"windirstat.exe", L"processhacker.exe", L"procexp.exe", L"procmon.exe",
        L"7zfm.exe", L"winrar.exe",
    };
    for (auto s : skip) if (lower == s) return true;
    if (lower.find(L"gamelauncher") != std::wstring::npos) return true;
    return false;
}
static bool IsLikelyGameWindow(HWND hwnd) {
    RECT rc;
    if (!GetWindowRect(hwnd, &rc)) return false;
    return ((rc.right - rc.left) >= 640 && (rc.bottom - rc.top) >= 480);
}
static void DetectRunningGameProcesses() {
    bool hasUnresolvedUriGame = false;
    for (auto& rg : g_runningGames) {
        if (IsUriLike(rg.gamePath) && rg.resolvedExeName.empty()) { hasUnresolvedUriGame = true; break; }
    }
    if (!hasUnresolvedUriGame) return;
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    DWORD fgPid = 0;
    GetWindowThreadProcessId(fg, &fgPid);
    if (!fgPid || fgPid == GetCurrentProcessId()) return;
    if (!IsLikelyGameWindow(fg)) return;
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, fgPid);
    if (!hProc) return;
    wchar_t path[MAX_PATH] = L"";
    DWORD size = MAX_PATH;
    bool ok = QueryFullProcessImageNameW(hProc, 0, path, &size);
    CloseHandle(hProc);
    if (!ok) return;
    std::wstring name = GetFileNameFromPath(path);
    if (name.empty()) return;
    std::wstring lower = name;
    for (auto& c : lower) c = (wchar_t)towlower(c);
    if (IsNonGameProcess(lower)) return;
    for (auto& rg : g_runningGames) {
        if (!IsUriLike(rg.gamePath)) continue;
        if (rg.pid == fgPid) return;
    }
    for (auto& rg : g_runningGames) {
        if (!IsUriLike(rg.gamePath)) continue;
        if (!rg.resolvedExeName.empty()) continue;
        if (rg.pid != 0) continue;
        ULONGLONG sinceLaunch = GetTickCount64() - rg.startTick;
        if (sinceLaunch > 90000ULL) continue;
        rg.resolvedExeName = name;
        rg.pid = fgPid;
        WriteLog(L"Detected URI game process: " + name);
        break;
    }
}

static void RefreshMuteCache() {
    g_muteCache.clear();
    for (auto& g : g_games) {
        if (IsUwpPath(g.exePath)) continue;
        if (IsUriLike(g.exePath)) {
            bool found = false;
            for (auto& rg : g_runningGames) {
                if (_wcsicmp(rg.gamePath.c_str(), g.exePath.c_str()) == 0 && !rg.resolvedExeName.empty()) {
                    int st = MuteAudio::GetMuteStateByExeName(rg.resolvedExeName);
                    if (st == 2) g_muteCache[g.exePath] = true;
                    found = true;
                    break;
                }
            }
            if (!found && g_manualMuteUris.count(g.exePath)) g_muteCache[g.exePath] = true;
            continue;
        }
        std::wstring exeName = GetFileNameFromPath(g.exePath);
        if (exeName.empty()) continue;
        int st = MuteAudio::GetMuteStateByExeName(exeName);
        if (st == 2) g_muteCache[g.exePath] = true;
    }
}
static void WriteMuteStateFile() {
    std::wstring target = MuteStatePath();
    std::wstring tmp = target + L".tmp";
    {
        std::wofstream f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return;
        for (auto& kv : g_muteCache) if (kv.second) f << kv.first << L"|1\n";
    }
    MoveFileExW(tmp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING);
}
static void ProcessMuteRequest() {
    std::wstring reqPath = MuteRequestPath();
    if (GetFileAttributesW(reqPath.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    std::wifstream f(reqPath.c_str());
    if (!f.is_open()) return;
    std::wstring line;
    std::getline(f, line);
    f.close();
    DeleteFileW(reqPath.c_str());
    line = TrimString(line);
    if (line.empty()) return;
    WriteLog(L"Mute request from Panel: " + line);
    static const wchar_t* UNMUTE_PREFIX = L"*unmute:";
    static const size_t UNMUTE_PREFIX_LEN = 8;
    if (line.compare(0, UNMUTE_PREFIX_LEN, UNMUTE_PREFIX) == 0) {
        std::wstring exeName = TrimString(line.substr(UNMUTE_PREFIX_LEN));
        if (!exeName.empty()) {
            bool ok = MuteAudio::SetMuteByExeName(exeName, false);
            WriteLog(L"  Panel unmute: " + exeName + L" → " + (ok ? L"OK" : L"FAILED"));
            RefreshMuteCache();
            WriteMuteStateFile();
        }
        return;
    }
    if (IsUriLike(line) || IsUwpPath(line)) {
        if (g_manualMuteUris.count(line)) g_manualMuteUris.erase(line);
        else g_manualMuteUris.insert(line);
        SaveManualMuteUris();
        RefreshMuteCache();
        WriteMuteStateFile();
        return;
    }
    std::wstring exeName = GetFileNameFromPath(line);
    if (exeName.empty()) return;
    int cur = MuteAudio::GetMuteStateByExeName(exeName);
    bool ok = MuteAudio::SetMuteByExeName(exeName, cur != 2);
    WriteLog(L"  Panel mute: " + exeName + L" → " + (ok ? L"OK" : L"FAILED"));
    RefreshMuteCache();
    WriteMuteStateFile();
}

static DWORD PriorityToWin32Class(ProcessPriority p) {
    switch (p) {
        case ProcessPriority::Idle:        return IDLE_PRIORITY_CLASS;
        case ProcessPriority::BelowNormal: return BELOW_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::Normal:      return NORMAL_PRIORITY_CLASS;
        case ProcessPriority::AboveNormal: return ABOVE_NORMAL_PRIORITY_CLASS;
        case ProcessPriority::High:        return HIGH_PRIORITY_CLASS;
        case ProcessPriority::Realtime:    return REALTIME_PRIORITY_CLASS;
    }
    return NORMAL_PRIORITY_CLASS;
}
static void ApplyProcessTuning(HANDLE hProcess, const GameEntry& g) {
    if (!hProcess) return;
    if (g.processPriority != ProcessPriority::Normal) {
        DWORD cls = PriorityToWin32Class(g.processPriority);
        if (SetPriorityClass(hProcess, cls)) {
            WriteLog(L"  Priority set to class 0x" + std::to_wstring(cls));
        } else {
            DWORD err = GetLastError();
            WriteLog(L"  Priority FAILED (error=" + std::to_wstring(err) + L")");
        }
    }
    if (g.applyAffinity && g.affinityMask != 0) {
        SYSTEM_INFO si = {};
        GetSystemInfo(&si);
        DWORD_PTR mask = (DWORD_PTR)g.affinityMask;
        DWORD_PTR validMask = 0;
        for (DWORD i = 0; i < si.dwNumberOfProcessors && i < 64; i++) {
            if (mask & (1ULL << i)) validMask |= (1ULL << i);
        }
        if (validMask == 0) {
            WriteLog(L"  Affinity mask invalid");
        } else if (SetProcessAffinityMask(hProcess, validMask)) {
            WriteLog(L"  Affinity set to 0x" + std::to_wstring(validMask));
        } else {
            DWORD err = GetLastError();
            WriteLog(L"  Affinity FAILED (error=" + std::to_wstring(err) + L")");
        }
    }
}

static void LaunchPath(const std::wstring& path, bool runAsAdmin, const std::wstring& args,
                       HANDLE* outProcess = nullptr, DWORD* outPid = nullptr) {
    if (path.empty()) return;
    if (IsUwpPath(path)) {
        HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        bool shouldUninit = (hr == S_OK);
        ActivateUwpApp(GetUwpAumid(path));
        if (shouldUninit) CoUninitialize();
        if (outProcess) *outProcess = nullptr;
        if (outPid) *outPid = 0;
        return;
    }
    std::wstring dir;
    if (!IsUriLike(path)) {
        size_t pos = path.find_last_of(L"\\/");
        if (pos != std::wstring::npos) dir = path.substr(0, pos);
    }
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOASYNC;
    if (outProcess) sei.fMask |= SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = runAsAdmin ? L"runas" : L"open";
    sei.lpFile = path.c_str();
    sei.lpParameters = args.empty() ? nullptr : args.c_str();
    sei.lpDirectory = dir.empty() ? nullptr : dir.c_str();
    sei.nShow = SW_SHOWNORMAL;
    ShellExecuteExW(&sei);
    if (outProcess) *outProcess = sei.hProcess;
    if (outPid) *outPid = sei.hProcess ? GetProcessId(sei.hProcess) : 0;
}
static void MaybeStartLayoutWatch(HANDLE hProcess, DWORD pid, HKL targetLayout) {
    if (!hProcess) return;
    if (g_langWatchActive) { CloseHandle(hProcess); return; }
    if (!targetLayout) { CloseHandle(hProcess); return; }
    g_langWatchProcess = hProcess;
    g_langWatchPid = pid;
    g_langWatchTargetLayout = targetLayout;
    g_langForcedNow = false;
    g_langWatchActive = true;
    SetTimer(g_hiddenWnd, LANG_WATCH_TIMER_ID, 500, nullptr);
}
static void PollLayoutWatch() {
    if (!g_langWatchActive) return;
    bool gameExited = WaitForSingleObject(g_langWatchProcess, 0) == WAIT_OBJECT_0;
    HWND fg = gameExited ? nullptr : GetForegroundWindow();
    DWORD fgPid = 0;
    if (fg) GetWindowThreadProcessId(fg, &fgPid);
    bool gameIsForeground = !gameExited && fg && fgPid == g_langWatchPid;
    if (gameIsForeground && !g_langForcedNow) {
        DWORD tid = GetWindowThreadProcessId(fg, nullptr);
        g_langSavedLayout = GetKeyboardLayout(tid);
        if (g_langWatchTargetLayout) {
            PostMessageW(fg, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD,
                         (LPARAM)g_langWatchTargetLayout);
        }
        g_langForcedNow = true;
    } else if (!gameIsForeground && g_langForcedNow) {
        HWND target = gameExited ? GetForegroundWindow() : fg;
        if (target && g_langSavedLayout)
            PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, INPUTLANGCHANGE_FORWARD, (LPARAM)g_langSavedLayout);
        g_langForcedNow = false;
    }
    if (gameExited) {
        CloseHandle(g_langWatchProcess);
        g_langWatchProcess = nullptr;
        g_langWatchActive = false;
        g_langWatchTargetLayout = nullptr;
        KillTimer(g_hiddenWnd, LANG_WATCH_TIMER_ID);
    }
}
static void PollRunningGames() {
    if (g_runningGames.empty()) {
        InterlockedExchange(&g_gameRunningCount, 0);
        KillTimer(g_hiddenWnd, PLAY_TIME_TIMER_ID);
        return;
    }
    ULONGLONG now = GetTickCount64();
    bool save = false;
    bool forceSave = false;
    bool muteChanged = false;
    bool listChanged = false;
    DetectRunningGameProcesses();
    for (auto it = g_runningGames.begin(); it != g_runningGames.end(); ) {
        ULONGLONG elapsed = (now - it->startTick) / 1000;
        if (elapsed > 0) {
            it->totalSeconds += elapsed;
            for (auto& gg : g_games) {
                if (_wcsicmp(gg.exePath.c_str(), it->gamePath.c_str()) == 0) {
                    gg.totalPlaySeconds += elapsed;
                    save = true;
                    break;
                }
            }
            it->startTick = now;
        }
        bool exited = false;
        if (it->hProcess) exited = (WaitForSingleObject(it->hProcess, 0) == WAIT_OBJECT_0);
        else if (!it->exeName.empty()) exited = !IsProcessRunningByExeName(it->exeName);
        else if (!it->resolvedExeName.empty()) exited = !IsProcessRunningByExeName(it->resolvedExeName);
        else if (now - it->startTick > 300000ULL) exited = true;
        if (exited) {
            // ✅ سجّل الجلسة في سجل الجلسات قبل الحذف
            DWORD totalSec = (DWORD)it->totalSeconds;
            if (totalSec >= 30) {
                DWORD nowUnix = (DWORD)time(nullptr);
                DWORD startUnix = (nowUnix > totalSec) ? (nowUnix - totalSec) : nowUnix;
                std::wstring gname;
                for (auto& gg : g_games) {
                    if (_wcsicmp(gg.exePath.c_str(), it->gamePath.c_str()) == 0) {
                        gname = gg.name;
                        break;
                    }
                }
                AppendPlaySession(it->gamePath, gname, startUnix, totalSec);
                WriteLog(L"[Session] Recorded: " + std::to_wstring(totalSec) +
                    L"s for " + gname);
            }

            if (it->hProcess) CloseHandle(it->hProcess);

            // ✅ Boost FPS — استرجاع كامل
            RevertBoostFps(it->boostFps, it->boostTimerResolution,
                          it->boostSystemResponsiveness, it->boostMmcss);

            // Black Box — أوقف المراقبة
            {
                auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
                if (pm.IsMonitoring() &&
                    _wcsicmp(pm.CurrentGamePath().c_str(), it->gamePath.c_str()) == 0) {
                    pm.StopSession();
                    WriteLog(L"  [BlackBox] Monitoring stopped (game exited)");
                }
            }

            it = g_runningGames.erase(it);
            muteChanged = true; listChanged = true; forceSave = true;
            continue;
        }
        ++it;
    }
    InterlockedExchange(&g_gameRunningCount, (LONG)g_runningGames.size());
    if (listChanged) WriteRunningGamesFile();
    bool shouldSave = save && (forceSave || (now - g_lastPlayTimeSave) > 30000ULL);
    if (shouldSave) {
        std::vector<GameEntry> diskGames;
        LoadGames(diskGames);
        for (auto& dg : diskGames) {
            for (auto& mg : g_games) {
                if (_wcsicmp(dg.exePath.c_str(), mg.exePath.c_str()) == 0) {
                    if (mg.totalPlaySeconds > dg.totalPlaySeconds) dg.totalPlaySeconds = mg.totalPlaySeconds;
                    if (mg.playCount > dg.playCount) dg.playCount = mg.playCount;
                    if (mg.lastPlayedUnix > dg.lastPlayedUnix) dg.lastPlayedUnix = mg.lastPlayedUnix;
                    break;
                }
            }
        }
        g_games = std::move(diskGames);
        SaveGames(g_games);
        g_lastPlayTimeSave = now;
    }
    if (muteChanged) { RefreshMuteCache(); WriteMuteStateFile(); }
    if (g_runningGames.empty()) {
        InterlockedExchange(&g_gameRunningCount, 0);
        KillTimer(g_hiddenWnd, PLAY_TIME_TIMER_ID);
    }
}
static void FlushPlayTimeOnExit() {
    // ✅ Boost FPS — استرجاع كامل لكل لعبة شغالة
    for (auto& rg : g_runningGames) {
        RevertBoostFps(rg.boostFps, rg.boostTimerResolution,
                      rg.boostSystemResponsiveness, rg.boostMmcss);
    }

    // ✅ احتياطي: أي refCounts باقية → رجّعها
    if (g_sysRespRefCount > 0 || g_mmcssRefCount > 0 || g_timerResolutionRefCount > 0) {
        WriteLog(L"[Boost] Leftover refcounts detected — forcing full restore");
        RestoreAllBoostDefaults();
    }

    {
        auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
        if (pm.IsMonitoring()) {
            pm.StopSession();
            WriteLog(L"  [BlackBox] Monitoring stopped (engine exit)");
        }
    }
    ULONGLONG now = GetTickCount64();
    bool save = false;
    for (auto& rg : g_runningGames) {
        ULONGLONG elapsed = (now - rg.startTick) / 1000;
        if (elapsed > 0) {
            for (auto& gg : g_games) {
                if (_wcsicmp(gg.exePath.c_str(), rg.gamePath.c_str()) == 0) {
                    gg.totalPlaySeconds += elapsed;
                    save = true;
                    break;
                }
            }
        }
        // ✅ سجّل الجلسة قبل الإغلاق
        DWORD totalSec = (DWORD)(rg.totalSeconds + elapsed);
        if (totalSec >= 30) {
            DWORD nowUnix = (DWORD)time(nullptr);
            DWORD startUnix = (nowUnix > totalSec) ? (nowUnix - totalSec) : nowUnix;
            std::wstring gname;
            for (auto& gg : g_games) {
                if (_wcsicmp(gg.exePath.c_str(), rg.gamePath.c_str()) == 0) {
                    gname = gg.name;
                    break;
                }
            }
            AppendPlaySession(rg.gamePath, gname, startUnix, totalSec);
        }
        if (rg.hProcess) CloseHandle(rg.hProcess);
    }
    g_runningGames.clear();

    WriteRunningGamesFile();
    if (save) SaveGames(g_games);
}

// ============================================================
// يد التحكم
// ============================================================
static DWORD PollAllControllers() {
    DWORD allButtons = 0;
    XINPUT_STATE state;
    for (DWORD i = 0; i < 4; i++) {
        if (MyXInputGetState(i, &state) == ERROR_SUCCESS) {
            allButtons |= state.Gamepad.wButtons;
        }
    }
    return allButtons;
}
static bool GetFirstActiveController(XINPUT_STATE& out) {
    for (DWORD i = 0; i < 4; i++) {
        if (MyXInputGetState(i, &out) == ERROR_SUCCESS) return true;
    }
    return false;
}

static DWORD WINAPI ControllerThreadProc(LPVOID) {
    DWORD lastButtons = 0;
    while (InterlockedCompareExchange(&g_controllerThreadStop, 0, 0) == 0) {
        if (!g_controllerSettings.enabled) {
            Sleep(1000);
            lastButtons = 0;
            continue;
        }
        if (!g_controllerSettings.allowControllerDuringGame && IsAnyTrackedGameRunning()) {
            lastButtons = PollAllControllers();
            Sleep(100);
            continue;
        }
        DWORD allButtons = PollAllControllers();
        DWORD pressed = allButtons & ~lastButtons;
        if (pressed & g_controllerSettings.openRadialButton) {
            if (GetTickCount64() >= g_controllerSuppressUntil) {
                if (g_hiddenWnd) PostMessageW(g_hiddenWnd, WM_APP_CONTROLLER_TOGGLE, 0, 0);
            }
        }
        lastButtons = allButtons;
        Sleep(50);
    }
    return 0;
}

static DWORD g_inRadialLastButtons = 0;
static ULONGLONG g_inRadialLastMoveTime = 0;

static void PollControllerInRadial() {
    if (!g_controllerSettings.enabled) return;
    XINPUT_STATE state = {};
    if (!GetFirstActiveController(state)) return;
    DWORD allButtons = PollAllControllers();
    DWORD pressed = allButtons & ~g_inRadialLastButtons;
    g_inRadialLastButtons = allButtons;
    if (GetTickCount64() < g_controllerSuppressUntil) return;

    if (pressed & g_controllerSettings.openRadialButton) {
        if (g_controllerSettings.toggleMode) {
            g_controllerSuppressUntil = GetTickCount64() + 600;
            g_shouldClosePopup = true;
        }
        return;
    }
    if (pressed & XINPUT_GAMEPAD_A) {
        if (g_hover >= 0) { LaunchGame(g_games[g_hover]); g_shouldClosePopup = true; }
        else if (g_hover == -1) { OpenPanel(); g_shouldClosePopup = true; }
        return;
    }
    if (pressed & XINPUT_GAMEPAD_B) { g_shouldClosePopup = true; return; }
    if (pressed & XINPUT_GAMEPAD_X) { OpenPanel(); g_shouldClosePopup = true; return; }

    const SHORT DEADZONE = 18000;
    bool moveNext = false;
    bool movePrev = false;
    if (state.Gamepad.sThumbLX >  DEADZONE) moveNext = true;
    if (state.Gamepad.sThumbLX < -DEADZONE) movePrev = true;
    if (state.Gamepad.sThumbLY < -DEADZONE) moveNext = true;
    if (state.Gamepad.sThumbLY >  DEADZONE) movePrev = true;
    if (pressed & (XINPUT_GAMEPAD_DPAD_RIGHT | XINPUT_GAMEPAD_DPAD_DOWN)) moveNext = true;
    if (pressed & (XINPUT_GAMEPAD_DPAD_LEFT  | XINPUT_GAMEPAD_DPAD_UP))   movePrev = true;

    bool dpadPressed = (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT |
                                    XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN)) != 0;
    ULONGLONG now = GetTickCount64();
    bool canMove = dpadPressed || (now - g_inRadialLastMoveTime > 170);
    if (canMove) {
        if (moveNext) {
            int n = KbNextIndex(g_hover, +1);
            if (n != g_hover) { g_hover = n; PaintPopup(); g_inRadialLastMoveTime = now; }
        } else if (movePrev) {
            int n = KbNextIndex(g_hover, -1);
            if (n != g_hover) { g_hover = n; PaintPopup(); g_inRadialLastMoveTime = now; }
        }
    }
}

static void LaunchGame(const GameEntry& g) {
    if (g_isFirstRun) {
        g_isFirstRun = false;
        MarkFirstRunDone();
    }

    bool isSteamGame = (g.exePath.find(L"steam://") == 0);
    bool isProtocolUri = IsUriLike(g.exePath) && !isSteamGame;

    HKL targetHkl = nullptr;
    if (g.gameLanguage == 1)      targetHkl = LoadKeyboardLayoutW(L"00000401", KLF_ACTIVATE);
    else if (g.gameLanguage == 2) targetHkl = LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE);
    bool wantLayoutWatch = (targetHkl != nullptr) && !IsUwpPath(g.exePath);

    WriteLog(L"Launching: " + g.name + L" (" + g.exePath + L")");
    HANDLE hProcess = nullptr; DWORD pid = 0;
    LaunchPath(g.exePath, g.runAsAdmin, g.launchArgs, &hProcess, &pid);
    if (hProcess && !IsUwpPath(g.exePath)) {
        ApplyProcessTuning(hProcess, g);
        ApplyBoostFps(hProcess, g);
    }
    for (auto& gg : g_games) {
        if (_wcsicmp(gg.exePath.c_str(), g.exePath.c_str()) == 0) {
            gg.playCount += 1;
            gg.lastPlayedUnix = NowUnix();
            break;
        }
    }
    SaveGames(g_games);
    RunningGame rg;
    rg.gamePath = g.exePath;
    rg.exeName = (isSteamGame || isProtocolUri) ? L"" : GetFileNameFromPath(g.exePath);
    rg.resolvedExeName = L"";
    rg.displayName = g.name;
    rg.startTick = GetTickCount64();
    rg.totalSeconds = 0;
    rg.boostFps = g.boostFps;
    rg.boostTimerResolution = g.boostFps && g.boostTimerResolution;
    rg.boostSystemResponsiveness = g.boostFps && g.boostSystemResponsiveness && IsProcessElevated();
    rg.boostMmcss = g.boostFps && g.boostMmcss && IsProcessElevated();
    if (isSteamGame || isProtocolUri) {
        if (hProcess) CloseHandle(hProcess);
        rg.hProcess = nullptr;
        rg.pid = 0;
    } else {
        rg.hProcess = hProcess;
        rg.pid = pid;
    }
    g_runningGames.push_back(rg);
    InterlockedExchange(&g_gameRunningCount, (LONG)g_runningGames.size());
    WriteRunningGamesFile();
    SetTimer(g_hiddenWnd, PLAY_TIME_TIMER_ID, 5000, nullptr);

    if (g.boostFps && g.boostStopStats) {
        if (g_overlayWnd) HideOverlay();
        WriteLog(L"[Boost] Stats + Overlay suppressed");
    }

    else if (g.performanceMonitor) {
        auto& pm = PerfBlackBox::PerformanceMonitor::Instance();

        // ✅ إيقاف أي جلسة نشطة قسراً — يمنع تسرب بيانات اللعبة السابقة
        if (pm.IsMonitoring()) {
            WriteLog(L"  [BlackBox] Force-stopping stale session for: " +
                pm.CurrentGameName() + L" before starting: " + g.name);
            pm.StopSession();
        }

        bool started = pm.StartSession(
            g.exePath, g.name, g.performanceCpuTemp, rg.pid);

        if (started) {
            WriteLog(L"  [BlackBox] Monitoring started for: " + g.name +
                L" (PID: " + std::to_wstring(rg.pid) + L")");
        }
        else {
            WriteLog(L"  [BlackBox] FAILED to start monitoring for: " + g.name +
                L" — check log");
        }
    }


    if (wantLayoutWatch && pid && !isSteamGame && !isProtocolUri && rg.hProcess) {
        HANDLE hDup = nullptr;
        if (DuplicateHandle(GetCurrentProcess(), rg.hProcess, GetCurrentProcess(),
                            &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            MaybeStartLayoutWatch(hDup, pid, targetHkl);
        }
    }
    for (auto& c : g.companions) {
        if (!c.showInRadial) continue;
        if (!IsUriLike(c.path) && !IsUwpPath(c.path)) {
            std::wstring exeName = GetFileNameFromPath(c.path);
            if (!exeName.empty() && IsProcessRunningByExeName(exeName)) continue;
        }
        LaunchPath(c.path, c.runAsAdmin, c.launchArgs);
    }
    if (g_overlaySettings.enabled && g_overlaySettings.showOnGameLaunch && !g.boostFps) {
        ShowOverlay();
    }
}

// ----------------------------- هندسة -----------------------------
static POINT GamePos(int index, int total) {
    double angle = (2.0 * PI_CONST * index / (double)total) - (PI_CONST / 2.0);
    POINT p;
    p.x = g_center.x + (int)std::round(g_orbitDistance * std::cos(angle));
    p.y = g_center.y + (int)std::round(g_orbitDistance * std::sin(angle));
    return p;
}
static int HitTest(POINT pt) {
    int dx = pt.x - g_center.x, dy = pt.y - g_center.y;
    if (dx * dx + dy * dy <= g_hubRadius * g_hubRadius) return -1;
    int total = (int)g_visibleGameIndices.size();
    if (total == 0) return -2;
    for (int i = 0; i < total; i++) {
        POINT gp = GamePos(i, total);
        int gdx = pt.x - gp.x, gdy = pt.y - gp.y;
        if (gdx * gdx + gdy * gdy <= g_gameRadius * g_gameRadius)
            return g_visibleGameIndices[i];
    }
    return -2;
}

// ----------------------------- رسم -----------------------------
static void DrawIconOnly(Graphics& gfx, POINT center, int r, HICON icon, const std::wstring& fallbackText) {
    if (icon) {
        Bitmap iconBmp(icon);
        int isz = (int)(r * 1.05);
        GraphicsPath iconClip;
        iconClip.AddEllipse((REAL)(center.x - r), (REAL)(center.y - r), (REAL)(r * 2), (REAL)(r * 2));
        GraphicsState clipState = gfx.Save();
        gfx.SetClip(&iconClip);
        gfx.DrawImage(&iconBmp, center.x - isz / 2, center.y - isz / 2, isz, isz);
        gfx.Restore(clipState);
    } else if (!fallbackText.empty()) {
        FontFamily ff(L"Segoe UI");
        Font font(&ff, (REAL)(r * 0.9f), FontStyleBold, UnitPixel);
        SolidBrush txtBrush(Color(255, 255, 255, 255));
        std::wstring letter(1, towupper(fallbackText[0]));
        RectF layoutRect((REAL)(center.x - r), (REAL)(center.y - r), (REAL)(r * 2), (REAL)(r * 2));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        gfx.DrawString(letter.c_str(), -1, &font, layoutRect, &sf, &txtBrush);
    }
}
static void DrawSoftShadow(Graphics& gfx, POINT center, int r) {
    for (int i = 4; i >= 1; i--) {
        SolidBrush shadow(Color(14 * i, 0, 0, 0));
        int rr = r + i;
        gfx.FillEllipse(&shadow, center.x - rr + 2, center.y - rr + 4, rr * 2, rr * 2);
    }
}
static void DrawRadialCustomBackground(Graphics& gfx, POINT center, int r, const std::wstring& bgPath, bool asIcon) {
    if (bgPath.empty()) return;
    Bitmap* bmp = GetCachedRadialBg(bgPath);
    if (!bmp) return;
    REAL imgW = (REAL)bmp->GetWidth();
    REAL imgH = (REAL)bmp->GetHeight();
    if (imgW <= 0 || imgH <= 0) return;
    GraphicsPath circlePath;
    circlePath.AddEllipse((REAL)(center.x - r), (REAL)(center.y - r), (REAL)(r * 2), (REAL)(r * 2));
    GraphicsState state = gfx.Save();
    gfx.SetClip(&circlePath);
    REAL scale = max((REAL)(r * 2) / imgW, (REAL)(r * 2) / imgH);
    REAL dw = imgW * scale;
    REAL dh = imgH * scale;
    REAL dx = (REAL)center.x - dw / 2.0f;
    REAL dy = (REAL)center.y - dh / 2.0f;
    gfx.DrawImage(bmp, RectF(dx, dy, dw, dh), 0.0f, 0.0f, imgW, imgH, UnitPixel);
    if (!asIcon) {
        SolidBrush dim(Color(110, 0, 0, 0));
        gfx.FillEllipse(&dim, (REAL)(center.x - r), (REAL)(center.y - r), (REAL)(r * 2), (REAL)(r * 2));
    }
    gfx.Restore(state);
}
static void DrawIconBackdrop(Graphics& gfx, POINT center, int r, const std::wstring& bgPath) {
    if (bgPath.empty()) return;
    REAL br = (REAL)(r * 0.62);
    SolidBrush plate(Color(215, 15, 12, 29));
    gfx.FillEllipse(&plate, (REAL)center.x - br, (REAL)center.y - br, br * 2, br * 2);
}

static void DrawContainmentEdge(Graphics& gfx, POINT center, int r) {
    Pen edge(Color(70, 255, 255, 255), 1.0f);
    gfx.DrawEllipse(&edge, (REAL)(center.x - r), (REAL)(center.y - r), (REAL)(r * 2), (REAL)(r * 2));
}

static void StyleOutline(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    DrawSoftShadow(gfx, c, r);
    Pen pen(Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), hover ? 4.0f : 3.0f);
    gfx.DrawEllipse(&pen, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleNeon(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    for (int i = 5; i >= 1; i--) {
        Pen glow(Color(30, R, G, B), (REAL)(i * 2));
        int rr = r + i * 2;
        gfx.DrawEllipse(&glow, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }
    Pen core(Color(255, R, G, B), 2.5f);
    gfx.DrawEllipse(&core, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleGradient(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    DrawSoftShadow(gfx, c, r);
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    LinearGradientBrush gradBrush(rect, Color(255, R, G, B), Color(40, 255, 255, 255),
                                  LinearGradientModeForwardDiagonal);
    Pen pen(&gradBrush, hover ? 5.0f : 3.5f);
    gfx.DrawEllipse(&pen, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleMinimal(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    (void)color; (void)hover;
    DrawSoftShadow(gfx, c, r);
}
static void StyleHex(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    DrawSoftShadow(gfx, c, r);
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    PointF pts[6];
    REAL hr = (REAL)(hover ? r + 3 : r);
    for (int i = 0; i < 6; i++) {
        REAL ang = (REAL)(PI_CONST / 180.0 * (60.0 * i - 90.0));
        pts[i] = PointF((REAL)c.x + hr * (REAL)cos(ang), (REAL)c.y + hr * (REAL)sin(ang));
    }
    Pen pen(Color(255, R, G, B), 3.0f);
    pen.SetLineJoin(LineJoinRound);
    gfx.DrawPolygon(&pen, pts, 6);
}
static void StyleComet(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    Pen faint(Color(50, R, G, B), 2.0f);
    gfx.DrawEllipse(&faint, c.x - r, c.y - r, r * 2, r * 2);
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    for (int i = 0; i < 3; i++) {
        int alpha = 220 - i * 60;
        Pen bright(Color((BYTE)alpha, R, G, B), (REAL)(hover ? 5 - i : 4 - i));
        gfx.DrawArc(&bright, rect, -90.0f + i * 8.0f, 100.0f - i * 15.0f);
    }
}
static void StyleBurst(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    for (int i = 4; i >= 1; i--) {
        Pen halo(Color(20 + i * 3, R, G, B), (REAL)(i * 3));
        int rr = r + i * 4;
        gfx.DrawEllipse(&halo, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }
    const int SPIKES = 8;
    REAL spikeLen = (REAL)(hover ? r + 16 : r + 10);
    REAL spikeInner = (REAL)r - 2;
    for (int i = 0; i < SPIKES; i++) {
        double ang = (PI_CONST * 2.0 * i / SPIKES) - PI_CONST / 2.0;
        REAL cx = (REAL)c.x, cy = (REAL)c.y;
        PointF p1((REAL)(cx + spikeInner * (REAL)cos(ang)), (REAL)(cy + spikeInner * (REAL)sin(ang)));
        PointF p2((REAL)(cx + spikeLen   * (REAL)cos(ang)), (REAL)(cy + spikeLen   * (REAL)sin(ang)));
        Pen spike(Color(200, R, G, B), hover ? 2.5f : 2.0f);
        spike.SetStartCap(LineCapRound);
        spike.SetEndCap(LineCapRound);
        gfx.DrawLine(&spike, p1, p2);
        if (hover) {
            SolidBrush tip(Color(255, 255, 255, 255));
            gfx.FillEllipse(&tip, (REAL)(p2.X - 1.5f), (REAL)(p2.Y - 1.5f), (REAL)3.0f, (REAL)3.0f);
        }
    }
    Pen core(Color(255, R, G, B), 3.0f);
    gfx.DrawEllipse(&core, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleSakura(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    int pr = (R * 30 + 255 * 70) / 100;
    int pg = (G * 30 + 182 * 70) / 100;
    int pb = (B * 30 + 193 * 70) / 100;
    DrawSoftShadow(gfx, c, r);
    Pen outer(Color(180, pr, pg, pb), 1.5f);
    gfx.DrawEllipse(&outer, c.x - r - 2, c.y - r - 2, (r + 2) * 2, (r + 2) * 2);
    Pen core(Color(255, pr, pg, pb), hover ? 4.0f : 3.0f);
    gfx.DrawEllipse(&core, c.x - r, c.y - r, r * 2, r * 2);
    const int PETALS = 5;
    REAL petalR = (REAL)(hover ? r + 8 : r + 4);
    for (int i = 0; i < PETALS; i++) {
        double ang = (PI_CONST * 2.0 * i / PETALS) - PI_CONST / 2.0;
        REAL px = (REAL)c.x + petalR * (REAL)cos(ang);
        REAL py = (REAL)c.y + petalR * (REAL)sin(ang);
        SolidBrush petal(Color(220, pr, pg, pb));
        gfx.FillEllipse(&petal, (REAL)(px - 4.0f), (REAL)(py - 4.0f), (REAL)8.0f, (REAL)8.0f);
        SolidBrush inner(Color(255, 255, 240, 245));
        gfx.FillEllipse(&inner, (REAL)(px - 1.5f), (REAL)(py - 1.5f), (REAL)3.0f, (REAL)3.0f);
        Pen edge(Color(200, 255, 220, 230), 1.0f);
        gfx.DrawEllipse(&edge, (REAL)(px - 4.0f), (REAL)(py - 4.0f), (REAL)8.0f, (REAL)8.0f);
    }
}
static void StyleGlass(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    SolidBrush glass(Color(60, R, G, B));
    gfx.FillEllipse(&glass, c.x - r, c.y - r, r * 2, r * 2);
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    LinearGradientBrush highlight(rect,
        Color(140, 255, 255, 255),
        Color(0, 255, 255, 255),
        LinearGradientModeVertical);
    gfx.FillEllipse(&highlight, c.x - r, c.y - r, r * 2, r * 2);
    Pen ring(Color(220, 255, 255, 255), hover ? 2.5f : 1.8f);
    gfx.DrawEllipse(&ring, c.x - r, c.y - r, r * 2, r * 2);
    Pen innerRing(Color(120, R, G, B), 1.2f);
    int ri = r - 3;
    gfx.DrawEllipse(&innerRing, c.x - ri, c.y - ri, ri * 2, ri * 2);
    SolidBrush glint(Color(180, 255, 255, 255));
    gfx.FillEllipse(&glint,
                    (REAL)c.x - r * 0.4f, (REAL)c.y - r * 0.6f,
                    (REAL)(r * 0.35f), (REAL)(r * 0.25f));
}
static void StyleVortex(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    const int ARCS = 4;
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    RectF rect2((REAL)(c.x - r + 4), (REAL)(c.y - r + 4), (REAL)((r - 4) * 2), (REAL)((r - 4) * 2));
    for (int i = 0; i < ARCS; i++) {
        int alpha = 200 - i * 40;
        float startAngle = (float)(i * 90.0 + 15.0);
        float sweep = 60.0f;
        Pen arc(Color((BYTE)alpha, R, G, B), hover ? 3.0f : 2.0f);
        arc.SetStartCap(LineCapRound);
        arc.SetEndCap(LineCapRound);
        gfx.DrawArc(&arc, i % 2 == 0 ? rect : rect2, startAngle, sweep);
    }
    Pen outer(Color(60, R, G, B), 1.0f);
    gfx.DrawEllipse(&outer, c.x - r - 3, c.y - r - 3, (r + 3) * 2, (r + 3) * 2);
    SolidBrush center(Color(hover ? 200 : 120, R, G, B));
    gfx.FillEllipse(&center, (REAL)((REAL)c.x - 2.5f), (REAL)((REAL)c.y - 2.5f), (REAL)5.0f, (REAL)5.0f);
}
static void StyleSimple(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    Pen pen(Color(255, GetRValue(color), GetGValue(color), GetBValue(color)), hover ? 3.0f : 2.0f);
    gfx.DrawEllipse(&pen, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleAurora(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    BYTE r2 = (BYTE)min(255, R + 90);
    BYTE g2 = (BYTE)min(255, G + 50);
    BYTE b2 = (BYTE)min(255, B + 160);
    for (int i = 3; i >= 1; i--) {
        Pen glow(Color((BYTE)(18 * i), r2, g2, b2), (REAL)(i * 2));
        int rr = r + i * 2;
        gfx.DrawEllipse(&glow, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }
    Color stops[3] = { Color(255, R, G, B), Color(255, r2, g2, b2), Color(255, B, R, G) };
    REAL positions[3] = { 0.0f, 0.5f, 1.0f };
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    LinearGradientBrush grad(rect, Color(255, R, G, B), Color(255, b2, r2, g2), LinearGradientModeForwardDiagonal);
    grad.SetInterpolationColors(stops, positions, 3);
    Pen ring(&grad, hover ? 4.5f : 3.5f);
    gfx.DrawEllipse(&ring, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleHalo(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    Pen base(Color(110, R, G, B), 1.2f);
    gfx.DrawEllipse(&base, c.x - r, c.y - r, r * 2, r * 2);
    const int DOTS = 16;
    REAL dotR = (REAL)(hover ? r + 6 : r + 3);
    for (int i = 0; i < DOTS; i++) {
        double ang = (PI_CONST * 2.0 * i / DOTS);
        REAL px = (REAL)c.x + dotR * (REAL)cos(ang);
        REAL py = (REAL)c.y + dotR * (REAL)sin(ang);
        bool major = (i % 4 == 0);
        int alpha = major ? 255 : 110;
        REAL sz = major ? 4.0f : 2.0f;
        SolidBrush dot(Color((BYTE)alpha, R, G, B));
        gfx.FillEllipse(&dot, px - sz / 2.0f, py - sz / 2.0f, sz, sz);
    }
    Pen tick(Color(200, R, G, B), 2.0f);
    gfx.DrawLine(&tick, (REAL)c.x, (REAL)(c.y - r - 8), (REAL)c.x, (REAL)(c.y - r - 2));
    gfx.DrawLine(&tick, (REAL)c.x, (REAL)(c.y + r + 2), (REAL)c.x, (REAL)(c.y + r + 8));
}
static void StyleRipple(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    for (int i = 0; i < 3; i++) {
        int rr = r - i * 6;
        if (rr <= 0) break;
        int alpha = 200 - i * 55;
        Pen p(Color((BYTE)alpha, R, G, B), hover ? 2.8f - i * 0.3f : 2.0f - i * 0.2f);
        gfx.DrawEllipse(&p, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }
}
static void StyleCrystal(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    PointF pts[6];
    REAL hr = (REAL)(hover ? r + 4 : r);
    for (int i = 0; i < 6; i++) {
        REAL ang = (REAL)(PI_CONST / 180.0 * (60.0 * i - 90.0));
        pts[i] = PointF((REAL)c.x + hr * (REAL)cos(ang), (REAL)c.y + hr * (REAL)sin(ang));
    }
    Pen pen(Color(255, R, G, B), hover ? 3.5f : 2.5f);
    pen.SetLineJoin(LineJoinRound);
    gfx.DrawPolygon(&pen, pts, 6);
    Pen inner(Color(140, R, G, B), 1.2f);
    gfx.DrawLine(&inner, pts[0].X, pts[0].Y, pts[3].X, pts[3].Y);
    gfx.DrawLine(&inner, pts[1].X, pts[1].Y, pts[4].X, pts[4].Y);
    gfx.DrawLine(&inner, pts[2].X, pts[2].Y, pts[5].X, pts[5].Y);
}
static void StylePlasma(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    BYTE r2 = (BYTE)min(255, R + 60);
    BYTE g2 = (BYTE)min(255, G + 100);
    BYTE b2 = (BYTE)min(255, B + 40);
    for (int i = 4; i >= 1; i--) {
        int rr = r + i * 3;
        Pen glow(Color((BYTE)(15 * i), r2, g2, b2), (REAL)(i * 1.8f));
        gfx.DrawEllipse(&glow, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    LinearGradientBrush grad(rect, Color(255, R, G, B), Color(200, r2, g2, b2),
                             LinearGradientModeBackwardDiagonal);
    Pen core(&grad, hover ? 4.5f : 3.5f);
    gfx.DrawEllipse(&core, c.x - r, c.y - r, r * 2, r * 2);
    SolidBrush dot(Color(hover ? 220 : 140, r2, g2, b2));
    gfx.FillEllipse(&dot, (REAL)(c.x - 3), (REAL)(c.y - 3), (REAL)6, (REAL)6);
}
static void StyleOrbit(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    Pen main(Color(230, R, G, B), hover ? 2.5f : 2.0f);
    gfx.DrawEllipse(&main, c.x - r, c.y - r, r * 2, r * 2);
    const int DOTS = 4;
    for (int i = 0; i < DOTS; i++) {
        double ang = (PI_CONST * 2.0 * i / DOTS) + (hover ? PI_CONST / 4.0 : 0);
        REAL rad = (REAL)(r + (hover ? 8 : 5));
        REAL px = (REAL)c.x + rad * (REAL)cos(ang);
        REAL py = (REAL)c.y + rad * (REAL)sin(ang);
        SolidBrush dot(Color(255, R, G, B));
        gfx.FillEllipse(&dot, px - 3.0f, py - 3.0f, 6.0f, 6.0f);
    }
    RectF innerR((REAL)(c.x - r + 5), (REAL)(c.y - r + 5), (REAL)((r - 5) * 2), (REAL)((r - 5) * 2));
    Pen arc(Color(140, R, G, B), 1.5f);
    gfx.DrawArc(&arc, innerR, 30.0f, 120.0f);
}
static void StylePixel(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    const int BOXES = 8;
    REAL boxSize = hover ? 5.0f : 4.0f;
    for (int i = 0; i < BOXES; i++) {
        double ang = PI_CONST * 2.0 * i / BOXES - PI_CONST / 2.0;
        REAL px = (REAL)c.x + (REAL)r * (REAL)cos(ang);
        REAL py = (REAL)c.y + (REAL)r * (REAL)sin(ang);
        SolidBrush box(Color(255, R, G, B));
        gfx.FillRectangle(&box, px - boxSize / 2, py - boxSize / 2, boxSize, boxSize);
    }
    Pen faint(Color(60, R, G, B), 1.0f);
    gfx.DrawEllipse(&faint, c.x - r, c.y - r, r * 2, r * 2);
}
static void StyleCircuit(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    DrawSoftShadow(gfx, c, r);
    Pen ring(Color(220, R, G, B), hover ? 2.5f : 2.0f);
    gfx.DrawEllipse(&ring, c.x - r, c.y - r, r * 2, r * 2);
    const int NODES = 6;
    for (int i = 0; i < NODES; i++) {
        double ang = PI_CONST * 2.0 * i / NODES - PI_CONST / 2.0;
        REAL px = (REAL)c.x + (REAL)r * (REAL)cos(ang);
        REAL py = (REAL)c.y + (REAL)r * (REAL)sin(ang);
        SolidBrush node(Color(255, R, G, B));
        gfx.FillEllipse(&node, px - 3.0f, py - 3.0f, 6.0f, 6.0f);
        REAL inPx = (REAL)c.x + (REAL)(r - 8) * (REAL)cos(ang);
        REAL inPy = (REAL)c.y + (REAL)(r - 8) * (REAL)sin(ang);
        Pen line(Color(160, R, G, B), 1.2f);
        gfx.DrawLine(&line, px, py, inPx, inPy);
    }
    SolidBrush center(Color(hover ? 255 : 180, R, G, B));
    gfx.FillEllipse(&center, (REAL)(c.x - 2), (REAL)(c.y - 2), (REAL)4, (REAL)4);
}
static void StyleCustom(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    CustomRadialStyle cs = LoadCustomRadialStyle();
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);

    DrawSoftShadow(gfx, c, r);

    // طبقات التوهج (خارج الحلقات)
    for (int i = cs.glowLayers; i >= 1; i--) {
        int alpha = (int)(cs.glowStrength * 70.0f) - i * 8;
        if (alpha < 0) alpha = 0;
        if (alpha > 255) alpha = 255;
        REAL thickness = (REAL)(i * 2);
        Pen glow(Color((BYTE)alpha, R, G, B), thickness);
        int rr = r + i * 3 + (hover ? 2 : 0);
        gfx.DrawEllipse(&glow, c.x - rr, c.y - rr, rr * 2, rr * 2);
    }

    // الحلقات
    for (int i = 0; i < cs.ringCount; i++) {
        int rr = r - i * 5;
        if (rr <= 0) break;
        REAL thickness = (REAL)cs.ringThickness + (hover ? 0.5f : 0.0f);
        Pen ring(Color(255, R, G, B), thickness);
        RectF rect((REAL)(c.x - rr), (REAL)(c.y - rr), (REAL)(rr * 2), (REAL)(rr * 2));
        if (cs.dashed) {
            for (float angle = 0.0f; angle < 360.0f; angle += 22.0f) {
                gfx.DrawArc(&ring, rect, angle, 14.0f);
            }
        }
        else {
            gfx.DrawEllipse(&ring, c.x - rr, c.y - rr, rr * 2, rr * 2);
        }
    }
}
static void StyleFlame(Graphics& gfx, POINT c, int r, COLORREF color, bool hover) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    BYTE r2 = (BYTE)min(255, R + 100);
    BYTE g2 = (BYTE)min(255, G + 40);
    DrawSoftShadow(gfx, c, r);
    RectF rect((REAL)(c.x - r), (REAL)(c.y - r), (REAL)(r * 2), (REAL)(r * 2));
    for (int i = 0; i < 4; i++) {
        int alpha = 220 - i * 45;
        Pen p(Color((BYTE)alpha, R, G, B), (REAL)(hover ? 4.0f - i * 0.6f : 3.0f - i * 0.5f));
        p.SetStartCap(LineCapRound);
        gfx.DrawArc(&p, rect, -60.0f - i * 15.0f, 120.0f + i * 30.0f);
    }
    for (int i = 2; i >= 1; i--) {
        Pen glow(Color((BYTE)(40 * i), r2, g2, B), (REAL)(i * 3));
        gfx.DrawArc(&glow, rect, -90.0f, 90.0f);
    }
}

typedef void (*RadialStyleFn)(Graphics&, POINT, int, COLORREF, bool);
static RadialStyleFn GetStyleFn(RadialStyle style) {
    switch (style) {
    case RadialStyle::Neon:     return StyleNeon;
    case RadialStyle::Gradient: return StyleGradient;
    case RadialStyle::Aurora:   return StyleAurora;
    case RadialStyle::Ripple:   return StyleRipple;
    case RadialStyle::Flame:    return StyleFlame;
    case RadialStyle::Custom:   return StyleCustom;
    default:                    return StyleNeon;
    }
}


static void DrawFavoriteBadge(Graphics& gfx, POINT c, int r) {
    REAL cx = (REAL)c.x;
    REAL cy = (REAL)c.y - (REAL)r - 6;
    REAL outer = 7.0f, inner = 3.2f;
    PointF pts[10];
    for (int i = 0; i < 10; i++) {
        REAL ang = (REAL)(PI_CONST / 180.0 * (36.0 * i - 90.0));
        REAL rad = (i % 2 == 0) ? outer : inner;
        pts[i] = PointF(cx + rad * (REAL)cos(ang), cy + rad * (REAL)sin(ang));
    }
    SolidBrush fill(Color(255, 250, 204, 21));
    Pen outline(Color(255, 180, 140, 10), 1.0f);
    gfx.FillPolygon(&fill, pts, 10);
    gfx.DrawPolygon(&outline, pts, 10);
}
static void DrawMuteBadge(Graphics& gfx, POINT c, int r) {
    REAL cx = (REAL)(c.x - r + 5);
    REAL cy = (REAL)(c.y - r + 5);
    REAL rad = 8.5f;
    for (int i = 3; i >= 1; i--) {
        Pen halo(Color(28, 239, 68, 68), (REAL)(i * 1.8f));
        gfx.DrawEllipse(&halo, cx - rad - i, cy - rad - i, (rad + i) * 2, (rad + i) * 2);
    }
    SolidBrush bg(Color(210, 15, 12, 29));
    gfx.FillEllipse(&bg, cx - rad, cy - rad, rad * 2, rad * 2);
    Pen ring(Color(220, 248, 250, 252), 1.2f);
    gfx.DrawEllipse(&ring, cx - rad, cy - rad, rad * 2, rad * 2);
    Pen slash(Color(255, 239, 68, 68), 1.9f);
    slash.SetStartCap(LineCapRound);
    slash.SetEndCap(LineCapRound);
    gfx.DrawLine(&slash, cx - rad * 0.55f, cy - rad * 0.55f, cx + rad * 0.55f, cy + rad * 0.55f);
}

static void DrawQuickSlotBadge(Graphics& gfx, POINT c, int r, int slot) {
    if (slot < 1 || slot > 9) return;
    REAL rad = 10.0f;
    REAL cx = (REAL)(c.x + r - 3);
    REAL cy = (REAL)(c.y + r - 3);
    for (int i = 2; i >= 1; i--) {
        Pen halo(Color(50, 168, 85, 247), (REAL)(i * 2.5f));
        gfx.DrawEllipse(&halo, cx - rad - i, cy - rad - i, (rad + i) * 2, (rad + i) * 2);
    }
    SolidBrush bg(Color(235, 15, 12, 29));
    gfx.FillEllipse(&bg, cx - rad, cy - rad, rad * 2, rad * 2);
    Pen ring(Color(255, 168, 85, 247), 1.6f);
    gfx.DrawEllipse(&ring, cx - rad, cy - rad, rad * 2, rad * 2);
    FontFamily ff(L"Segoe UI");
    Font font(&ff, 13.0f, FontStyleBold, UnitPixel);
    SolidBrush textBrush(Color(255, 255, 255, 255));
    std::wstring txt = std::to_wstring(slot);
    RectF layoutRect(cx - rad, cy - rad, rad * 2, rad * 2);
    StringFormat sf;
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    gfx.DrawString(txt.c_str(), -1, &font, layoutRect, &sf, &textBrush);
}

static void DrawHoverHalo(Graphics& gfx, POINT center, int r, COLORREF color) {
    int R = GetRValue(color), G = GetGValue(color), B = GetBValue(color);
    for (int i = 3; i >= 1; i--) {
        BYTE alpha = (BYTE)(50 - i * 12);
        Pen halo(Color(alpha, R, G, B), (REAL)(i * 2.5f));
        int hr = r + i * 2 + 3;
        gfx.DrawEllipse(&halo,
                        (REAL)(center.x - hr), (REAL)(center.y - hr),
                        (REAL)(hr * 2), (REAL)(hr * 2));
    }
}

static void DrawCircleIcon(Graphics& gfx, POINT center, int radius,
                            const GameEntry& game, HICON icon, bool hover,
                            RadialStyle style, bool isMuted) {
    if (g_radialNoGlow) {
        int r = hover ? radius + 6 : radius;
        bool hasCustomBg = !game.radialBgPath.empty();
        bool replaceIcon = hasCustomBg && game.hideOriginalIcon;
        DrawRadialCustomBackground(gfx, center, r, game.radialBgPath, replaceIcon);
        if (!replaceIcon) {
            DrawIconOnly(gfx, center, r, icon, game.name);
        }
        if (game.favorite) DrawFavoriteBadge(gfx, center, r);
        if (isMuted) DrawMuteBadge(gfx, center, r);
        DrawQuickSlotBadge(gfx, center, r, game.quickSlot);
        return;
    }

    int r = hover ? radius + 10 : radius;
    bool hasCustomBg = !game.radialBgPath.empty();
    bool replaceIcon = hasCustomBg && game.hideOriginalIcon;
    if (hover) DrawHoverHalo(gfx, center, r, game.color);
    DrawRadialCustomBackground(gfx, center, r, game.radialBgPath, replaceIcon);
    GetStyleFn(style)(gfx, center, r, game.color, hover);
    if (!replaceIcon) {
        DrawIconBackdrop(gfx, center, r, game.radialBgPath);
        DrawIconOnly(gfx, center, r, icon, game.name);
    }
    DrawContainmentEdge(gfx, center, r);
    if (game.favorite) DrawFavoriteBadge(gfx, center, r);
    if (isMuted) DrawMuteBadge(gfx, center, r);
    DrawQuickSlotBadge(gfx, center, r, game.quickSlot);
}

static void DrawHub(Graphics& gfx, bool hover) {
    bool showFull = hover || g_hubAlwaysVisible || g_isFirstRun;

    if (!showFull) {
        int r = g_hubRadius;
        SolidBrush invisible(Color(1, 0, 0, 0));
        gfx.FillEllipse(&invisible,
                        (REAL)(g_center.x - r), (REAL)(g_center.y - r),
                        (REAL)(r * 2), (REAL)(r * 2));
        return;
    }

    int r = g_hubRadius + 4;
    SolidBrush shadow(Color(80, 0, 0, 0));
    gfx.FillEllipse(&shadow, (REAL)(g_center.x - r + 2), (REAL)(g_center.y - r + 4),
                    (REAL)(r * 2), (REAL)(r * 2));
    SolidBrush brush(Color(255, 90, 90, 96));
    gfx.FillEllipse(&brush, (REAL)(g_center.x - r), (REAL)(g_center.y - r),
                    (REAL)(r * 2), (REAL)(r * 2));

    if (g_isFirstRun && !hover && !g_hubAlwaysVisible) {
        for (int i = 3; i >= 1; i--) {
            Pen pulse(Color(40 - i * 8, 250, 204, 21), (REAL)(i * 4));
            int pr = r + i * 3;
            gfx.DrawEllipse(&pulse, (REAL)(g_center.x - pr), (REAL)(g_center.y - pr),
                            (REAL)(pr * 2), (REAL)(pr * 2));
        }
    }

    Pen ring(Color(180, 168, 85, 247), 2.0f);
    gfx.DrawEllipse(&ring, (REAL)(g_center.x - r), (REAL)(g_center.y - r),
                    (REAL)(r * 2), (REAL)(r * 2));
    Pen pen(Color(255, 255, 255, 255), 4);
    gfx.DrawLine(&pen, (INT)(g_center.x - 14), (INT)g_center.y, (INT)(g_center.x + 14), (INT)g_center.y);
    gfx.DrawLine(&pen, (INT)g_center.x, (INT)(g_center.y - 14), (INT)g_center.x, (INT)(g_center.y + 14));

    if (g_isFirstRun && !g_hubAlwaysVisible) {
        FontFamily ff(L"Segoe UI");
        Font font(&ff, 12.0f, FontStyleBold, UnitPixel);

        std::wstring hint = (g_lang == Lang::EN)
            ? L"Click here to open the control panel"
            : L"اضغط هنا لفتح لوحة التحكم";

        RectF measureRect(0, 0, 600, 60);
        RectF measured;
        gfx.MeasureString(hint.c_str(), -1, &font, measureRect, &measured);

        REAL tw = measured.Width + 24;
        REAL th = measured.Height + 12;
        REAL tx = (REAL)g_center.x - tw / 2.0f;
        REAL ty = (REAL)g_center.y + (REAL)r + 14.0f;

        SolidBrush bgBrush(Color(220, 15, 12, 29));
        gfx.FillRectangle(&bgBrush, tx, ty, tw, th);

        Pen borderPen(Color(180, 250, 204, 21), 1.5f);
        gfx.DrawRectangle(&borderPen, tx, ty, tw, th);

        SolidBrush textBrush(Color(255, 250, 240, 200));
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        gfx.DrawString(hint.c_str(), -1, &font, RectF(tx, ty, tw, th), &sf, &textBrush);
    }
}

static Bitmap* GetCachedRingBackground(const std::wstring& bgPath, int w, int h) {
    if (bgPath.empty()) { ClearRingBgCache(); return nullptr; }
    ULONGLONG sig = GetFileSignature(bgPath);
    if (bgPath == g_ringBgSourcePath && sig == g_ringBgSourceSig &&
        w == g_ringBgCachedW && h == g_ringBgCachedH && g_ringBgCachedBmp)
        return g_ringBgCachedBmp;
    Bitmap* src = Bitmap::FromFile(bgPath.c_str());
    if (!src || src->GetLastStatus() != Ok) {
        if (src) delete src;
        ClearRingBgCache();
        return nullptr;
    }
    REAL imgW = (REAL)src->GetWidth(), imgH = (REAL)src->GetHeight();
    Bitmap* scaled = nullptr;
    if (imgW > 0 && imgH > 0 && w > 0 && h > 0) {
        scaled = new Bitmap(w, h, PixelFormat32bppARGB);
        if (scaled && scaled->GetLastStatus() == Ok) {
            Graphics gfx(scaled);
            gfx.SetSmoothingMode(SmoothingModeAntiAlias);
            gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            gfx.SetPixelOffsetMode(PixelOffsetModeHighQuality);
            gfx.Clear(Color(0, 0, 0, 0));
            REAL scale = max((REAL)w / imgW, (REAL)h / imgH);
            REAL dw = imgW * scale, dh = imgH * scale;
            REAL dx = ((REAL)w - dw) / 2.0f, dy = ((REAL)h - dh) / 2.0f;
            gfx.DrawImage(src, RectF(dx, dy, dw, dh), 0.0f, 0.0f, imgW, imgH, UnitPixel);
        } else { delete scaled; scaled = nullptr; }
    }
    delete src;
    if (!scaled) { ClearRingBgCache(); return nullptr; }
    if (g_ringBgCachedBmp) delete g_ringBgCachedBmp;
    g_ringBgCachedBmp = scaled;
    g_ringBgSourcePath = bgPath;
    g_ringBgSourceSig = sig;
    g_ringBgCachedW = w; g_ringBgCachedH = h;
    return g_ringBgCachedBmp;
}

static void DrawDimBackdrop(Graphics& gfx, int w, int h) {
    REAL ringR = (REAL)(g_orbitDistance + g_gameRadius + 40);
    GraphicsPath circlePath;
    circlePath.AddEllipse((REAL)g_center.x - ringR, (REAL)g_center.y - ringR,
                          ringR * 2, ringR * 2);

    PathGradientBrush brush(&circlePath);
    brush.SetCenterColor(Color(160, 8, 6, 18));
    int cnt = 1;
    Color surround(0, 8, 6, 18);
    brush.SetSurroundColors(&surround, &cnt);
    gfx.FillPath(&brush, &circlePath);
}

static void DrawCustomRadialBackground(Graphics& gfx, int w, int h) {
    std::wstring bgPath = LoadRadialBackground();
    if (bgPath.empty()) return;
    Bitmap* bmp = GetCachedRingBackground(bgPath, w, h);
    if (!bmp) return;
    REAL ringR = (REAL)(g_orbitDistance + g_gameRadius + 18);
    GraphicsPath clipPath;
    clipPath.AddEllipse((REAL)g_center.x - ringR, (REAL)g_center.y - ringR, ringR * 2, ringR * 2);
    gfx.SetClip(&clipPath);

    ColorMatrix cm = {1,0,0,0,0, 0,1,0,0,0, 0,0,1,0,0, 0,0,0,g_radialTransparency,0, 0,0,0,0,1};
    ImageAttributes ia;
    ia.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);
    gfx.DrawImage(bmp, Rect(0, 0, w, h), 0, 0, w, h, UnitPixel, &ia);

    PathGradientBrush edgeFade(&clipPath);
    edgeFade.SetCenterColor(Color(0, 0, 0, 0));
    int cnt = 1;
    Color surround(230, 13, 11, 24);
    edgeFade.SetSurroundColors(&surround, &cnt);
    gfx.FillPath(&edgeFade, &clipPath);
    gfx.ResetClip();
}

static void PaintPopup() {
    if (!g_popupWnd) return;
    RECT rc; GetClientRect(g_popupWnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bmp = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP old = (HBITMAP)SelectObject(memDC, bmp);
    {
        Graphics gfx(memDC);
        gfx.SetSmoothingMode(SmoothingModeAntiAlias);
        gfx.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        gfx.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        gfx.Clear(Color(0, 0, 0, 0));
        DrawCustomRadialBackground(gfx, w, h);
        RadialStyle style = LoadRadialStyle();
        int total = (int)g_visibleGameIndices.size();
        for (int i = 0; i < total; i++) {
            int idx = g_visibleGameIndices[i];
            POINT p = GamePos(i, total);
            HICON ic = GetCachedIcon(g_games[idx].exePath);
            bool isMuted = g_muteCache.count(g_games[idx].exePath) > 0;
            DrawCircleIcon(gfx, p, g_gameRadius, g_games[idx], ic,
                g_hover == idx, style, isMuted);
        }
        DrawHub(gfx, g_hover == -1);
    }
    POINT ptSrc = { 0, 0 };
    SIZE size = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    RECT wrc; GetWindowRect(g_popupWnd, &wrc);
    POINT ptDst = { wrc.left, wrc.top };
    UpdateLayeredWindow(g_popupWnd, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    SelectObject(memDC, old);
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

static void OpenPanel() {
    MarkFirstRunDone();
    g_isFirstRun = false;

    wchar_t exePath[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return;
    LaunchPath(exePath, false, L"--panel");
}

static int KbNextIndex(int current, int dir) {
    int total = (int)g_visibleGameIndices.size();
    if (total == 0) return -2;
    int pos = -1;
    if (current == -1) pos = (dir > 0) ? -1 : total;
    else {
        for (int i = 0; i < total; i++) if (g_visibleGameIndices[i] == current) { pos = i; break; }
        if (pos == -1) pos = (dir > 0) ? -1 : 0;
    }
    pos += dir;
    if (pos < -1) pos = total - 1;
    if (pos >= total) pos = -1;
    if (pos == -1) return -1;
    return g_visibleGameIndices[pos];
}

static int FindGameByQuickSlot(int slot) {
    if (slot < 1 || slot > 9) return -1;
    for (size_t i = 0; i < g_games.size(); i++) {
        if (g_games[i].quickSlot == slot && g_games[i].showInRadial) return (int)i;
    }
    return -1;
}

static LRESULT CALLBACK PopupProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTest(pt);
        if (hit != g_hover) { g_hover = hit; PaintPopup(); }
        TRACKMOUSEEVENT tme = { sizeof(tme) };
        tme.dwFlags = TME_LEAVE; tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        if (g_hover != -2) { g_hover = -2; PaintPopup(); }
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = HitTest(pt);
        if (hit >= 0) { LaunchGame(g_games[hit]); g_shouldClosePopup = true; }
        else if (hit == -1) { OpenPanel(); g_shouldClosePopup = true; }
        else { g_shouldClosePopup = true; }
        return 0;
    }
    case WM_KEYDOWN: {
        if (wp == VK_ESCAPE) { g_shouldClosePopup = true; return 0; }
        if (wp == VK_LEFT || wp == VK_UP) { int n = KbNextIndex(g_hover, -1); if (n != g_hover) { g_hover = n; PaintPopup(); } return 0; }
        if (wp == VK_RIGHT || wp == VK_DOWN) { int n = KbNextIndex(g_hover, +1); if (n != g_hover) { g_hover = n; PaintPopup(); } return 0; }
        if (wp == VK_RETURN || wp == VK_SPACE) {
            if (g_hover == -1) OpenPanel();
            else if (g_hover >= 0) LaunchGame(g_games[g_hover]);
            g_shouldClosePopup = true;
            return 0;
        }
        if (wp >= '1' && wp <= '9') {
            int slot = (int)(wp - '0');
            int idx = FindGameByQuickSlot(slot);
            if (idx >= 0) {
                LaunchGame(g_games[idx]);
                g_shouldClosePopup = true;
            }
            return 0;
        }
        return 0;
    }
    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) g_shouldClosePopup = true;
        return 0;
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowRadialPopup() {
    if (g_popupWnd) return;
    LoadGames(g_games);

    g_hubAlwaysVisible = LoadHubAlwaysVisible();
    g_isFirstRun = IsFirstRun();

    RadialScale scale = LoadRadialScale();
    g_hubRadius = scale.hubSize;
    g_gameRadius = scale.iconSize;
    g_baseOrbit = scale.orbitDist;

    RebuildVisibleIndices();
    ComputeOrbitDistance();
    DetectRunningGameProcesses();
    RefreshMuteCache();
    BuildIconCache();
    g_radialTransparency = LoadRadialTransparency();
    g_radialNoGlow = LoadRadialNoGlow();

    POINT cursor;
    GetCursorPos(&cursor);
    HMONITOR hmon = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = { sizeof(mi) };
    GetMonitorInfoW(hmon, &mi);
    int monitorX = mi.rcWork.left, monitorY = mi.rcWork.top;
    int monitorW = mi.rcWork.right - mi.rcWork.left;
    int monitorH = mi.rcWork.bottom - mi.rcWork.top;

    int maxWinSize = min(monitorW, monitorH) - 20;
    int maxOrbit = maxWinSize / 2 - g_gameRadius - WINDOW_MARGIN;
    if (maxOrbit < g_baseOrbit) maxOrbit = g_baseOrbit;
    if (g_orbitDistance > maxOrbit) g_orbitDistance = maxOrbit;

    int winSize = (g_orbitDistance + g_gameRadius + WINDOW_MARGIN) * 2;
    int wx = monitorX + (monitorW - winSize) / 2;
    int wy = monitorY + (monitorH - winSize) / 2;

    g_popupWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        POPUP_CLASS, L"GameLauncherPopup", WS_POPUP,
        wx, wy, winSize, winSize, nullptr, nullptr, g_hInst, nullptr);

    g_center.x = winSize / 2;
    g_center.y = winSize / 2;
    POINT ptCursor;
    GetCursorPos(&ptCursor);
    ScreenToClient(g_popupWnd, &ptCursor);
    g_hover = HitTest(ptCursor);
    g_shouldClosePopup = false;

    g_inRadialLastButtons = PollAllControllers();
    g_inRadialLastMoveTime = 0;
    g_controllerSuppressUntil = GetTickCount64() + 400;

    ShowWindow(g_popupWnd, SW_SHOW);
    SetForegroundWindow(g_popupWnd);
    SetFocus(g_popupWnd);
    PaintPopup();

    MSG msg;
    while (!g_shouldClosePopup) {
        DWORD wait = MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { g_shouldClosePopup = true; break; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
        PollControllerInRadial();
    }
    DestroyWindow(g_popupWnd);
    g_popupWnd = nullptr;
}

// ============================================================
// Overlay
// ============================================================
static ULONGLONG g_lastCpuIdle = 0, g_lastCpuKernel = 0, g_lastCpuUser = 0;
static float g_cpuPercent = 0.0f;
static float g_ramPercent = 0.0f;
static ULONGLONG g_ramUsedMB = 0, g_ramTotalMB = 0;

static void InitCpuMeasure() {
    FILETIME idle, kernel, user;
    if (GetSystemTimes(&idle, &kernel, &user)) {
        g_lastCpuIdle   = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
        g_lastCpuKernel = ((ULONGLONG)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
        g_lastCpuUser   = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;
    }
}

static void UpdateCpuMeasure() {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) return;
    ULONGLONG i = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
    ULONGLONG k = ((ULONGLONG)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
    ULONGLONG u = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;
    ULONGLONG dIdle   = i - g_lastCpuIdle;
    ULONGLONG dKernel = k - g_lastCpuKernel;
    ULONGLONG dUser   = u - g_lastCpuUser;
    ULONGLONG dTotal = dKernel + dUser;
    if (dTotal > 0) {
        float busy = (float)(dTotal - dIdle) / (float)dTotal * 100.0f;
        g_cpuPercent = g_cpuPercent * 0.6f + busy * 0.4f;
        if (g_cpuPercent < 0) g_cpuPercent = 0;
        if (g_cpuPercent > 100) g_cpuPercent = 100;
    }
    g_lastCpuIdle = i; g_lastCpuKernel = k; g_lastCpuUser = u;
}

static void UpdateRamMeasure() {
    MEMORYSTATUSEX ms = { sizeof(ms) };
    if (GlobalMemoryStatusEx(&ms)) {
        g_ramPercent = (float)ms.dwMemoryLoad;
        g_ramTotalMB = ms.ullTotalPhys / (1024ULL * 1024ULL);
        ULONGLONG used = (ms.ullTotalPhys - ms.ullAvailPhys) / (1024ULL * 1024ULL);
        g_ramUsedMB = used;
    }
}

static void PaintOverlay() {
    if (!g_overlayWnd) return;
    RECT rc; GetClientRect(g_overlayWnd, &rc);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    PerfBlackBox::PerformanceSample live;
    bool hasLive = false;
    {
        auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
        if (pm.HasLiveData()) {
            live = pm.GetLatestSample();
            hasLive = true;
        }
    }

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bmp = CreateCompatibleBitmap(screenDC, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);
    {
        Graphics gfx(memDC);
        gfx.SetSmoothingMode(SmoothingModeAntiAlias);
        gfx.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        gfx.Clear(Color(0, 0, 0, 0));

        int alpha = (int)(g_overlaySettings.opacity * 180);
        if (alpha < 60) alpha = 60;
        if (alpha > 200) alpha = 200;

        SolidBrush bgBrush(Color((BYTE)alpha, 8, 8, 14));
        gfx.FillRectangle(&bgBrush, 0, 0, w, h);

        FontFamily ff(L"Segoe UI");
        Font fontBig(&ff, 20.0f, FontStyleBold, UnitPixel);
        Font fontLabel(&ff, 10.0f, FontStyleRegular, UnitPixel);
        Font fontValue(&ff, 12.0f, FontStyleBold, UnitPixel);

        SolidBrush shadowBrush(Color(255, 0, 0, 0));
        SolidBrush greenBrush(Color(255, 16, 185, 129));
        SolidBrush orangeBrush(Color(255, 249, 115, 22));
        SolidBrush cyanBrush(Color(255, 6, 182, 212));
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        SolidBrush labelBrush(Color(200, 180, 180, 190));

        REAL x = 12.0f;
        REAL y = 8.0f;

        // FPS — كبير
        {
            std::wstring fpsStr = L"\x2014";
            if (hasLive && live.fps >= 0) {
                wchar_t buf[32]; swprintf_s(buf, 32, L"%.0f", (double)live.fps);
                fpsStr = buf;
            }
            gfx.DrawString(fpsStr.c_str(), -1, &fontBig, PointF(x + 1, y + 1), &shadowBrush);
            gfx.DrawString(fpsStr.c_str(), -1, &fontBig, PointF(x, y), &greenBrush);

            gfx.DrawString(L"FPS", -1, &fontLabel, PointF(x + 61, y + 9), &shadowBrush);
            gfx.DrawString(L"FPS", -1, &fontLabel, PointF(x + 60, y + 8), &labelBrush);
            y += 28;
        }

        // GPU
        {
            int gpuTemp = (hasLive && live.gpuTempC >= 0) ? (int)live.gpuTempC : -1;
            int gpuUse = (hasLive && live.gpuUsage >= 0) ? (int)live.gpuUsage : -1;
            int gpuPow = (hasLive && live.gpuPowerW >= 0) ? (int)live.gpuPowerW : -1;
            wchar_t buf[64];
            if (gpuTemp >= 0 || gpuUse >= 0) {
                if (gpuPow >= 0)
                    swprintf_s(buf, 64, L"GPU  %d\x00B0""C   %d%%   %dW", gpuTemp, gpuUse, gpuPow);
                else
                    swprintf_s(buf, 64, L"GPU  %d\x00B0""C   %d%%", gpuTemp, gpuUse);
            }
            else {
                swprintf_s(buf, 64, L"GPU  \x2014");
            }
            gfx.DrawString(buf, -1, &fontValue, PointF(x + 1, y + 1), &shadowBrush);
            gfx.DrawString(buf, -1, &fontValue, PointF(x, y), &orangeBrush);
            y += 18;
        }

        // CPU
        {
            wchar_t buf[64];
            swprintf_s(buf, 64, L"CPU  %.0f%%", (double)g_cpuPercent);
            gfx.DrawString(buf, -1, &fontValue, PointF(x + 1, y + 1), &shadowBrush);
            gfx.DrawString(buf, -1, &fontValue, PointF(x, y), &cyanBrush);
            y += 18;
        }

        // RAM
        {
            wchar_t buf[64];
            swprintf_s(buf, 64, L"RAM  %llu / %llu MB", g_ramUsedMB, g_ramTotalMB);
            gfx.DrawString(buf, -1, &fontValue, PointF(x + 1, y + 1), &shadowBrush);
            gfx.DrawString(buf, -1, &fontValue, PointF(x, y), &whiteBrush);
            y += 18;
        }

        // Playtime
        {
            std::wstring playStr = L"\x2014";
            if (!g_runningGames.empty()) {
                ULONGLONG total = 0;
                for (auto& rg : g_runningGames) total += rg.totalSeconds;
                ULONGLONG hh = total / 3600, mm = (total % 3600) / 60, ss = total % 60;
                wchar_t tbuf[32];
                swprintf_s(tbuf, 32, L"%02llu:%02llu:%02llu", hh, mm, ss);
                playStr = tbuf;
            }
            gfx.DrawString(playStr.c_str(), -1, &fontValue, PointF(x + 1, y + 1), &shadowBrush);
            gfx.DrawString(playStr.c_str(), -1, &fontValue, PointF(x, y), &labelBrush);
        }
    }
    POINT ptSrc = { 0, 0 };
    SIZE size = { w, h };
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    RECT wrc; GetWindowRect(g_overlayWnd, &wrc);
    POINT ptDst = { wrc.left, wrc.top };
    UpdateLayeredWindow(g_overlayWnd, screenDC, &ptDst, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);
}

static const int OVERLAY_W = 220;
static const int OVERLAY_H = 116;

static void RepaintOverlay() {
    if (g_overlayWnd) PaintOverlay();
}

static void UpdateOverlayContent() {
    if (!g_overlayWnd) return;
    UpdateCpuMeasure();
    UpdateRamMeasure();

    static float     lastCpu      = -999.0f;
    static ULONGLONG lastRamUsed  = 0;
    static float     lastFps      = -999.0f;
    static float     lastGpuTemp  = -999.0f;
    static float     lastGpuUse   = -999.0f;
    static float     lastGpuPower = -999.0f;
    static ULONGLONG lastPlay     = (ULONGLONG)-1;

    PerfBlackBox::PerformanceSample live;
    bool hasLive = false;
    {
        auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
        if (pm.HasLiveData()) { live = pm.GetLatestSample(); hasLive = true; }
    }
    float curFps      = (hasLive && live.fps       >= 0) ? live.fps       : -1.0f;
    float curGpuTemp  = (hasLive && live.gpuTempC  >= 0) ? live.gpuTempC  : -1.0f;
    float curGpuUse   = (hasLive && live.gpuUsage  >= 0) ? live.gpuUsage  : -1.0f;
    float curGpuPower = (hasLive && live.gpuPowerW >= 0) ? live.gpuPowerW : -1.0f;

    ULONGLONG curPlay = 0;
    for (auto& rg : g_runningGames) curPlay += rg.totalSeconds;

    bool changed =
        fabs(curFps      - lastFps)      >= 0.5f ||
        fabs(curGpuTemp  - lastGpuTemp)  >= 1.0f ||
        fabs(curGpuUse   - lastGpuUse)   >= 1.0f ||
        fabs(curGpuPower - lastGpuPower) >= 1.0f ||
        fabs(g_cpuPercent - lastCpu)     >= 1.0f ||
        lastRamUsed != g_ramUsedMB ||
        lastPlay    != curPlay;

    if (!changed) return;

    lastFps      = curFps;
    lastGpuTemp  = curGpuTemp;
    lastGpuUse   = curGpuUse;
    lastGpuPower = curGpuPower;
    lastCpu      = g_cpuPercent;
    lastRamUsed  = g_ramUsedMB;
    lastPlay     = curPlay;

    PaintOverlay();
}

static void OverlayProc_Mouse(HWND hwnd, POINT pt) {
    (void)pt;
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}


static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        OverlayProc_Mouse(hwnd, pt);
        return 0;
    }
    case WM_RBUTTONUP:
        HideOverlay();
        return 0;
    case WM_TIMER:
        if (wp == OVERLAY_TIMER_ID) { UpdateOverlayContent(); }
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, OVERLAY_TIMER_ID);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowOverlay() {
    if (g_overlayWnd) {
        ShowWindow(g_overlayWnd, SW_SHOW);
        SetWindowPos(g_overlayWnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        UpdateOverlayContent();
        return;
    }
    int px = g_overlaySettings.posX;
    int py = g_overlaySettings.posY;
    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    if (px < wa.left) px = wa.left + 10;
    if (py < wa.top) py = wa.top + 10;
    if (px + OVERLAY_W > wa.right) px = wa.right - OVERLAY_W - 10;
    if (py + OVERLAY_H > wa.bottom) py = wa.bottom - OVERLAY_H - 10;

    g_overlayWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        OVERLAY_CLASS, L"GameLauncherOverlay", WS_POPUP,
        px, py, OVERLAY_W, OVERLAY_H, nullptr, nullptr, g_hInst, nullptr);

    if (!g_overlayWnd) {
        WriteLog(L"Failed to create overlay window");
        return;
    }

    InitCpuMeasure();
    UpdateRamMeasure();
    SetTimer(g_overlayWnd, OVERLAY_TIMER_ID, 500, nullptr);
    ShowWindow(g_overlayWnd, SW_SHOWNOACTIVATE);
    SetWindowPos(g_overlayWnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    UpdateOverlayContent();
    WriteLog(L"Overlay shown");
}

static void HideOverlay() {
    if (!g_overlayWnd) return;
    DestroyWindow(g_overlayWnd);
    g_overlayWnd = nullptr;
    WriteLog(L"Overlay hidden");
}

static void ToggleOverlay() {
    if (g_overlayWnd) HideOverlay();
    else ShowOverlay();
}

// ----------------------------- Tray -----------------------------
static void AddTrayIcon(HWND hwnd) {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = (HICON)LoadImageW(g_hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!g_nid.hIcon) g_nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    std::wstring tip = L"GameLauncher (" + HotkeyToString(g_hotkey) + L")" + TXT(L" — يعمل", L" — running");
    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void UpdateTrayTooltip() {
    std::wstring tip = L"GameLauncher (" + HotkeyToString(g_hotkey) + L")" + TXT(L" — يعمل", L" — running");
    wcsncpy_s(g_nid.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void ShowTrayMenu(HWND hwnd) {
    g_hotkey = LoadHotkey();
    g_muteSettings = LoadMuteSettings();
    g_overlaySettings = LoadOverlaySettings();
    HMENU menu = CreatePopupMenu();
    std::wstring showLabel = TXT(L"فتح دائرة الألعاب (", L"Open Game Circle (")
                           + HotkeyToString(g_hotkey) + L")";
    AppendMenuW(menu, MF_STRING, ID_TRAY_SHOW, showLabel.c_str());
    if (g_muteSettings.enabled) {
        std::wstring muteLabel = TXT(L"كتم/إلغاء كتم التطبيق النشط (",
                                      L"Toggle mute for active app (")
                               + MuteHotkeyToString(g_muteSettings) + L")";
        AppendMenuW(menu, MF_STRING, ID_TRAY_MUTE, muteLabel.c_str());
    }
    std::wstring ovLabel = (g_overlayWnd ? TXT(L"إخفاء الـ Overlay", L"Hide Overlay")
        : TXT(L"إظهار الـ Overlay", L"Show Overlay"));

    AppendMenuW(menu, MF_STRING, ID_TRAY_OVERLAY, ovLabel.c_str());

    std::vector<MuteAudio::MutedAppInfo> muted = MuteAudio::GetMutedApps();
    if (!muted.empty()) {
        HMENU sub = CreatePopupMenu();
        g_trayMutedExes.clear();
        size_t count = muted.size();
        if (count > (size_t)(ID_MUTED_MAX - ID_MUTED_BASE)) count = ID_MUTED_MAX - ID_MUTED_BASE;
        for (size_t i = 0; i < count; i++) {
            AppendMenuW(sub, MF_STRING, ID_MUTED_BASE + (UINT)i, muted[i].displayName.c_str());
            g_trayMutedExes.push_back(muted[i].exeName);
        }
        AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(sub, MF_STRING, ID_MUTED_ALL, TXT(L"إلغاء كتم الكل", L"Unmute all"));
        std::wstring subLabel = TXT(L"التطبيقات المكتومة (", L"Muted applications (")
                              + std::to_wstring(muted.size()) + L")";
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, subLabel.c_str());
    }
    AppendMenuW(menu, MF_STRING, ID_TRAY_PANEL, TXT(L"لوحة التحكم...", L"Control Panel..."));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_TRAY_EXIT, TXT(L"إغلاق البرنامج نهائياً", L"Exit GameLauncher"));
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
}
static void ReloadHotkeyNow(HWND hwnd) {
    UnregisterHotKey(hwnd, HOTKEY_ID);
    g_hotkey = LoadHotkey();
    if (!RegisterHotKey(hwnd, HOTKEY_ID, g_hotkey.modifiers, g_hotkey.vk)) {
        WriteLog(L"FAILED to register main hotkey");
        MessageBoxW(nullptr,
            TXT(L"تعذّر تسجيل الاختصار الجديد.", L"Couldn't register the new hotkey."),
            L"GameLauncher", MB_OK | MB_ICONWARNING);
    } else WriteLog(L"Main hotkey registered");
    g_lang = LoadLanguage();
    UpdateTrayTooltip();
}
static void ReloadMuteHotkeyNow(HWND hwnd) {
    UnregisterHotKey(hwnd, MUTE_HOTKEY_ID);
    g_muteSettings = LoadMuteSettings();
    if (g_muteSettings.enabled) {
        if (RegisterHotKey(hwnd, MUTE_HOTKEY_ID, g_muteSettings.modifiers, g_muteSettings.vk))
            WriteLog(L"Mute hotkey registered: " + MuteHotkeyToString(g_muteSettings));
        else WriteLog(L"FAILED to register mute hotkey");
    }
}
static void ReloadOverlayNow() {
    OverlaySettings old = g_overlaySettings;
    g_overlaySettings = LoadOverlaySettings();
    if (g_hiddenWnd) {
        UnregisterHotKey(g_hiddenWnd, OVERLAY_HOTKEY_ID);
        if (g_overlaySettings.enabled) {
            if (RegisterHotKey(g_hiddenWnd, OVERLAY_HOTKEY_ID,
                               g_overlaySettings.hotkeyModifiers,
                               g_overlaySettings.hotkeyVk)) {
                WriteLog(L"Overlay hotkey registered");
            } else {
                WriteLog(L"FAILED to register overlay hotkey");
            }
        }
    }
    if (!g_overlaySettings.enabled && g_overlayWnd) {
        HideOverlay();
    }
    if (g_overlayWnd) PaintOverlay();
    (void)old;
}
static void ReloadControllerNow(HWND /*hwnd*/) {
    g_controllerSettings = LoadControllerSettings();
    g_radialTransparency = LoadRadialTransparency();
    g_radialNoGlow = LoadRadialNoGlow();

    RadialScale scale = LoadRadialScale();
    g_hubRadius = scale.hubSize;
    g_gameRadius = scale.iconSize;
    g_baseOrbit = scale.orbitDist;

    WriteLog(L"Controller/transparency/noglow/scale reloaded, enabled=" +
        std::to_wstring(g_controllerSettings.enabled ? 1 : 0) +
        L", scale=" + std::to_wstring(scale.iconSize) + L"/" +
        std::to_wstring(scale.hubSize) + L"/" +
        std::to_wstring(scale.orbitDist));
}

static void ToggleMuteForeground() {
    HWND fg = GetForegroundWindow();
    if (!fg) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (!pid || pid == GetCurrentProcessId()) return;
    std::wstring procName = L"?";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        wchar_t path[MAX_PATH] = L"";
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProc, 0, path, &size)) procName = GetFileNameFromPath(path);
        CloseHandle(hProc);
    }
    if (procName.empty() || procName == L"?") return;
    int exactState = MuteAudio::GetMuteStateByPid(pid);
    int nameState  = MuteAudio::GetMuteStateByExeName(procName);
    int effectiveState = (nameState != 0) ? nameState : exactState;
    if (effectiveState == 0) return;
    bool wantMute = (effectiveState != 2);
    MuteAudio::SetMuteByExeName(procName, wantMute);
    RefreshMuteCache();
    WriteMuteStateFile();
}

static LRESULT CALLBACK HiddenProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_HOTKEY:
        if (wp == HOTKEY_ID) ShowRadialPopup();
        else if (wp == MUTE_HOTKEY_ID) ToggleMuteForeground();
        else if (wp == OVERLAY_HOTKEY_ID) {
            if (g_overlaySettings.enabled) ToggleOverlay();
        }
        return 0;

    case WM_APP_CONTROLLER_TOGGLE:
        if (GetTickCount64() < g_controllerSuppressUntil) return 0;
        if (!g_controllerSettings.allowControllerDuringGame && IsAnyTrackedGameRunning()) return 0;

        if (g_popupWnd) {
            if (g_controllerSettings.toggleMode) {
                g_controllerSuppressUntil = GetTickCount64() + 600;
                g_shouldClosePopup = true;
            }
        } else {
            g_controllerSuppressUntil = GetTickCount64() + 400;
            ShowRadialPopup();
        }
        return 0;

    case WM_APP_RELOAD_HOTKEY: ReloadHotkeyNow(hwnd); return 0;
    case WM_APP_RELOAD_MUTE_HOTKEY: ReloadMuteHotkeyNow(hwnd); return 0;
    case WM_APP_RELOAD_CONTROLLER: ReloadControllerNow(hwnd); return 0;
    case WM_APP_RELOAD_OVERLAY: ReloadOverlayNow(); return 0;

    // ✅ زر "استرجاع إعدادات النظام"
    case WM_APP_RESTORE_BOOST_DEFAULTS: {
        RestoreAllBoostDefaults();
        WriteLog(L"[Boost] Manual restore triggered from panel");
        return 0;
    }

    case WM_APP_RESTART_AS_ADMIN: {
        wchar_t exePath[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, exePath, MAX_PATH)) return 0;

        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOASYNC;
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.lpParameters = L"--restart-admin";
        sei.nShow = SW_SHOWNORMAL;

        if (ShellExecuteExW(&sei)) {
            WriteLog(L"[Admin] Restarting as Administrator...");
            DestroyWindow(hwnd);
        } else {
            DWORD err = GetLastError();
            if (err == ERROR_CANCELLED) {
                WriteLog(L"[Admin] User cancelled UAC prompt");
            } else {
                WriteLog(L"[Admin] ShellExecuteExW failed (err=" + std::to_wstring(err) + L")");
            }
        }
        return 0;
    }

    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK || lp == NIN_SELECT) ShowRadialPopup();
        else if (lp == WM_RBUTTONUP) ShowTrayMenu(hwnd);
        return 0;
    case WM_TIMER:
        if (wp == LANG_WATCH_TIMER_ID) PollLayoutWatch();
        else if (wp == PLAY_TIME_TIMER_ID) PollRunningGames();
        return 0;
    case WM_COMMAND: {
        WORD id = LOWORD(wp);
        if (id >= ID_MUTED_BASE && id <= ID_MUTED_MAX) {
            size_t idx = (size_t)(id - ID_MUTED_BASE);
            if (idx < g_trayMutedExes.size()) {
                MuteAudio::SetMuteByExeName(g_trayMutedExes[idx], false);
                RefreshMuteCache();
                WriteMuteStateFile();
            }
            return 0;
        }
        if (id == ID_MUTED_ALL) {
            MuteAudio::UnmuteAll();
            RefreshMuteCache();
            WriteMuteStateFile();
            return 0;
        }
        switch (id) {
        case ID_TRAY_SHOW:  ShowRadialPopup(); break;
        case ID_TRAY_MUTE:  ToggleMuteForeground(); break;
        case ID_TRAY_PANEL: OpenPanel(); break;
        case ID_TRAY_OVERLAY:
            if (g_overlaySettings.enabled) ToggleOverlay();
            break;
        case ID_TRAY_EXIT:  DestroyWindow(hwnd); break;
        }
        return 0;
    }
    case WM_DESTROY:
        UnregisterHotKey(hwnd, HOTKEY_ID);
        UnregisterHotKey(hwnd, MUTE_HOTKEY_ID);
        UnregisterHotKey(hwnd, OVERLAY_HOTKEY_ID);
        Shell_NotifyIconW(NIM_DELETE, &g_nid);
        if (g_langWatchActive && g_langWatchProcess) {
            CloseHandle(g_langWatchProcess);
            g_langWatchProcess = nullptr;
        }
        if (g_overlayWnd) { DestroyWindow(g_overlayWnd); g_overlayWnd = nullptr; }
        ClearIconCache();
        ClearRingBgCache();
        FlushPlayTimeOnExit();
        WriteLog(L"GameLauncher shutting down");
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int RunEngineApp(HINSTANCE hInst) {
    HRESULT hrCom = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool comInitialized = SUCCEEDED(hrCom);

    HANDLE hSingleInstance = CreateMutexW(nullptr, TRUE, SINGLE_INSTANCE_MUTEX_NAME);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (comInitialized) CoUninitialize();
        return 0;
    }

    g_hInst = hInst;
    g_lang = LoadLanguage();
    GdiplusStartupInput gdiInput;
    GdiplusStartup(&g_gdiplusToken, &gdiInput, nullptr);

    StartFreshLog();

    // ✅ استرجاع أي تعديلات Boost معلّقة من crash سابق
    CheckAndRestoreStaleBoost();

    {
        BOOL isElevated = FALSE;
        PSID adminGroup = nullptr;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
            CheckTokenMembership(nullptr, adminGroup, &isElevated);
            FreeSid(adminGroup);
        }
        std::wstring adminFile = GetAppDataDir() + L"\\engine_admin.txt";
        std::wstring tmp = adminFile + L".tmp";
        {
            std::wofstream f(tmp.c_str(), std::ios::trunc);
            if (f.is_open()) f << (isElevated ? L"1" : L"0") << L"\n";
        }
        MoveFileExW(tmp.c_str(), adminFile.c_str(), MOVEFILE_REPLACE_EXISTING);
        WriteLog(L"[Admin] Engine elevated=" + std::to_wstring(isElevated ? 1 : 0));
    }

    AutoCleanupOnVersionChange();

    LoadGames(g_games);
    RebuildVisibleIndices();
    g_hotkey = LoadHotkey();
    g_muteSettings = LoadMuteSettings();
    g_controllerSettings = LoadControllerSettings();
    g_overlaySettings = LoadOverlaySettings();
    g_radialTransparency = LoadRadialTransparency();
    g_radialNoGlow = LoadRadialNoGlow();
    LoadManualMuteUris();
    InitXInputEx();

    DeleteFileW(RunningGamesPath().c_str());
    DeleteFileW((RunningGamesPath() + L".tmp").c_str());

    if (!std::ifstream(SettingsPath().c_str())) SaveHotkey(g_hotkey);
    if (!std::ifstream(MuteSettingsPath().c_str())) SaveMuteSettings(g_muteSettings);
    if (!std::ifstream(ControllerSettingsPath().c_str())) SaveControllerSettings(g_controllerSettings);
    if (!std::ifstream(OverlaySettingsPath().c_str())) SaveOverlaySettings(g_overlaySettings);

    WNDCLASSEXW hc = { sizeof(hc) };
    hc.lpfnWndProc = HiddenProc;
    hc.hInstance = hInst;
    hc.lpszClassName = HIDDEN_CLASS;
    RegisterClassExW(&hc);
    g_hiddenWnd = CreateWindowExW(0, HIDDEN_CLASS, L"GameLauncherHidden", WS_OVERLAPPEDWINDOW,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, hInst, nullptr);

    WNDCLASSEXW pc = { sizeof(pc) };
    pc.lpfnWndProc = PopupProc;
    pc.hInstance = hInst;
    pc.lpszClassName = POPUP_CLASS;
    pc.hCursor = LoadCursor(nullptr, IDC_HAND);
    RegisterClassExW(&pc);

    WNDCLASSEXW oc = { sizeof(oc) };
    oc.lpfnWndProc = OverlayProc;
    oc.hInstance = hInst;
    oc.lpszClassName = OVERLAY_CLASS;
    oc.hCursor = LoadCursor(nullptr, IDC_SIZEALL);
    RegisterClassExW(&oc);

    if (RegisterHotKey(g_hiddenWnd, HOTKEY_ID, g_hotkey.modifiers, g_hotkey.vk))
        WriteLog(L"Main hotkey registered: " + HotkeyToString(g_hotkey));
    else WriteLog(L"FAILED to register main hotkey");

    if (g_muteSettings.enabled) {
        if (RegisterHotKey(g_hiddenWnd, MUTE_HOTKEY_ID, g_muteSettings.modifiers, g_muteSettings.vk))
            WriteLog(L"Mute hotkey registered");
        else WriteLog(L"FAILED to register mute hotkey");
    }

    if (g_overlaySettings.enabled) {
        if (RegisterHotKey(g_hiddenWnd, OVERLAY_HOTKEY_ID,
                           g_overlaySettings.hotkeyModifiers,
                           g_overlaySettings.hotkeyVk)) {
            WriteLog(L"Overlay hotkey registered: " + OverlayHotkeyToString(g_overlaySettings));
        } else {
            WriteLog(L"FAILED to register overlay hotkey");
        }
    }

    g_controllerThreadStop = 0;
    g_controllerThread = CreateThread(nullptr, 0, ControllerThreadProc, nullptr, 0, nullptr);
    if (g_controllerThread) WriteLog(L"Controller thread started");
    else WriteLog(L"FAILED to start controller thread");

    AddTrayIcon(g_hiddenWnd);

    g_muteEvent = CreateEventW(nullptr, FALSE, FALSE, MUTE_REQUEST_EVENT_NAME);
    if (!g_muteEvent) g_muteEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    MSG msg;
    bool running = true;
    while (running) {
        DWORD waitResult = MsgWaitForMultipleObjects(1, &g_muteEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (waitResult == WAIT_OBJECT_0) {
            ProcessMuteRequest();
        } else if (waitResult == WAIT_OBJECT_0 + 1) {
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        } else if (waitResult == WAIT_FAILED) {
            while (GetMessageW(&msg, nullptr, 0, 0)) {
                if (msg.message == WM_QUIT) { running = false; break; }
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            break;
        }
    }

    if (g_controllerThread) {
        InterlockedExchange(&g_controllerThreadStop, 1);
        WaitForSingleObject(g_controllerThread, 1000);
        CloseHandle(g_controllerThread);
        g_controllerThread = nullptr;
    }

    if (g_muteEvent) CloseHandle(g_muteEvent);
    GdiplusShutdown(g_gdiplusToken);
    CloseHandle(hSingleInstance);
    if (comInitialized) CoUninitialize();
    return 0;
}