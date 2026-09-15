// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
#define NOMINMAX

// Panel.cpp
#include "../Common/Config.h"
#include "../Common/Resource.h"
#include "../Common/MuteAudio.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <oleidl.h>
#include <wrl.h>
#include <WebView2.h>
#include <gdiplus.h>
#include <objidl.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <ctime>
#include <algorithm>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Microsoft::WRL;
using namespace Gdiplus;

static const wchar_t* const PANEL_SINGLE_INSTANCE_MUTEX = L"GameLauncherPanel_SingleInstance_Mutex_v1";
static const wchar_t* CLASS_NAME = L"GameLauncherPanelWnd";
static HANDLE g_singleInstanceMutex = nullptr;

static Lang g_lang = Lang::AR;
static std::vector<GameEntry> g_games;
static HotkeySettings g_hotkey;
static HWND g_hwnd = nullptr;
static int g_engineStatusTimerId = 1;
static const UINT_PTR PUSH_STATE_TIMER_ID = 2;

static bool g_lastEngineStatus = false;
static bool g_firstStatusCheck = true;
static bool g_wasMinimized = false;

static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2> g_webview;

static std::map<std::wstring, std::wstring> g_iconUriCache;
static std::map<std::wstring, std::wstring> g_radialBgUriCache;
static std::wstring g_iconCacheKey;

static std::set<std::wstring> g_runningGamesCache;
static std::set<std::wstring> g_muteStateCache;
static std::wstring g_runningGamesCacheStamp;
static std::wstring g_muteStateCacheStamp;

// ----------------------------- استخراج HTML المُدمج -----------------------------
static bool ExtractEmbeddedHtml(std::wstring& outPath) {
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PANEL_HTML), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(nullptr, hRes);
    const char* data = (const char*)LockResource(hData);
    if (!data || size == 0) return false;

    outPath = GetAppDataDir() + L"\\panel_ui.html";
    std::ofstream f(outPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(data, (std::streamsize)size);
    f.close();
    return true;
}

static void SignalMuteRequestToLauncher() {
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, MUTE_REQUEST_EVENT_NAME);
    if (hEvent) { SetEvent(hEvent); CloseHandle(hEvent); }
}

// ----------------------------- أدوات -----------------------------
static std::wstring MakeGameNameFromPath(const std::wstring& path) {
    if (IsUwpPath(path)) return DefaultUwpName(GetUwpAumid(path));
    if (IsUriLike(path)) {
        size_t q = path.find(L'?');
        std::wstring base = (q == std::wstring::npos) ? path : path.substr(0, q);
        size_t slash = base.find_last_of(L"/");
        if (slash != std::wstring::npos && slash + 1 < base.size())
            return base.substr(slash + 1);
        return path;
    }
    size_t s = path.find_last_of(L"\\/");
    size_t d = path.find_last_of(L'.');
    std::wstring base = (s == std::wstring::npos) ? path : path.substr(s + 1);
    if (d != std::wstring::npos && d > s) {
        size_t cut = d - (s == std::wstring::npos ? 0 : s + 1);
        base = base.substr(0, cut);
    }
    return base;
}

static bool BrowseForExe(std::wstring& outPath, const wchar_t* title) {
    wchar_t buf[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = L"Programs, shortcuts, links (*.exe;*.lnk;*.url)\0*.exe;*.lnk;*.url\0All files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = title;
    if (GetOpenFileNameW(&ofn)) { outPath = buf; return true; }
    return false;
}

static bool BrowseForImage(std::wstring& outPath, const wchar_t* title) {
    IFileOpenDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr) || !pfd) return false;
    COMDLG_FILTERSPEC filters[] = {
        { L"Images", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif" },
        { L"All files", L"*.*" }
    };
    pfd->SetFileTypes(2, filters);
    pfd->SetTitle(title);
    bool ok = false;
    if (SUCCEEDED(pfd->Show(g_hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(pfd->GetResult(&item)) && item) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                outPath = path; CoTaskMemFree(path); ok = true;
            }
            item->Release();
        }
    }
    pfd->Release();
    return ok;
}

// ✅ لون مخصص عبر ChooseColor
static bool BrowseForCustomColor(COLORREF initial, COLORREF& outColor) {
    CHOOSECOLORW cc = { sizeof(cc) };
    static COLORREF customColors[16] = {};
    cc.hwndOwner      = g_hwnd;
    cc.rgbResult      = initial;
    cc.lpCustColors   = customColors;
    cc.Flags          = CC_FULLOPEN | CC_RGBINIT | CC_ANYCOLOR;
    if (ChooseColorW(&cc)) {
        outColor = cc.rgbResult;
        return true;
    }
    return false;
}

static bool BrowseForJsonSave(std::wstring& outPath, const wchar_t* title,
                              const wchar_t* defaultFileName) {
    wchar_t buf[MAX_PATH] = L"";
    if (defaultFileName) wcsncpy_s(buf, defaultFileName, _TRUNCATE);
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = L"JSON backup (*.json)\0*.json\0All files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = title;
    ofn.lpstrDefExt = L"json";
    if (GetSaveFileNameW(&ofn)) { outPath = buf; return true; }
    return false;
}

static bool BrowseForJsonOpen(std::wstring& outPath, const wchar_t* title) {
    wchar_t buf[MAX_PATH] = L"";
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = L"JSON backup (*.json)\0*.json\0All files\0*.*\0";
    ofn.lpstrFile   = buf;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = title;
    if (GetOpenFileNameW(&ofn)) { outPath = buf; return true; }
    return false;
}

static HICON ExtractIconFromShellItem(const std::wstring& path, int sizePx) {
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
    ii.fIcon = TRUE;
    ii.hbmColor = hbmp;
    ii.hbmMask = CreateBitmap(sizePx, sizePx, 1, 1, nullptr);
    HICON hicon = CreateIconIndirect(&ii);
    DeleteObject(hbmp);
    if (ii.hbmMask) DeleteObject(ii.hbmMask);
    return hicon;
}

static const int ICON_EXTRACT_SIZE = 64;
static HICON ExtractIconForEntry(const std::wstring& exePath, const std::wstring& iconPath, int iconIndex) {
    if (IsUwpPath(exePath)) return ExtractHighQualityIcon(exePath, 0, ICON_EXTRACT_SIZE);
    if (IsUriLike(exePath)) {
        if (!iconPath.empty()) return ExtractHighQualityIcon(iconPath, iconIndex, ICON_EXTRACT_SIZE);
        return nullptr;
    }
    std::wstring src = !iconPath.empty() ? iconPath : exePath;
    std::wstring lower = src;
    for (auto& c : lower) c = (wchar_t)towlower(c);
    if (lower.size() >= 4 && lower.substr(lower.size() - 4) == L".lnk") {
        HICON shellIcon = ExtractIconFromShellItem(src, ICON_EXTRACT_SIZE);
        if (shellIcon) return shellIcon;
    }
    return ExtractHighQualityIcon(src, iconIndex, ICON_EXTRACT_SIZE);
}

// ----------------------------- Base64 + PNG -----------------------------
static std::wstring Base64Encode(const std::vector<BYTE>& data) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::wstring out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < data.size()) {
        unsigned v = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += (wchar_t)tbl[(v >> 18) & 0x3F];
        out += (wchar_t)tbl[(v >> 12) & 0x3F];
        out += (wchar_t)tbl[(v >> 6) & 0x3F];
        out += (wchar_t)tbl[v & 0x3F];
        i += 3;
    }
    size_t rem = data.size() - i;
    if (rem == 1) {
        unsigned v = data[i] << 16;
        out += (wchar_t)tbl[(v >> 18) & 0x3F];
        out += (wchar_t)tbl[(v >> 12) & 0x3F];
        out += L"==";
    } else if (rem == 2) {
        unsigned v = (data[i] << 16) | (data[i + 1] << 8);
        out += (wchar_t)tbl[(v >> 18) & 0x3F];
        out += (wchar_t)tbl[(v >> 12) & 0x3F];
        out += (wchar_t)tbl[(v >> 6) & 0x3F];
        out += L"=";
    }
    return out;
}

static int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    Gdiplus::ImageCodecInfo* pInfo = (Gdiplus::ImageCodecInfo*)malloc(size);
    if (!pInfo) return -1;
    Gdiplus::GetImageEncoders(num, size, pInfo);
    int found = -1;
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pInfo[j].MimeType, format) == 0) {
            *pClsid = pInfo[j].Clsid;
            found = (int)j;
            break;
        }
    }
    free(pInfo);
    return found;
}

static std::wstring IconToDataUri(HICON hicon, int size) {
    if (!hicon) return L"";
    Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromHICON(hicon);
    if (!src || src->GetLastStatus() != Gdiplus::Ok) {
        if (src) delete src;
        return L"";
    }
    Gdiplus::Bitmap bmp(size, size, PixelFormat32bppARGB);
    if (bmp.GetLastStatus() != Gdiplus::Ok) { delete src; return L""; }
    {
        Gdiplus::Graphics g(&bmp);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        g.DrawImage(src, 0, 0, size, size);
    }
    delete src;

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) return L"";
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) return L"";
    if (bmp.Save(stream, &pngClsid, nullptr) != Gdiplus::Ok) {
        stream->Release();
        return L"";
    }
    STATSTG stat = {};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) { stream->Release(); return L""; }
    ULONGLONG total = stat.cbSize.QuadPart;
    if (total == 0 || total > 10ULL * 1024 * 1024) { stream->Release(); return L""; }
    std::vector<BYTE> data((size_t)total);
    LARGE_INTEGER zero = {};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG read = 0;
    stream->Read(data.data(), (ULONG)total, &read);
    stream->Release();
    if (read != (ULONG)total) return L"";
    return L"data:image/png;base64," + Base64Encode(data);
}

static std::wstring ImageFileToDataUri(const std::wstring& path) {
    if (path.empty()) return L"";
    Gdiplus::Bitmap* src = Gdiplus::Bitmap::FromFile(path.c_str());
    if (!src || src->GetLastStatus() != Gdiplus::Ok) {
        if (src) delete src;
        return L"";
    }
    const int preview = 64;
    Gdiplus::Bitmap bmp(preview, preview, PixelFormat32bppARGB);
    if (bmp.GetLastStatus() != Gdiplus::Ok) { delete src; return L""; }
    {
        Gdiplus::Graphics g(&bmp);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        REAL imgW = (REAL)src->GetWidth();
        REAL imgH = (REAL)src->GetHeight();
        if (imgW <= 0 || imgH <= 0) { delete src; return L""; }
        REAL scale = (std::max)((REAL)preview / imgW, (REAL)preview / imgH);
        REAL dw = imgW * scale;
        REAL dh = imgH * scale;
        REAL dx = (preview - dw) / 2.0f;
        REAL dy = (preview - dh) / 2.0f;
        g.DrawImage(src, RectF(dx, dy, dw, dh), 0.0f, 0.0f, imgW, imgH, UnitPixel);
    }
    delete src;

    CLSID pngClsid;
    if (GetEncoderClsid(L"image/png", &pngClsid) < 0) return L"";
    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) return L"";
    if (bmp.Save(stream, &pngClsid, nullptr) != Gdiplus::Ok) {
        stream->Release();
        return L"";
    }
    STATSTG stat = {};
    if (FAILED(stream->Stat(&stat, STATFLAG_NONAME))) { stream->Release(); return L""; }
    ULONGLONG total = stat.cbSize.QuadPart;
    if (total == 0 || total > 10ULL * 1024 * 1024) { stream->Release(); return L""; }
    std::vector<BYTE> data((size_t)total);
    LARGE_INTEGER zero = {};
    stream->Seek(zero, STREAM_SEEK_SET, nullptr);
    ULONG read = 0;
    stream->Read(data.data(), (ULONG)total, &read);
    stream->Release();
    if (read != (ULONG)total) return L"";
    return L"data:image/png;base64," + Base64Encode(data);
}

// ----------------------------- JSON helpers -----------------------------
static std::wstring JsonEscape(const std::wstring& in) {
    std::wstring out;
    out.reserve(in.size() + 8);
    for (wchar_t c : in) {
        switch (c) {
        case L'"':  out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default:
            if (c < 0x20) { wchar_t buf[8]; wsprintfW(buf, L"\\u%04x", c); out += buf; }
            else out += c;
        }
    }
    return out;
}

static std::wstring JsonGetString(const std::wstring& json, const std::wstring& key) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = json.find(pat);
    if (p == std::wstring::npos) return L"";
    p = json.find(L':', p + pat.size());
    if (p == std::wstring::npos) return L"";
    p++;
    while (p < json.size() && (json[p] == L' ' || json[p] == L'\t')) p++;
    if (p >= json.size() || json[p] != L'"') return L"";
    p++;
    std::wstring out;
    while (p < json.size() && json[p] != L'"') {
        if (json[p] == L'\\' && p + 1 < json.size()) {
            p++;
            wchar_t c = json[p];
            switch (c) {
            case L'n': out += L'\n'; break;
            case L't': out += L'\t'; break;
            case L'r': out += L'\r'; break;
            case L'"': out += L'"'; break;
            case L'\\': out += L'\\'; break;
            case L'/': out += L'/'; break;
            case L'u':
                if (p + 4 < json.size()) { out += (wchar_t)wcstol(json.substr(p + 1, 4).c_str(), nullptr, 16); p += 4; }
                break;
            default: out += c;
            }
        } else out += json[p];
        p++;
    }
    return out;
}

static long long JsonGetLongLong(const std::wstring& json, const std::wstring& key, long long def = 0) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = json.find(pat);
    if (p == std::wstring::npos) return def;
    p = json.find(L':', p + pat.size());
    if (p == std::wstring::npos) return def;
    p++;
    while (p < json.size() && json[p] == L' ') p++;
    size_t start = p;
    if (p < json.size() && json[p] == L'-') p++;
    while (p < json.size() && iswdigit(json[p])) p++;
    if (p == start || (p == start + 1 && json[start] == L'-')) return def;
    return _wcstoi64(json.substr(start, p - start).c_str(), nullptr, 10);
}

static long JsonGetNumber(const std::wstring& json, const std::wstring& key, long def = -1) {
    return (long)JsonGetLongLong(json, key, def);
}

static bool JsonGetBool(const std::wstring& json, const std::wstring& key, bool def) {
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = json.find(pat);
    if (p == std::wstring::npos) return def;
    p = json.find(L':', p + pat.size());
    if (p == std::wstring::npos) return def;
    p++;
    while (p < json.size() && json[p] == L' ') p++;
    if (p + 4 <= json.size() && json.compare(p, 4, L"true") == 0) return true;
    if (p + 5 <= json.size() && json.compare(p, 5, L"false") == 0) return false;
    long n = JsonGetNumber(json, key, def ? 1 : 0);
    return n != 0;
}

static std::vector<std::wstring> JsonGetStringArray(const std::wstring& json, const std::wstring& key) {
    std::vector<std::wstring> out;
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = json.find(pat);
    if (p == std::wstring::npos) return out;
    p = json.find(L'[', p + pat.size());
    if (p == std::wstring::npos) return out;
    size_t end = json.find(L']', p);
    if (end == std::wstring::npos) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (json[i] == L' ' || json[i] == L',' || json[i] == L'\t' ||
                            json[i] == L'\n' || json[i] == L'\r')) i++;
        if (i >= end || json[i] != L'"') break;
        i++;
        std::wstring s;
        while (i < end && json[i] != L'"') {
            if (json[i] == L'\\' && i + 1 < end) {
                i++;
                wchar_t c = json[i];
                switch (c) {
                case L'n': s += L'\n'; break;
                case L't': s += L'\t'; break;
                case L'r': s += L'\r'; break;
                case L'"': s += L'"'; break;
                case L'\\': s += L'\\'; break;
                case L'/': s += L'/'; break;
                case L'u':
                    if (i + 4 < end) { s += (wchar_t)wcstol(json.substr(i + 1, 4).c_str(), nullptr, 16); i += 4; }
                    break;
                default: s += c;
                }
            } else s += json[i];
            i++;
        }
        out.push_back(s);
        i++;
    }
    return out;
}

static std::vector<long> JsonGetNumberArray(const std::wstring& json, const std::wstring& key) {
    std::vector<long> out;
    std::wstring pat = L"\"" + key + L"\"";
    size_t p = json.find(pat);
    if (p == std::wstring::npos) return out;
    p = json.find(L'[', p + pat.size());
    if (p == std::wstring::npos) return out;
    size_t end = json.find(L']', p);
    if (end == std::wstring::npos) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (json[i] == L' ' || json[i] == L',' || json[i] == L'\t' ||
                            json[i] == L'\n' || json[i] == L'\r')) i++;
        if (i >= end) break;
        size_t start = i;
        if (json[i] == L'-') i++;
        while (i < end && iswdigit(json[i])) i++;
        if (i == start) break;
        out.push_back(wcstol(json.substr(start, i - start).c_str(), nullptr, 10));
    }
    return out;
}

// ----------------------------- كاشات الملفات -----------------------------
static std::wstring RunningGamesPath() { return GetAppDataDir() + L"\\running_games.txt"; }
static std::wstring MuteStatePath()    { return GetAppDataDir() + L"\\mute_state.txt"; }

static std::wstring FileStamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) return L"";
    wchar_t buf[64];
    wsprintfW(buf, L"%08X%08X-%08X%08X",
              fad.ftLastWriteTime.dwHighDateTime, fad.ftLastWriteTime.dwLowDateTime,
              fad.nFileSizeHigh, fad.nFileSizeLow);
    return buf;
}

static void RefreshSharedCaches() {
    std::wstring rgPath = RunningGamesPath();
    std::wstring rgStamp = FileStamp(rgPath);
    if (rgStamp != g_runningGamesCacheStamp) {
        g_runningGamesCacheStamp = rgStamp;
        g_runningGamesCache.clear();
        std::wifstream f(rgPath.c_str());
        if (f.is_open()) {
            std::wstring line;
            while (std::getline(f, line)) {
                line = TrimString(line);
                if (!line.empty()) g_runningGamesCache.insert(line);
            }
        }
    }
    std::wstring msPath = MuteStatePath();
    std::wstring msStamp = FileStamp(msPath);
    if (msStamp != g_muteStateCacheStamp) {
        g_muteStateCacheStamp = msStamp;
        g_muteStateCache.clear();
        std::wifstream f(msPath.c_str());
        if (f.is_open()) {
            std::wstring line;
            while (std::getline(f, line)) {
                line = TrimString(line);
                size_t p = line.find(L"|1");
                if (p != std::wstring::npos) g_muteStateCache.insert(line.substr(0, p));
            }
        }
    }
}

static bool IsGameRunningCached(const std::wstring& gamePath) {
    return g_runningGamesCache.count(gamePath) > 0;
}
static bool IsGameMutedCached(const std::wstring& gamePath) {
    return g_muteStateCache.count(gamePath) > 0;
}

// ----------------------------- كاش الأيقونات -----------------------------
static void EnsureIconCache() {
    std::wstringstream key;
    for (auto& g : g_games) {
        key << g.exePath << L'|' << g.iconPath << L'|' << g.iconIndex
            << L'|' << g.radialBgPath;
        // ✅ نضيف المرافقين للمفتاح — يضمن إعادة البناء عند أي تغيير
        for (auto& c : g.companions) {
            key << L'#' << c.path;
        }
        key << L'\n';
    }
    std::wstring newKey = key.str();
    if (newKey == g_iconCacheKey && !g_iconUriCache.empty()) return;

    g_iconCacheKey = newKey;
    g_iconUriCache.clear();
    g_radialBgUriCache.clear();

    for (auto& g : g_games) {
        HICON ic = ExtractIconForEntry(g.exePath, g.iconPath, g.iconIndex);
        std::wstring uri = IconToDataUri(ic, ICON_EXTRACT_SIZE);
        if (ic) DestroyIcon(ic);
        g_iconUriCache[g.exePath] = uri;

        for (size_t k = 0; k < g.companions.size(); k++) {
            auto& c = g.companions[k];
            std::wstring ckey = c.path + L"#" + std::to_wstring(k);
            HICON cicon = ExtractIconForEntry(c.path, L"", 0);
            std::wstring curi = IconToDataUri(cicon, ICON_EXTRACT_SIZE);
            if (cicon) DestroyIcon(cicon);
            g_iconUriCache[ckey] = curi;
        }

        if (!g.radialBgPath.empty()) {
            g_radialBgUriCache[g.radialBgPath] = ImageFileToDataUri(g.radialBgPath);
        }
    }
}

static std::wstring GetIconUri(const std::wstring& exePath) {
    auto it = g_iconUriCache.find(exePath);
    return (it == g_iconUriCache.end()) ? L"" : it->second;
}
static std::wstring GetCompanionIconUri(const std::wstring& path, int idx) {
    std::wstring ckey = path + L"#" + std::to_wstring(idx);
    auto it = g_iconUriCache.find(ckey);
    return (it == g_iconUriCache.end()) ? L"" : it->second;
}
static std::wstring GetRadialBgUri(const std::wstring& bgPath) {
    if (bgPath.empty()) return L"";
    auto it = g_radialBgUriCache.find(bgPath);
    return (it == g_radialBgUriCache.end()) ? L"" : it->second;
}

// ----------------------------- State JSON -----------------------------
static std::wstring BuildStateJson() {
    RefreshSharedCaches();
    EnsureIconCache();

    ControllerSettings cs = LoadControllerSettings();
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);

    std::wstringstream j;
    j << L"{\"type\":\"state\",";
    j << L"\"lang\":\"" << (g_lang == Lang::EN ? L"en" : L"ar") << L"\",";
    j << L"\"hotkey\":\"" << JsonEscape(HotkeyToString(g_hotkey)) << L"\",";
    j << L"\"engineRunning\":" << (IsEngineRunning() ? L"true" : L"false") << L",";
    j << L"\"autoStart\":" << (IsAutoStartEnabled() ? L"true" : L"false") << L",";
    j << L"\"autoLayoutSwitch\":" << (LoadAutoLayoutSwitch() ? L"true" : L"false") << L",";
    j << L"\"radialStyle\":" << (int)LoadRadialStyle() << L",";
    j << L"\"radialTransparency\":" << LoadRadialTransparency() << L",";  // ✅ جديد
    j << L"\"panelBackground\":\"" << JsonEscape(LoadPanelBackground()) << L"\",";
    j << L"\"radialBackground\":\"" << JsonEscape(LoadRadialBackground()) << L"\",";
    j << L"\"panelPreset\":\"" << JsonEscape(LoadPanelPreset()) << L"\",";
    j << L"\"muteEnabled\":" << (LoadMuteSettings().enabled ? L"true" : L"false") << L",";
    j << L"\"muteHotkey\":\"" << JsonEscape(MuteHotkeyToString(LoadMuteSettings())) << L"\",";
    j << L"\"hubAlwaysVisible\":" << (LoadHubAlwaysVisible() ? L"true" : L"false") << L",";
    j << L"\"controllerEnabled\":" << (cs.enabled ? L"true" : L"false") << L",";
    j << L"\"controllerButton\":" << (unsigned long)cs.openRadialButton << L",";
    j << L"\"controllerButtonName\":\"" << JsonEscape(ControllerButtonName(cs.openRadialButton, g_lang)) << L"\",";
    j << L"\"cpuCoreCount\":" << si.dwNumberOfProcessors << L",";
    j << L"\"processPriorityNames\":[";
    for (int i = 0; i < ProcessPriorityCount(); i++) {
        if (i) j << L",";
        j << L"\"" << JsonEscape(ProcessPriorityName((ProcessPriority)i, g_lang)) << L"\"";
    }
    j << L"],";
    j << L"\"radialStyleNames\":[";
    int count = RadialStyleCount();
    for (int i = 0; i < count; i++) {
        if (i) j << L",";
        j << L"\"" << JsonEscape(RadialStyleName((RadialStyle)i, g_lang)) << L"\"";
    }
    j << L"],\"games\":[";
    for (size_t i = 0; i < g_games.size(); i++) {
        if (i) j << L",";
        auto& g = g_games[i];
        std::wstring iconUri = GetIconUri(g.exePath);
        std::wstring radialBgUri = GetRadialBgUri(g.radialBgPath);

        j << L"{\"index\":" << i
          << L",\"name\":\"" << JsonEscape(g.name)
          << L"\",\"path\":\"" << JsonEscape(g.exePath)
          << L"\",\"icon\":\"" << iconUri
          << L"\",\"missing\":" << (PathExistsOnDisk(g.exePath) ? L"false" : L"true")
          << L",\"autoLangSwitch\":" << (g.autoLangSwitch ? L"true" : L"false")
          << L",\"runAsAdmin\":" << (g.runAsAdmin ? L"true" : L"false")
          << L",\"launchArgs\":\"" << JsonEscape(g.launchArgs) << L"\""
          << L",\"showInRadial\":" << (g.showInRadial ? L"true" : L"false")
          << L",\"isUwp\":" << (IsUwpPath(g.exePath) ? L"true" : L"false")
          << L",\"isSteam\":" << (IsUriLike(g.exePath) ? L"true" : L"false")
          << L",\"color\":" << (unsigned long)g.color
          << L",\"favorite\":" << (g.favorite ? L"true" : L"false")
          << L",\"totalPlaySeconds\":" << g.totalPlaySeconds
          << L",\"lastPlayedUnix\":" << (unsigned long)g.lastPlayedUnix
          << L",\"playCount\":" << g.playCount
          << L",\"isRunning\":" << (IsGameRunningCached(g.exePath) ? L"true" : L"false")
          << L",\"muted\":" << (IsGameMutedCached(g.exePath) ? L"true" : L"false")
          << L",\"radialBgPath\":\"" << JsonEscape(g.radialBgPath) << L"\""
          << L",\"radialBgPreview\":\"" << radialBgUri << L"\""
          << L",\"hideOriginalIcon\":" << (g.hideOriginalIcon ? L"true" : L"false")
          << L",\"processPriority\":" << (int)g.processPriority
          << L",\"applyAffinity\":" << (g.applyAffinity ? L"true" : L"false")
          << L",\"affinityMask\":" << g.affinityMask
          << L",\"companions\":[";
        for (size_t k = 0; k < g.companions.size(); k++) {
            if (k) j << L",";
            auto& c = g.companions[k];
            std::wstring curi = GetCompanionIconUri(c.path, (int)k);
            j << L"{\"index\":" << k
              << L",\"name\":\"" << JsonEscape(GetFileNameFromPath(c.path))
              << L"\",\"path\":\"" << JsonEscape(c.path)
              << L"\",\"icon\":\"" << curi
              << L"\",\"runAsAdmin\":" << (c.runAsAdmin ? L"true" : L"false")
              << L",\"launchArgs\":\"" << JsonEscape(c.launchArgs) << L"\""
              << L",\"showInRadial\":" << (c.showInRadial ? L"true" : L"false")
              << L"}";
        }
        j << L"]}";
    }
    j << L"]}";
    return j.str();
}

static void PushStateToJs() {
    if (!g_webview) return;
    LoadGames(g_games);
    g_webview->PostWebMessageAsString(BuildStateJson().c_str());
}

static void PushStatus(const std::wstring& text, const wchar_t* kind = L"info") {
    if (!g_webview) return;
    std::wstring j = L"{\"type\":\"status\",\"text\":\"" + JsonEscape(text) +
        L"\",\"kind\":\"" + kind + L"\"}";
    g_webview->PostWebMessageAsString(j.c_str());
}

static void PushSuggestExe(const std::wstring& folderName, const std::wstring& exePath) {
    if (!g_webview) return;
    std::wstring j = L"{\"type\":\"suggestExe\",\"folder\":\"" + JsonEscape(folderName) +
        L"\",\"path\":\"" + JsonEscape(exePath) + L"\"}";
    g_webview->PostWebMessageAsString(j.c_str());
}

static void PushSuggestExeList(const std::wstring& folderName,
                                const std::vector<ExeScanResult>& items) {
    if (!g_webview) return;
    std::wstringstream j;
    j << L"{\"type\":\"suggestExeList\",\"folder\":\"" << JsonEscape(folderName) << L"\",\"items\":[";
    const int kMaxItems = 50;
    int n = (int)items.size(); if (n > kMaxItems) n = kMaxItems;
    for (int i = 0; i < n; i++) {
        if (i) j << L",";
        auto& it = items[i];
        HICON ic = ExtractHighQualityIcon(it.path, 0, 32);
        std::wstring uri = IconToDataUri(ic, 32);
        if (ic) DestroyIcon(ic);
        j << L"{\"name\":\"" << JsonEscape(GetFileNameFromPath(it.path))
          << L"\",\"path\":\"" << JsonEscape(it.path)
          << L"\",\"icon\":\"" << uri << L"\""
          << L",\"main\":" << (it.looksLikeMain ? L"true" : L"false")
          << L",\"size\":" << it.size << L"}";
    }
    j << L"]}";
    g_webview->PostWebMessageAsString(j.str().c_str());
}

// ----------------------------- Export / Import -----------------------------
static bool ExportBackupToFile(const std::wstring& path) {
    std::wstring tmp = path + L".tmp";
    std::wofstream f(tmp.c_str(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << L"{\n  \"version\": 3,\n  \"exportedAt\": " << (unsigned long)time(nullptr) << L",\n";
    f << L"  \"games\": [\n";
    for (size_t i = 0; i < g_games.size(); i++) {
        auto& g = g_games[i];
        f << L"    {\n";
        f << L"      \"name\": \"" << JsonEscape(g.name) << L"\",\n";
        f << L"      \"exePath\": \"" << JsonEscape(g.exePath) << L"\",\n";
        f << L"      \"color\": " << (unsigned long)g.color << L",\n";
        f << L"      \"iconPath\": \"" << JsonEscape(g.iconPath) << L"\",\n";
        f << L"      \"iconIndex\": " << g.iconIndex << L",\n";
        f << L"      \"autoLangSwitch\": " << (g.autoLangSwitch ? L"true" : L"false") << L",\n";
        f << L"      \"runAsAdmin\": " << (g.runAsAdmin ? L"true" : L"false") << L",\n";
        f << L"      \"launchArgs\": \"" << JsonEscape(g.launchArgs) << L"\",\n";
        f << L"      \"showInRadial\": " << (g.showInRadial ? L"true" : L"false") << L",\n";
        f << L"      \"favorite\": " << (g.favorite ? L"true" : L"false") << L",\n";
        f << L"      \"totalPlaySeconds\": " << g.totalPlaySeconds << L",\n";
        f << L"      \"lastPlayedUnix\": " << (unsigned long)g.lastPlayedUnix << L",\n";
        f << L"      \"playCount\": " << g.playCount << L",\n";
        f << L"      \"radialBgPath\": \"" << JsonEscape(g.radialBgPath) << L"\",\n";
        f << L"      \"hideOriginalIcon\": " << (g.hideOriginalIcon ? L"true" : L"false") << L",\n";
        f << L"      \"processPriority\": " << (int)g.processPriority << L",\n";
        f << L"      \"applyAffinity\": " << (g.applyAffinity ? L"true" : L"false") << L",\n";
        f << L"      \"affinityMask\": " << g.affinityMask << L",\n";
        f << L"      \"companions\": [\n";
        for (size_t k = 0; k < g.companions.size(); k++) {
            auto& c = g.companions[k];
            f << L"        { \"path\": \"" << JsonEscape(c.path) << L"\", ";
            f << L"\"runAsAdmin\": " << (c.runAsAdmin ? L"true" : L"false") << L", ";
            f << L"\"launchArgs\": \"" << JsonEscape(c.launchArgs) << L"\", ";
            f << L"\"showInRadial\": " << (c.showInRadial ? L"true" : L"false") << L" }";
            if (k + 1 < g.companions.size()) f << L",";
            f << L"\n";
        }
        f << L"      ]\n";
        f << L"    }";
        if (i + 1 < g_games.size()) f << L",";
        f << L"\n";
    }
    f << L"  ]\n}\n";
    f.close();
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

static size_t FindObjectEnd(const std::wstring& content, size_t start) {
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (size_t i = start; i < content.size(); i++) {
        wchar_t c = content[i];
        if (escape) { escape = false; continue; }
        if (c == L'\\') { escape = true; continue; }
        if (c == L'"') { inString = !inString; continue; }
        if (inString) continue;
        if (c == L'{') depth++;
        else if (c == L'}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::wstring::npos;
}

static bool ImportBackupFromFile(const std::wstring& path) {
    std::wifstream f(path.c_str());
    if (!f.is_open()) return false;
    std::wstringstream ss;
    ss << f.rdbuf();
    std::wstring content = ss.str();
    if (content.find(L"\"games\"") == std::wstring::npos) return false;

    std::vector<GameEntry> imported;
    size_t pos = 0;
    int guard = 0;
    while (guard++ < 10000) {
        size_t p = content.find(L"\"name\"", pos);
        if (p == std::wstring::npos) break;
        size_t objStart = content.rfind(L'{', p);
        if (objStart == std::wstring::npos) break;
        size_t objEnd = FindObjectEnd(content, objStart);
        if (objEnd == std::wstring::npos) break;
        std::wstring obj = content.substr(objStart, objEnd - objStart + 1);

        GameEntry g;
        g.name = JsonGetString(obj, L"name");
        g.exePath = JsonGetString(obj, L"exePath");
        g.color = (COLORREF)JsonGetNumber(obj, L"color", 0x2EA6DA);
        g.iconPath = JsonGetString(obj, L"iconPath");
        g.iconIndex = (int)JsonGetNumber(obj, L"iconIndex", 0);
        g.autoLangSwitch = JsonGetBool(obj, L"autoLangSwitch", true);
        g.runAsAdmin = JsonGetBool(obj, L"runAsAdmin", false);
        g.launchArgs = JsonGetString(obj, L"launchArgs");
        g.showInRadial = JsonGetBool(obj, L"showInRadial", true);
        g.favorite = JsonGetBool(obj, L"favorite", false);
        g.totalPlaySeconds = (unsigned long long)JsonGetLongLong(obj, L"totalPlaySeconds", 0);
        g.lastPlayedUnix = (DWORD)JsonGetLongLong(obj, L"lastPlayedUnix", 0);
        g.playCount = (unsigned int)JsonGetLongLong(obj, L"playCount", 0);
        g.radialBgPath = JsonGetString(obj, L"radialBgPath");
        g.hideOriginalIcon = JsonGetBool(obj, L"hideOriginalIcon", false);
        g.processPriority = ProcessPriorityFromIndex((int)JsonGetNumber(obj, L"processPriority", 0));
        g.applyAffinity = JsonGetBool(obj, L"applyAffinity", false);
        g.affinityMask = (unsigned long long)JsonGetLongLong(obj, L"affinityMask", 0);

        size_t cpos = obj.find(L"\"companions\"");
        if (cpos != std::wstring::npos) {
            size_t arrStart = obj.find(L'[', cpos);
            if (arrStart != std::wstring::npos) {
                int depth = 0;
                bool inStr = false, esc = false;
                size_t i = arrStart;
                for (; i < obj.size(); i++) {
                    wchar_t c = obj[i];
                    if (esc) { esc = false; continue; }
                    if (c == L'\\') { esc = true; continue; }
                    if (c == L'"') { inStr = !inStr; continue; }
                    if (inStr) continue;
                    if (c == L'[') depth++;
                    else if (c == L']') { depth--; if (depth == 0) { i++; break; } }
                }
                std::wstring arr = obj.substr(arrStart, i - arrStart);
                size_t cp = 0;
                int cguard = 0;
                while (cguard++ < 1000) {
                    size_t cObjStart = arr.find(L'{', cp);
                    if (cObjStart == std::wstring::npos) break;
                    size_t cObjEnd = FindObjectEnd(arr, cObjStart);
                    if (cObjEnd == std::wstring::npos) break;
                    std::wstring cStr = arr.substr(cObjStart, cObjEnd - cObjStart + 1);
                    CompanionEntry ce;
                    ce.path = JsonGetString(cStr, L"path");
                    ce.runAsAdmin = JsonGetBool(cStr, L"runAsAdmin", false);
                    ce.launchArgs = JsonGetString(cStr, L"launchArgs");
                    ce.showInRadial = JsonGetBool(cStr, L"showInRadial", true);
                    if (!ce.path.empty()) g.companions.push_back(ce);
                    cp = cObjEnd + 1;
                }
            }
        }

        if (!g.name.empty() && !g.exePath.empty()) imported.push_back(g);
        pos = objEnd + 1;
    }

    if (imported.empty()) return false;
    g_games = imported;
    SaveGames(g_games);
    return true;
}

// ----------------------------- إجراءات الألعاب -----------------------------
static bool GameExistsWithPath(const std::wstring& path) {
    for (auto& g : g_games)
        if (_wcsicmp(g.exePath.c_str(), path.c_str()) == 0) return true;
    return false;
}

static COLORREF PickColorForIndex(int index) {
    static const COLORREF PALETTE[] = {
        RGB(0x2E,0xA6,0xDA), RGB(0x6C,0xC0,0x4A), RGB(0xE0,0x5A,0x47),
        RGB(0xF2,0xA6,0x2E), RGB(0x9B,0x59,0xB6), RGB(0x1E,0xBF,0xA3),
    };
    return PALETTE[index % (sizeof(PALETTE) / sizeof(PALETTE[0]))];
}

static void RemoveGameAt(int idx) {
    if (idx < 0 || idx >= (int)g_games.size()) return;
    g_games.erase(g_games.begin() + idx);
    SaveGames(g_games); PushStateToJs();
}

static void ReorderGames(const std::vector<long>& order) {
    if (order.size() != g_games.size()) return;
    std::vector<bool> seen(g_games.size(), false);
    std::vector<GameEntry> newOrder; newOrder.reserve(g_games.size());
    for (long idx : order) {
        if (idx < 0 || idx >= (long)g_games.size() || seen[idx]) return;
        seen[idx] = true; newOrder.push_back(g_games[idx]);
    }
    g_games = std::move(newOrder);
    SaveGames(g_games); PushStateToJs();
}

static void RemoveCompanionAt(int gameIdx, int compIdx) {
    if (gameIdx < 0 || gameIdx >= (int)g_games.size()) return;
    auto& comps = g_games[gameIdx].companions;
    if (compIdx < 0 || compIdx >= (int)comps.size()) return;
    comps.erase(comps.begin() + compIdx);
    SaveGames(g_games); PushStateToJs();
}

static void AddGameFromResolved(const std::wstring& displayPath, const ResolvedDrop& rd) {
    GameEntry g;
    g.name = MakeGameNameFromPath(displayPath);
    g.exePath = rd.launchPath;
    g.iconPath = rd.iconPath;
    g.iconIndex = rd.iconIndex;
    g.color = PickColorForIndex((int)g_games.size());
    g_games.push_back(g);
}

static void AddGameFlow() {
    std::wstring path;
    if (!BrowseForExe(path, g_lang == Lang::EN ? L"Choose the game (exe/lnk/url)" : L"اختر ملف اللعبة (exe/lnk/url)")) return;
    ResolvedDrop rd = ResolveDroppedPath(path);
    if (!rd.valid) { PushStatus(g_lang == Lang::EN ? L"Couldn't understand that file." : L"ما قدرت أفهم هذا الملف.", L"warn"); return; }
    if (GameExistsWithPath(rd.launchPath)) {
        PushStatus(g_lang == Lang::EN ? L"This game is already in the list." : L"هذي اللعبة موجودة بالقائمة أصلاً.", L"warn");
        return;
    }
    AddGameFromResolved(path, rd);
    SaveGames(g_games); PushStateToJs();
}

static void AddCompanionFlow(int gameIdx) {
    if (gameIdx < 0 || gameIdx >= (int)g_games.size()) return;
    std::wstring path;
    if (!BrowseForExe(path, g_lang == Lang::EN ? L"Choose the companion program (exe/lnk/url)" : L"اختر البرنامج المرافق (exe/lnk/url)")) return;
    ResolvedDrop rd = ResolveDroppedPath(path);
    if (!rd.valid) { PushStatus(g_lang == Lang::EN ? L"Couldn't understand that file." : L"ما قدرت أفهم هذا الملف.", L"warn"); return; }
    CompanionEntry ce; ce.path = rd.launchPath;
    g_games[gameIdx].companions.push_back(ce);
    SaveGames(g_games); PushStateToJs();
}

static void StartLauncherFlow() {
    if (IsEngineRunning()) { PushStateToJs(); return; }
    std::wstring exe = GetExeDir() + L"\\GameLauncher.exe";
    ShellExecuteW(nullptr, L"open", exe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    SetTimer(g_hwnd, g_engineStatusTimerId, 500, nullptr);
}
static void StopLauncherFlow() {
    if (!IsEngineRunning()) { PushStateToJs(); return; }
    HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
    if (h) PostMessageW(h, WM_CLOSE, 0, 0);
    SetTimer(g_hwnd, g_engineStatusTimerId, 500, nullptr);
}

static void NotifyLauncherHotkeyChanged() {
    HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
    if (h) PostMessageW(h, WM_APP_RELOAD_HOTKEY, 0, 0);
}
static void NotifyLauncherMuteHotkeyChanged() {
    HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
    if (h) PostMessageW(h, WM_APP_RELOAD_MUTE_HOTKEY, 0, 0);
}
static void NotifyLauncherControllerChanged() {
    HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
    if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
}

static void OpenGameFolder(int idx) {
    if (idx < 0 || idx >= (int)g_games.size()) return;
    std::wstring path = g_games[idx].exePath;
    if (IsUriLike(path) || IsUwpPath(path)) {
        PushStatus(g_lang == Lang::EN
            ? L"Cannot open folder for Steam / UWP games."
            : L"لا يمكن فتح مجلد لعبة Steam أو UWP.", L"warn");
        return;
    }
    size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos) return;
    std::wstring folder = path.substr(0, pos);
    if (GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) {
        PushStatus(g_lang == Lang::EN ? L"Folder not found." : L"المجلد غير موجود.", L"warn");
        return;
    }
    ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ----------------------------- سحب وإفلات -----------------------------
static void ProcessDroppedPaths(const std::vector<std::wstring>& paths) {
    int added = 0, duplicates = 0, folderSuggestions = 0;
    for (auto& p : paths) {
        DWORD attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            std::vector<ExeScanResult> allExes;
            FindAllExesInFolder(p, allExes, 4, 80);
            std::vector<ExeScanResult> newExes;
            for (auto& e : allExes) if (!GameExistsWithPath(e.path)) newExes.push_back(e);
            if (newExes.empty()) { duplicates++; continue; }
            if (newExes.size() == 1) PushSuggestExe(GetFileNameFromPath(p), newExes[0].path);
            else PushSuggestExeList(GetFileNameFromPath(p), newExes);
            folderSuggestions++;
            continue;
        }
        ResolvedDrop rd = ResolveDroppedPath(p);
        if (!rd.valid) continue;
        if (GameExistsWithPath(rd.launchPath)) { duplicates++; continue; }
        AddGameFromResolved(p, rd);
        added++;
    }
    if (added > 0) {
        SaveGames(g_games); PushStateToJs();
        if (duplicates > 0) {
            PushStatus(g_lang == Lang::EN
                ? (L"Added " + std::to_wstring(added) + L", skipped " + std::to_wstring(duplicates) + L" already in the list.")
                : (L"أُضيفت " + std::to_wstring(added) + L"، وتم تخطي " + std::to_wstring(duplicates) + L" موجودة أصلاً."),
                L"info");
        }
    } else if (folderSuggestions > 0) {
    } else if (duplicates > 0) {
        PushStatus(g_lang == Lang::EN ? L"This game is already in the list." : L"هذي اللعبة موجودة بالقائمة أصلاً.", L"warn");
    } else {
        PushStatus(g_lang == Lang::EN ? L"Drag a valid .exe, .lnk, .url file, or a game folder." : L"لازم تسحب ملف exe، اختصار (.lnk)، رابط (.url)، أو مجلد لعبة صالح.", L"warn");
    }
}

static void HandleDroppedFilesMessage(ICoreWebView2WebMessageReceivedEventArgs* args) {
    ComPtr<ICoreWebView2WebMessageReceivedEventArgs2> args2;
    if (FAILED(args->QueryInterface(IID_PPV_ARGS(&args2))) || !args2) return;
    ComPtr<ICoreWebView2ObjectCollectionView> objects;
    if (FAILED(args2->get_AdditionalObjects(&objects)) || !objects) return;
    UINT count = 0; objects->get_Count(&count);
    std::vector<std::wstring> paths;
    for (UINT i = 0; i < count; i++) {
        ComPtr<IUnknown> obj;
        if (FAILED(objects->GetValueAtIndex(i, &obj)) || !obj) continue;
        ComPtr<ICoreWebView2File> file;
        if (FAILED(obj.As(&file)) || !file) continue;
        LPWSTR path = nullptr;
        if (SUCCEEDED(file->get_Path(&path)) && path) { paths.push_back(path); CoTaskMemFree(path); }
    }
    ProcessDroppedPaths(paths);
}

static void HandleDropOnPanel(HDROP hDrop) {
    UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> paths;
    for (UINT i = 0; i < count; i++) {
        wchar_t buf[MAX_PATH];
        if (DragQueryFileW(hDrop, i, buf, MAX_PATH) != 0) paths.push_back(buf);
    }
    DragFinish(hDrop);
    ProcessDroppedPaths(paths);
}

static void ChangeHotkeyFromJs(UINT modifiers, UINT vk) {
    if (vk == 0) return;
    g_hotkey.modifiers = modifiers;
    g_hotkey.vk = vk;
    SaveHotkey(g_hotkey);
    NotifyLauncherHotkeyChanged();
    PushStateToJs();
}

// ----------------------------- استقبال رسائل JS -----------------------------
static void HandleWebMessage(const std::wstring& json) {
    std::wstring action = JsonGetString(json, L"action");

    if (action == L"ready")            { PushStateToJs(); }
    else if (action == L"addGame")     { AddGameFlow(); }
    else if (action == L"removeGame")  { RemoveGameAt((int)JsonGetNumber(json, L"index")); }
    else if (action == L"addCompanion"){ AddCompanionFlow((int)JsonGetNumber(json, L"gameIndex")); }
    else if (action == L"removeCompanion") {
        RemoveCompanionAt((int)JsonGetNumber(json, L"gameIndex"), (int)JsonGetNumber(json, L"companionIndex"));
    }
    else if (action == L"reorderGames") { ReorderGames(JsonGetNumberArray(json, L"order")); }
    else if (action == L"startEngine") { StartLauncherFlow(); }
    else if (action == L"stopEngine")  { StopLauncherFlow(); }
    else if (action == L"setLang") {
        std::wstring lang = JsonGetString(json, L"lang");
        g_lang = (lang == L"en") ? Lang::EN : Lang::AR;
        SaveLanguage(g_lang); PushStateToJs();
    }
    else if (action == L"setRadialStyle") {
        long s = JsonGetNumber(json, L"style", 0);
        if (s >= 0 && s < RadialStyleCount()) SaveRadialStyle((RadialStyle)s);
        PushStateToJs();
    }
    // ✅ شفافية الدائرة
    else if (action == L"setRadialTransparency") {
        // نحصل على القيمة كنص عشري
        std::wstring vstr = JsonGetString(json, L"value");
        float v = 0.72f;
        if (!vstr.empty()) {
            try { v = std::stof(vstr); } catch (...) { v = 0.72f; }
        }
        SaveRadialTransparency(v);
        PushStateToJs();
    }
    else if (action == L"setPanelPreset") {
        std::wstring preset = JsonGetString(json, L"preset");
        if (!preset.empty()) SavePanelPreset(preset);
        PushStateToJs();
    }
    else if (action == L"addDroppedPaths") { ProcessDroppedPaths(JsonGetStringArray(json, L"paths")); }
    else if (action == L"addSuggestedExe") {
        std::wstring path = JsonGetString(json, L"path");
        if (!path.empty()) ProcessDroppedPaths({ path });
    }
    else if (action == L"addSelectedExes") {
        auto paths = JsonGetStringArray(json, L"paths");
        int added = 0;
        for (auto& p : paths) {
            if (p.empty() || GameExistsWithPath(p)) continue;
            ResolvedDrop rd = ResolveDroppedPath(p);
            if (!rd.valid) continue;
            AddGameFromResolved(p, rd);
            added++;
        }
        if (added > 0) { SaveGames(g_games); PushStateToJs(); }
    }
    else if (action == L"addUwpApp") {
        std::wstring aumid = TrimString(JsonGetString(json, L"aumid"));
        if (aumid.empty()) return;
        std::wstring path = MakeUwpPath(aumid);
        if (GameExistsWithPath(path)) return;
        GameEntry g;
        std::wstring customName = TrimString(JsonGetString(json, L"name"));
        g.name = customName.empty() ? DefaultUwpName(aumid) : customName;
        g.exePath = path;
        g.color = PickColorForIndex((int)g_games.size());
        g_games.push_back(g);
        SaveGames(g_games); PushStateToJs();
    }
    else if (action == L"setAutoStart") {
        SetAutoStartEnabled(JsonGetNumber(json, L"enabled", 0) != 0);
        PushStateToJs();
    }
    else if (action == L"setAutoLayoutSwitch") {
        SaveAutoLayoutSwitch(JsonGetNumber(json, L"enabled", 0) != 0);
        PushStateToJs();
    }
    else if (action == L"setGameAutoLangSwitch") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].autoLangSwitch = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"changeHotkey") {
        UINT mods = (UINT)JsonGetNumber(json, L"modifiers", 0);
        UINT vk = (UINT)JsonGetNumber(json, L"vk", 0);
        ChangeHotkeyFromJs(mods, vk);
    }
    else if (action == L"setGameRunAsAdmin") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].runAsAdmin = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameLaunchArgs") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].launchArgs = JsonGetString(json, L"args");
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameShowInRadial") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].showInRadial = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameColor") {
        int idx = (int)JsonGetNumber(json, L"index");
        long c = JsonGetNumber(json, L"color", -1);
        if (idx >= 0 && idx < (int)g_games.size() && c >= 0) {
            g_games[idx].color = (COLORREF)(unsigned long)c;
            SaveGames(g_games); PushStateToJs();
        }
    }
    // ✅ لون مخصص عبر نافذة ويندوز
    else if (action == L"browseGameCustomColor") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            COLORREF c = g_games[idx].color;
            if (BrowseForCustomColor(c, c)) {
                g_games[idx].color = c;
                SaveGames(g_games); PushStateToJs();
            }
        }
    }
    else if (action == L"setGameFavorite") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].favorite = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"resetGamePlayTime") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].totalPlaySeconds = 0;
            g_games[idx].playCount = 0;
            g_games[idx].lastPlayedUnix = 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"browseGameRadialBg") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            std::wstring p;
            if (BrowseForImage(p,
                    g_lang == Lang::EN ? L"Choose a radial background"
                                       : L"اختر خلفية دائرية للعبة")) {
                g_games[idx].radialBgPath = p;
                SaveGames(g_games); PushStateToJs();
            }
        }
    }
    else if (action == L"clearGameRadialBg") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].radialBgPath.clear();
            g_games[idx].hideOriginalIcon = false;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameHideOriginalIcon") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].hideOriginalIcon = JsonGetNumber(json, L"enabled", 0) != 0
                                             && !g_games[idx].radialBgPath.empty();
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameProcessPriority") {
        int idx = (int)JsonGetNumber(json, L"index");
        int prio = (int)JsonGetNumber(json, L"priority", 0);
        if (idx >= 0 && idx < (int)g_games.size() && prio >= 0 && prio < ProcessPriorityCount()) {
            g_games[idx].processPriority = ProcessPriorityFromIndex(prio);
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameAffinity") {
        int idx = (int)JsonGetNumber(json, L"index");
        long long mask = JsonGetLongLong(json, L"mask", 0);
        int enabled = (int)JsonGetNumber(json, L"enabled", 0);
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].applyAffinity = (enabled != 0);
            g_games[idx].affinityMask = (unsigned long long)mask;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setHubAlwaysVisible") {
        SaveHubAlwaysVisible(JsonGetNumber(json, L"enabled", 0) != 0);
        PushStateToJs();
    }

    else if (action == L"setControllerEnabled") {
        ControllerSettings cs = LoadControllerSettings();
        cs.enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveControllerSettings(cs);
        NotifyLauncherControllerChanged();
        PushStateToJs();
    }
    else if (action == L"setControllerButton") {
        long btn = JsonGetNumber(json, L"button", 0x0010);
        ControllerSettings cs = LoadControllerSettings();
        cs.openRadialButton = (DWORD)btn;
        SaveControllerSettings(cs);
        NotifyLauncherControllerChanged();
        PushStateToJs();
    }
    else if (action == L"changeMuteHotkey") {
        UINT mods = (UINT)JsonGetNumber(json, L"modifiers", 0);
        UINT vk = (UINT)JsonGetNumber(json, L"vk", 0);
        if (vk != 0) {
            MuteSettings ms = LoadMuteSettings();
            ms.modifiers = mods;
            ms.vk = vk;
            SaveMuteSettings(ms);
            NotifyLauncherMuteHotkeyChanged();
            PushStateToJs();
        }
    }
    else if (action == L"setMuteEnabled") {
        MuteSettings ms = LoadMuteSettings();
        ms.enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveMuteSettings(ms);
        NotifyLauncherMuteHotkeyChanged();
        PushStateToJs();
    }
    else if (action == L"toggleMuteNow") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            std::wstring reqPath = GetAppDataDir() + L"\\mute_request.txt";
            std::wstring tmp = reqPath + L".tmp";
            {
                std::wofstream f(tmp.c_str(), std::ios::trunc);
                if (f.is_open()) f << g_games[idx].exePath << L"\n";
            }
            MoveFileExW(tmp.c_str(), reqPath.c_str(), MOVEFILE_REPLACE_EXISTING);
            SignalMuteRequestToLauncher();
            KillTimer(g_hwnd, PUSH_STATE_TIMER_ID);
            SetTimer(g_hwnd, PUSH_STATE_TIMER_ID, 150, nullptr);
        }
    }
    else if (action == L"unmuteApp") {
        std::wstring exeName = JsonGetString(json, L"exeName");
        if (!exeName.empty()) {
            std::wstring reqPath = GetAppDataDir() + L"\\mute_request.txt";
            std::wstring tmp = reqPath + L".tmp";
            {
                std::wofstream f(tmp.c_str(), std::ios::trunc);
                f << L"*unmute:" << exeName << L"\n";
            }
            MoveFileExW(tmp.c_str(), reqPath.c_str(), MOVEFILE_REPLACE_EXISTING);
            SignalMuteRequestToLauncher();
            KillTimer(g_hwnd, PUSH_STATE_TIMER_ID);
            SetTimer(g_hwnd, PUSH_STATE_TIMER_ID, 150, nullptr);
        }
    }
    else if (action == L"getMutedApps") {
        std::vector<MuteAudio::MutedAppInfo> apps = MuteAudio::GetMutedApps();
        std::wstringstream j;
        j << L"{\"type\":\"mutedAppsList\",\"apps\":[";
        for (size_t i = 0; i < apps.size(); i++) {
            if (i) j << L",";
            j << L"{\"exeName\":\"" << JsonEscape(apps[i].exeName)
              << L"\",\"displayName\":\"" << JsonEscape(apps[i].displayName) << L"\"}";
        }
        j << L"]}";
        if (g_webview) g_webview->PostWebMessageAsString(j.str().c_str());
    }
    else if (action == L"setCompanionRunAsAdmin") {
        int gi = (int)JsonGetNumber(json, L"gameIndex");
        int ci = (int)JsonGetNumber(json, L"companionIndex");
        if (gi >= 0 && gi < (int)g_games.size() && ci >= 0 && ci < (int)g_games[gi].companions.size()) {
            g_games[gi].companions[ci].runAsAdmin = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setCompanionLaunchArgs") {
        int gi = (int)JsonGetNumber(json, L"gameIndex");
        int ci = (int)JsonGetNumber(json, L"companionIndex");
        if (gi >= 0 && gi < (int)g_games.size() && ci >= 0 && ci < (int)g_games[gi].companions.size()) {
            g_games[gi].companions[ci].launchArgs = JsonGetString(json, L"args");
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"setCompanionShowInRadial") {
        int gi = (int)JsonGetNumber(json, L"gameIndex");
        int ci = (int)JsonGetNumber(json, L"companionIndex");
        if (gi >= 0 && gi < (int)g_games.size() && ci >= 0 && ci < (int)g_games[gi].companions.size()) {
            g_games[gi].companions[ci].showInRadial = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGames(g_games); PushStateToJs();
        }
    }
    else if (action == L"browsePanelBackground") {
        std::wstring p;
        if (BrowseForImage(p, g_lang == Lang::EN ? L"Choose a control panel background" : L"اختر خلفية لوحة التحكم")) {
            SavePanelBackground(p); PushStateToJs();
        }
    }
    else if (action == L"browseRadialBackground") {
        std::wstring p;
        if (BrowseForImage(p, g_lang == Lang::EN ? L"Choose a game circle background" : L"اختر خلفية دائرة الألعاب")) {
            SaveRadialBackground(p); PushStateToJs();
        }
    }
    else if (action == L"clearPanelBackground")  { SavePanelBackground(L"");  PushStateToJs(); }
    else if (action == L"clearRadialBackground") { SaveRadialBackground(L""); PushStateToJs(); }
    else if (action == L"exportBackup") {
        std::wstring p;
        if (BrowseForJsonSave(p,
                g_lang == Lang::EN ? L"Save games backup" : L"حفظ نسخة احتياطية",
                L"GameLauncher-backup.json")) {
            if (ExportBackupToFile(p))
                PushStatus(g_lang == Lang::EN ? L"Backup saved successfully." : L"تم حفظ النسخة الاحتياطية بنجاح.", L"info");
            else
                PushStatus(g_lang == Lang::EN ? L"Failed to save backup." : L"فشل حفظ النسخة الاحتياطية.", L"error");
        }
    }
    else if (action == L"importBackup") {
        std::wstring p;
        if (BrowseForJsonOpen(p,
                g_lang == Lang::EN ? L"Select games backup to import" : L"اختر النسخة الاحتياطية للاستيراد")) {
            if (ImportBackupFromFile(p)) {
                PushStatus(g_lang == Lang::EN ? L"Backup imported successfully." : L"تم استيراد النسخة بنجاح.", L"info");
                PushStateToJs();
            } else {
                PushStatus(g_lang == Lang::EN ? L"Invalid or empty backup file." : L"الملف غير صالح أو فارغ.", L"error");
            }
        }
    }
    else if (action == L"openLogFile") {
        std::wstring logPath = GetAppDataDir() + L"\\launcher.log";
        if (GetFileAttributesW(logPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            PushStatus(g_lang == Lang::EN ? L"No log file yet." : L"لا يوجد ملف سجل بعد.", L"warn");
        } else {
            ShellExecuteW(nullptr, L"open", logPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
    else if (action == L"openGameFolder") {
        OpenGameFolder((int)JsonGetNumber(json, L"index"));
    }
}

// ----------------------------- WebView2 -----------------------------
static void ResizeWebView() {
    if (!g_controller) return;
    RECT bounds; GetClientRect(g_hwnd, &bounds);
    g_controller->put_Bounds(bounds);
}

static void InitWebView(HWND hwnd) {
    std::wstring userDataDir = GetAppDataDir() + L"\\WebView2Data";
    CreateDirectoryW(userDataDir.c_str(), nullptr);

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataDir.c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    MessageBoxW(hwnd,
                        L"تعذّر تشغيل WebView2 (تأكد من تثبيت \"WebView2 Runtime\" من مايكروسوفت).",
                        L"GameLauncher", MB_OK | MB_ICONERROR);
                    return S_OK;
                }
                env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                            if (FAILED(result) || !controller) return S_OK;
                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);
                            ComPtr<ICoreWebView2Settings> settings;
                            if (g_webview) {
                                g_webview->get_Settings(&settings);
                                if (settings) {
                                    settings->put_AreDefaultContextMenusEnabled(FALSE);
                                    settings->put_IsZoomControlEnabled(FALSE);
                                    settings->put_IsStatusBarEnabled(FALSE);
                                }
                            }
                            DragAcceptFiles(hwnd, TRUE);
                            ResizeWebView();
                            g_controller->put_IsVisible(TRUE);

                            std::wstring uiPath;
                            // ✅ cache-busting ببصمة الـ EXE
                            wchar_t selfPath[MAX_PATH];
                            GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
                            WIN32_FILE_ATTRIBUTE_DATA fad;
                            ULONGLONG sig = 0;
                            if (GetFileAttributesExW(selfPath, GetFileExInfoStandard, &fad)) {
                                ULONGLONG t = ((ULONGLONG)fad.ftLastWriteTime.dwHighDateTime << 32) | fad.ftLastWriteTime.dwLowDateTime;
                                ULONGLONG s = ((ULONGLONG)fad.nFileSizeHigh << 32) | fad.nFileSizeLow;
                                sig = t ^ (s * 0x9E3779B97F4A7C15ULL);
                            }
                            wchar_t sigStr[32];
                            wsprintfW(sigStr, L"%llX", sig);

                            if (ExtractEmbeddedHtml(uiPath)) {
                                std::wstring url = L"file:///" + uiPath + L"?v=" + sigStr;
                                for (auto& c : url) if (c == L'\\') c = L'/';
                                if (g_webview) g_webview->Navigate(url.c_str());
                            } else {
                                uiPath = GetExeDir() + L"\\panel_ui.html";
                                std::wstring url = L"file:///" + uiPath + L"?v=" + sigStr;
                                for (auto& c : url) if (c == L'\\') c = L'/';
                                if (g_webview) g_webview->Navigate(url.c_str());
                            }

                            EventRegistrationToken token;
                            if (g_webview) {
                                g_webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            LPWSTR msg = nullptr;
                                            if (SUCCEEDED(args->TryGetWebMessageAsString(&msg)) && msg) {
                                                std::wstring raw(msg); CoTaskMemFree(msg);
                                                if (raw == L"addDroppedPaths") HandleDroppedFilesMessage(args);
                                                else HandleWebMessage(raw);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &token);
                            }
                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());

    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"CreateCoreWebView2EnvironmentWithOptions فشل.", L"GameLauncher", MB_OK | MB_ICONERROR);
    }
}

// ----------------------------- نافذة المضيف -----------------------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_lang = LoadLanguage();
        LoadGames(g_games);
        g_hotkey = LoadHotkey();
        InitWebView(hwnd);
        SetTimer(hwnd, g_engineStatusTimerId, 5000, nullptr);
        return 0;
    case WM_SIZE: {
        ResizeWebView();
        if (wp == SIZE_RESTORED && g_wasMinimized) {
            g_wasMinimized = false;
            PushStateToJs();
        } else if (wp == SIZE_MINIMIZED) {
            g_wasMinimized = true;
        }
        return 0;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hwnd, &rc);
        HBRUSH dark = CreateSolidBrush(RGB(0x0d, 0x0b, 0x18));
        FillRect(hdc, &rc, dark); DeleteObject(dark);
        return 1;
    }
    case WM_TIMER:
        if (wp == (WPARAM)g_engineStatusTimerId) {
            if (IsIconic(hwnd)) return 0;
            bool current = IsEngineRunning();
            if (g_firstStatusCheck || current != g_lastEngineStatus) {
                g_lastEngineStatus = current;
                g_firstStatusCheck = false;
                PushStateToJs();
            }
        }
        else if (wp == (WPARAM)PUSH_STATE_TIMER_ID) {
            KillTimer(hwnd, PUSH_STATE_TIMER_ID);
            PushStateToJs();
        }
        return 0;
    case WM_DROPFILES: HandleDropOnPanel((HDROP)wp); return 0;
    case WM_SETFOCUS:
        if (g_controller) g_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        return 0;
    case WM_CLOSE: DestroyWindow(hwnd); return 0;
    case WM_DESTROY:
        KillTimer(hwnd, g_engineStatusTimerId);
        KillTimer(hwnd, PUSH_STATE_TIMER_ID);
        if (g_controller) { g_controller->Close(); g_controller = nullptr; g_webview = nullptr; }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int RunPanelApp(HINSTANCE hInst) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    g_singleInstanceMutex = CreateMutexW(nullptr, TRUE, PANEL_SINGLE_INSTANCE_MUTEX);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(CLASS_NAME, nullptr);
        if (existing) {
            if (IsIconic(existing)) ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        if (g_singleInstanceMutex) CloseHandle(g_singleInstanceMutex);
        Gdiplus::GdiplusShutdown(gdiplusToken);
        CoUninitialize(); return 0;
    }

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDI_APPICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"GameLauncher — Control Panel",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1180, 760,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg); DispatchMessageW(&msg);
    }
    if (g_singleInstanceMutex) { ReleaseMutex(g_singleInstanceMutex); CloseHandle(g_singleInstanceMutex); }
    Gdiplus::GdiplusShutdown(gdiplusToken);
    CoUninitialize();
    return 0;
}