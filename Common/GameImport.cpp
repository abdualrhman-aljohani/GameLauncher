// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// GameImport.cpp — اكتشاف الألعاب المثبّتة (قراءة ملفات المتاجر و registry فقط)
#include "GameImport.h"
#include "GameImportParse.h"
#include "MiniJson.h"
#include "Utf8Stream.h"
#include <windows.h>
#include <shlobj.h>
#include <sstream>
#include <algorithm>

namespace gimport {

static bool DirExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}
static bool FileExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}
static std::wstring Slashes(std::wstring p) {
    std::replace(p.begin(), p.end(), L'/', L'\\');
    return p;
}
static std::wstring ReadFileText(const std::wstring& path) {
    Utf8In f(path.c_str());
    if (!f.is_open()) return L"";
    std::wstringstream ss;
    ss << f.rdbuf();
    return ss.str();
}
static std::wstring ReadRegString(HKEY root, const std::wstring& sub, const wchar_t* name, REGSAM view) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(root, sub.c_str(), 0, KEY_READ | view, &h) != ERROR_SUCCESS) return L"";
    wchar_t buf[2048];
    DWORD sz = sizeof(buf) - sizeof(wchar_t), type = 0;
    const LONG r = RegQueryValueExW(h, name, nullptr, &type, (LPBYTE)buf, &sz);
    RegCloseKey(h);
    if (r != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return L"";
    buf[sz / sizeof(wchar_t)] = L'\0';
    return buf;
}
static std::vector<std::wstring> ListFiles(const std::wstring& dir, const wchar_t* pattern) {
    std::vector<std::wstring> out;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\" + pattern).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) out.push_back(dir + L"\\" + fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return out;
}

// ---------------------------------------------------------------- Steam
static void FindSteam(std::vector<InstalledGame>& out) {
    std::wstring steam = ReadRegString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", 0);
    if (steam.empty())
        steam = ReadRegString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Valve\\Steam", L"InstallPath", KEY_WOW64_32KEY);
    if (steam.empty()) return;
    steam = TrimTrailingSlashes(Slashes(steam));

    std::vector<std::wstring> libs;
    libs.push_back(steam);
    const std::wstring vdf = ReadFileText(steam + L"\\steamapps\\libraryfolders.vdf");
    for (auto& p : ParseSteamLibraryPaths(vdf)) {
        const std::wstring lib = TrimTrailingSlashes(Slashes(p));
        bool dup = false;
        for (auto& e : libs) if (LowerW(e) == LowerW(lib)) { dup = true; break; }
        if (!dup) libs.push_back(lib);
    }

    for (auto& lib : libs) {
        const std::wstring apps = lib + L"\\steamapps";
        for (auto& file : ListFiles(apps, L"appmanifest_*.acf")) {
            SteamManifest m;
            if (!ParseSteamManifest(ReadFileText(file), m)) continue;
            if (IsSteamNonGame(m)) continue;
            if ((m.stateFlags & 4) == 0) continue;            // 4 = مثبّتة بالكامل
            const std::wstring dir = apps + L"\\common\\" + m.installDir;
            if (!DirExists(dir)) continue;
            InstalledGame g;
            g.source = L"Steam";
            g.name = m.name;
            g.launchPath = L"steam://rungameid/" + m.appId;
            g.installDir = dir;
            out.push_back(g);
        }
    }
}

// ---------------------------------------------------------------- Epic Games
static void FindEpic(std::vector<InstalledGame>& out) {
    // ProgramData (CSIDL بدل GUID فلا نحتاج ربط uuid.lib)
    wchar_t pdBuf[MAX_PATH] = L"";
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, SHGFP_TYPE_CURRENT, pdBuf))) return;
    const std::wstring programData = pdBuf;
    if (programData.empty()) return;

    const std::wstring dir = programData + L"\\Epic\\EpicGamesLauncher\\Data\\Manifests";
    for (auto& file : ListFiles(dir, L"*.item")) {
        const std::wstring text = ReadFileText(file);
        if (text.empty()) continue;
        const std::wstring name = JsonGetString(text, L"DisplayName");
        const std::wstring install = TrimTrailingSlashes(Slashes(JsonGetString(text, L"InstallLocation")));
        const std::wstring exeRel = Slashes(JsonGetString(text, L"LaunchExecutable"));
        if (name.empty() || install.empty() || exeRel.empty()) continue;     // DLC/أدوات بلا ملف تشغيل
        if (JsonGetBool(text, L"bIsIncompleteInstall", false)) continue;
        const auto cats = JsonGetStringArray(text, L"AppCategories");
        if (!cats.empty() && std::find(cats.begin(), cats.end(), L"games") == cats.end()) continue;
        if (!DirExists(install)) continue;

        const std::wstring exeFull = install + L"\\" + exeRel;
        const std::wstring appName = JsonGetString(text, L"AppName");
        const std::wstring ns = JsonGetString(text, L"CatalogNamespace");
        const std::wstring itemId = JsonGetString(text, L"CatalogItemId");

        InstalledGame g;
        g.source = L"Epic";
        g.name = name;
        g.launchPath = (ns.empty() || itemId.empty() || appName.empty())
            ? exeFull : MakeEpicLaunchUri(ns, itemId, appName);
        g.installDir = install;
        g.iconExe = FileExists(exeFull) ? exeFull : L"";
        out.push_back(g);
    }
}

// ---------------------------------------------------------------- GOG Galaxy
static void FindGog(std::vector<InstalledGame>& out) {
    const std::wstring base = L"SOFTWARE\\GOG.com\\Games";
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, base.c_str(), 0,
                      KEY_READ | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_32KEY, &h) != ERROR_SUCCESS) return;
    for (DWORD i = 0; ; i++) {
        wchar_t sub[256];
        DWORD len = 256;
        if (RegEnumKeyExW(h, i, sub, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        const std::wstring key = base + L"\\" + sub;
        const std::wstring name = ReadRegString(HKEY_LOCAL_MACHINE, key, L"gameName", KEY_WOW64_32KEY);
        const std::wstring exe = Slashes(ReadRegString(HKEY_LOCAL_MACHINE, key, L"exe", KEY_WOW64_32KEY));
        const std::wstring path = Slashes(ReadRegString(HKEY_LOCAL_MACHINE, key, L"path", KEY_WOW64_32KEY));
        if (name.empty() || exe.empty() || !FileExists(exe)) continue;
        InstalledGame g;
        g.source = L"GOG";
        g.name = name;
        g.launchPath = exe;
        g.installDir = TrimTrailingSlashes(path);
        g.iconExe = exe;
        out.push_back(g);
    }
    RegCloseKey(h);
}

// ---------------------------------------------------------------- Ubisoft Connect
static void FindUbisoft(std::vector<InstalledGame>& out) {
    const std::wstring base = L"SOFTWARE\\Ubisoft\\Launcher\\Installs";
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, base.c_str(), 0,
                      KEY_READ | KEY_ENUMERATE_SUB_KEYS | KEY_WOW64_32KEY, &h) != ERROR_SUCCESS) return;
    for (DWORD i = 0; ; i++) {
        wchar_t sub[256];
        DWORD len = 256;
        if (RegEnumKeyExW(h, i, sub, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        const std::wstring id = sub;
        if (!AllDigits(id)) continue;
        const std::wstring dir = TrimTrailingSlashes(Slashes(
            ReadRegString(HKEY_LOCAL_MACHINE, base + L"\\" + id, L"InstallDir", KEY_WOW64_32KEY)));
        if (dir.empty() || !DirExists(dir)) continue;
        InstalledGame g;
        g.source = L"Ubisoft";
        g.name = LastPathComponent(dir);         // الـ registry لا يحفظ اسم اللعبة: نأخذه من اسم المجلد
        g.launchPath = L"uplay://launch/" + id + L"/0";
        g.installDir = dir;
        out.push_back(g);
    }
    RegCloseKey(h);
}

void FindInstalledGames(std::vector<InstalledGame>& out) {
    out.clear();
    // فشل أي متجر (ملف تالف مثلاً) لا يوقف البقية
    try { FindSteam(out); }   catch (...) {}
    try { FindEpic(out); }    catch (...) {}
    try { FindGog(out); }     catch (...) {}
    try { FindUbisoft(out); } catch (...) {}

    // إزالة التكرار (بنفس رابط التشغيل) ثم ترتيب: المتجر ثم الاسم
    std::vector<InstalledGame> unique;
    for (auto& g : out) {
        bool dup = false;
        for (auto& u : unique) if (LowerW(u.launchPath) == LowerW(g.launchPath)) { dup = true; break; }
        if (!dup) unique.push_back(g);
    }
    std::sort(unique.begin(), unique.end(), [](const InstalledGame& a, const InstalledGame& b) {
        if (a.source != b.source) return a.source < b.source;
        return LowerW(a.name) < LowerW(b.name);
    });
    out.swap(unique);
}

} // namespace gimport
