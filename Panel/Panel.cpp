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
#include "../Common/MiniJson.h"
#include "../Common/GameImport.h"
#include "../Common/Resource.h"
#include "../Common/MuteAudio.h"
#include "../Common/PerformanceMonitor.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <oleidl.h>
#include <dwmapi.h>
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
#include <cmath>
#include <cctype>
#include <cstdio>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "dwmapi.lib")

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
static bool g_glassEffectActive = false;

static ComPtr<ICoreWebView2Controller> g_controller;
static ComPtr<ICoreWebView2> g_webview;

static std::map<std::wstring, std::wstring> g_iconUriCache;
static std::map<std::wstring, std::wstring> g_radialBgUriCache;
static std::wstring g_iconCacheKey;

static std::set<std::wstring> g_runningGamesCache;
static std::set<std::wstring> g_muteStateCache;
static std::wstring g_runningGamesCacheStamp;
static std::wstring g_muteStateCacheStamp;

// ============================================================
// المظهر الزجاجي الحقيقي (Acrylic Blur)
// ============================================================
typedef enum _ACCENT_STATE_LOCAL {
    ACCENT_DISABLED_LOCAL                    = 0,
    ACCENT_ENABLE_GRADIENT_LOCAL             = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT_LOCAL  = 2,
    ACCENT_ENABLE_BLURBEHIND_LOCAL           = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND_LOCAL    = 4,
    ACCENT_ENABLE_HOSTBACKDROP_LOCAL         = 5,
} ACCENT_STATE_LOCAL;

typedef struct _ACCENT_POLICY_LOCAL {
    ACCENT_STATE_LOCAL AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY_LOCAL;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA_LOCAL {
    DWORD Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA_LOCAL;

typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA_LOCAL*);

static const DWORD WCA_ACCENT_POLICY_LOCAL = 19;

static bool SetWindowGlass(HWND hwnd, bool enabled) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return false;
    auto fn = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!fn) return false;

    ACCENT_POLICY_LOCAL accent = {};
    if (enabled) {
        accent.AccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND_LOCAL;
        accent.GradientColor = 0x99201A30;
        accent.AccentFlags = 0;
    } else {
        accent.AccentState = ACCENT_DISABLED_LOCAL;
    }
    WINDOWCOMPOSITIONATTRIBDATA_LOCAL data = {};
    data.Attrib = WCA_ACCENT_POLICY_LOCAL;
    data.pvData = &accent;
    data.cbData = sizeof(accent);
    return fn(hwnd, &data) != FALSE;
}

static void SetWebViewTransparent(bool transparent) {
    if (!g_controller) return;
    ComPtr<ICoreWebView2Controller2> ctrl2;
    if (SUCCEEDED(g_controller.As(&ctrl2)) && ctrl2) {
        COREWEBVIEW2_COLOR bg;
        if (transparent) {
            bg.A = 0; bg.R = 0; bg.G = 0; bg.B = 0;
        } else {
            bg.A = 255; bg.R = 0x0d; bg.G = 0x0b; bg.B = 0x18;
        }
        ctrl2->put_DefaultBackgroundColor(bg);
    }
}

static void ApplyGlassMode(bool enabled) {
    g_glassEffectActive = enabled;
    SetWindowGlass(g_hwnd, enabled);
    SetWebViewTransparent(enabled);
}

// ----------------------------- استخراج الملفات المُدمجة -----------------------------
// عنوان مجلد الأصول المحلية للواجهة (ui_assets بجانب البرنامج) بصيغة file:/// مشفّرة.
// إن لم يوجد المجلد تسقط الواجهة تلقائياً على الـ CDN (انظر الروابط في panel_ui.html).
static std::string BuildAssetsUrlPrefix() {
    std::wstring dir = GetExeDir() + L"\\ui_assets\\";
    int n = WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), nullptr, 0, nullptr, nullptr);
    std::string u8;
    if (n > 0) {
        u8.resize((size_t)n);
        WideCharToMultiByte(CP_UTF8, 0, dir.c_str(), (int)dir.size(), &u8[0], n, nullptr, nullptr);
    }
    std::string out = "file:///";
    for (unsigned char c : u8) {
        if (c == '\\') out += '/';
        else if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/' || c == ':') out += (char)c;
        else { char b[4]; sprintf_s(b, "%%%02X", (unsigned)c); out += b; }
    }
    return out;
}

static bool ExtractResourceToFile(int resId, const std::wstring& outPath,
                                   const std::string& buildSigUtf8, const std::string& assetsUrlUtf8) {
    HRSRC hRes = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(nullptr, hRes);
    const char* data = (const char*)LockResource(hData);
    if (!data || size == 0) return false;

    std::string content(data, size);

    const std::string placeholder = "__GL_BUILD_SIG__";
    size_t pos = 0;
    while ((pos = content.find(placeholder, pos)) != std::string::npos) {
        content.replace(pos, placeholder.length(), buildSigUtf8);
        pos += buildSigUtf8.length();
    }

    const std::string assetsPlaceholder = "__GL_ASSETS__";
    pos = 0;
    while ((pos = content.find(assetsPlaceholder, pos)) != std::string::npos) {
        content.replace(pos, assetsPlaceholder.length(), assetsUrlUtf8);
        pos += assetsUrlUtf8.length();
    }

    std::ofstream f(outPath.c_str(), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(content.data(), (std::streamsize)content.size());
    f.flush();
    const bool wroteOk = f.good();
    f.close();
    return wroteOk;     // لو فشلت الكتابة يعتمد الكود على المسار الاحتياطي بدل صفحة فارغة
}

static bool ExtractEmbeddedFiles(std::wstring& outHtmlPath, const std::wstring& buildSig) {
    std::wstring dir = GetAppDataDir();
    outHtmlPath = dir + L"\\panel_ui.html";
    std::wstring cssPath = dir + L"\\panel_ui.css";
    std::wstring jsPath  = dir + L"\\panel_ui.js";

    std::string sigUtf8;
    if (!buildSig.empty()) {
        int wlen = (int)buildSig.size();
        int u8len = WideCharToMultiByte(CP_UTF8, 0, buildSig.c_str(), wlen, nullptr, 0, nullptr, nullptr);
        if (u8len > 0) {
            sigUtf8.resize(u8len);
            WideCharToMultiByte(CP_UTF8, 0, buildSig.c_str(), wlen, &sigUtf8[0], u8len, nullptr, nullptr);
        }
    }

    const std::string assetsUrl = BuildAssetsUrlPrefix();
    bool okHtml = ExtractResourceToFile(IDR_PANEL_HTML, outHtmlPath, sigUtf8, assetsUrl);
    bool okCss  = ExtractResourceToFile(IDR_PANEL_CSS,  cssPath,      sigUtf8, assetsUrl);
    bool okJs   = ExtractResourceToFile(IDR_PANEL_JS,   jsPath,       sigUtf8, assetsUrl);

    return okHtml && okCss && okJs;
}

static void SignalMuteRequestToLauncher() {
    HANDLE hEvent = OpenEventW(EVENT_MODIFY_STATE, FALSE, MUTE_REQUEST_EVENT_NAME);
    if (hEvent) { SetEvent(hEvent); CloseHandle(hEvent); }
}

static bool IsEngineElevated() {
    std::wstring adminFile = GetAppDataDir() + L"\\engine_admin.txt";
    Utf8In f(adminFile.c_str());
    if (!f.is_open()) return false;
    std::wstring line;
    std::getline(f, line);
    return TrimString(line) == L"1";
}

// أسماء ملفات Unreal Engine مثل Endeavor-Win64-Shipping.exe لا تدل على اسم اللعبة:
// نأخذ اسم مجلد اللعبة الذي يعلو Binaries:  ...\<اللعبة>\<المشروع>\Binaries\Win64\X-Win64-Shipping.exe
static std::wstring FriendlyNameForShippingExe(const std::wstring& path, const std::wstring& baseName) {
    auto lower = [](std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; };
    const std::wstring lb = lower(baseName);
    if (lb.find(L"-shipping") == std::wstring::npos && lb.find(L"-win64-") == std::wstring::npos &&
        lb.find(L"-wingdk-") == std::wstring::npos) return baseName;
    std::vector<std::wstring> parts;
    {
        std::wstring cur;
        for (wchar_t c : path) {
            if (c == L'\\' || c == L'/') { parts.push_back(cur); cur.clear(); }
            else cur += c;
        }
        parts.push_back(cur);
    }
    int bin = -1;
    for (int i = (int)parts.size() - 1; i >= 0; i--) if (lower(parts[i]) == L"binaries") { bin = i; break; }
    if (bin < 1) return baseName;
    auto generic = [&](const std::wstring& n) {
        const std::wstring l = lower(n);
        return l.empty() || l == L"common" || l == L"games" || l == L"steamapps" || l == L"content" ||
               l == L"program files" || l == L"program files (x86)" || l == L"epic games" ||
               l == L"xboxgames" || (l.size() == 2 && l[1] == L':');
    };
    if (bin >= 2 && !generic(parts[bin - 2])) return parts[bin - 2];
    if (!generic(parts[bin - 1])) return parts[bin - 1];
    return baseName;
}

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
    return FriendlyNameForShippingExe(path, base);
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

// الـ Panel والـ Engine كلاهما يكتب games.cfg: المحرك يحدّث وقت اللعب وعدد المرات أثناء اللعب.
// قبل الحفظ نقرأ نسخة القرص وندمج منها الإحصائيات الأحدث (الأكبر) كي لا نكتب فوقها بقيم قديمة.
static void SaveGamesSafe(std::vector<GameEntry>& games) {
    std::vector<GameEntry> disk;
    LoadGames(disk);
    for (auto& g : games) {
        for (auto& d : disk) {
            if (_wcsicmp(d.exePath.c_str(), g.exePath.c_str()) != 0) continue;
            if (d.totalPlaySeconds > g.totalPlaySeconds) g.totalPlaySeconds = d.totalPlaySeconds;
            if (d.playCount > g.playCount) g.playCount = d.playCount;
            if (d.lastPlayedUnix > g.lastPlayedUnix) g.lastPlayedUnix = d.lastPlayedUnix;
            break;
        }
    }
    SaveGames(games);
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
        Utf8In f(rgPath.c_str());
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
        Utf8In f(msPath.c_str());
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
    OverlaySettings os = LoadOverlaySettings();
    SYSTEM_INFO si = {};
    GetSystemInfo(&si);

    std::wstringstream j;
    j << L"{\"type\":\"state\",";
    j << L"\"lang\":\"" << (g_lang == Lang::EN ? L"en" : L"ar") << L"\",";
    j << L"\"hotkey\":\"" << JsonEscape(HotkeyToString(g_hotkey)) << L"\",";
    j << L"\"engineRunning\":" << (IsEngineRunning() ? L"true" : L"false") << L",";
    j << L"\"autoStart\":" << (IsAutoStartEnabled() ? L"true" : L"false") << L",";
    j << L"\"radialStyle\":" << (int)LoadRadialStyle() << L",";
    j << L"\"radialTransparency\":" << LoadRadialTransparency() << L",";
    j << L"\"radialNoGlow\":" << (LoadRadialNoGlow() ? L"true" : L"false") << L",";
    j << L"\"sharpMode\":" << (LoadSharpMode() ? L"true" : L"false") << L",";
    {
        RadialScale rs = LoadRadialScale();
        j << L"\"radialScale\":{"
            << L"\"iconSize\":" << rs.iconSize
            << L",\"hubSize\":" << rs.hubSize
            << L",\"orbitDist\":" << rs.orbitDist
            << L"},";
    }
    {
        CustomTheme ct = LoadCustomTheme();
        j << L"\"customTheme\":{"
            << L"\"accent\":\"" << JsonEscape(ct.accentHex) << L"\""
            << L",\"secondary\":\"" << JsonEscape(ct.secondaryHex) << L"\""
            << L",\"bg\":\"" << JsonEscape(ct.bgHex) << L"\""
            << L",\"card\":\"" << JsonEscape(ct.cardHex) << L"\""
            << L",\"cardOpacity\":" << ct.cardOpacity
            << L",\"accentStrength\":" << ct.accentStrength
            << L",\"bgGlow\":" << ct.bgGlow
            << L",\"isActive\":" << (ct.isActive ? L"true" : L"false")
            << L"},";
    }
    {
        CustomRadialStyle crs = LoadCustomRadialStyle();
        j << L"\"customRadialStyle\":{"
            << L"\"ringCount\":" << crs.ringCount
            << L",\"ringThickness\":" << crs.ringThickness
            << L",\"dashed\":" << (crs.dashed ? L"true" : L"false")
            << L",\"glowLayers\":" << crs.glowLayers
            << L",\"glowStrength\":" << crs.glowStrength
            << L",\"isActive\":" << (crs.isActive ? L"true" : L"false")
            << L"},";
    }
    j << L"\"panelBackground\":\"" << JsonEscape(LoadPanelBackground()) << L"\",";
    j << L"\"radialBackground\":\"" << JsonEscape(LoadRadialBackground()) << L"\",";
    j << L"\"panelPreset\":\"" << JsonEscape(LoadPanelPreset()) << L"\",";
    j << L"\"muteEnabled\":" << (LoadMuteSettings().enabled ? L"true" : L"false") << L",";
    j << L"\"muteHotkey\":\"" << JsonEscape(MuteHotkeyToString(LoadMuteSettings())) << L"\",";
    j << L"\"hubAlwaysVisible\":" << (LoadHubAlwaysVisible() ? L"true" : L"false") << L",";
    j << L"\"controllerEnabled\":" << (cs.enabled ? L"true" : L"false") << L",";
    j << L"\"controllerButton\":" << (unsigned long)cs.openRadialButton << L",";
    j << L"\"controllerButtonName\":\"" << JsonEscape(ControllerButtonName(cs.openRadialButton, g_lang)) << L"\",";
    j << L"\"controllerToggleMode\":" << (cs.toggleMode ? L"true" : L"false") << L",";
    j << L"\"allowControllerDuringGame\":" << (cs.allowControllerDuringGame ? L"true" : L"false") << L",";
    j << L"\"panelGlassEffect\":" << (LoadPanelGlassEffect() ? L"true" : L"false") << L",";
    j << L"\"overlayEnabled\":" << (os.enabled ? L"true" : L"false") << L",";
    j << L"\"overlayHotkey\":\"" << JsonEscape(OverlayHotkeyToString(os)) << L"\",";
    j << L"\"overlayShowOnGameLaunch\":" << (os.showOnGameLaunch ? L"true" : L"false") << L",";
    j << L"\"overlayOpacity\":" << os.opacity << L",";
    j << L"\"isAdmin\":" << (IsEngineElevated() ? L"true" : L"false") << L",";
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
          << L",\"quickSlot\":" << g.quickSlot
          << L",\"gameLanguage\":" << g.gameLanguage
          << L",\"processPriority\":" << (int)g.processPriority
          << L",\"applyAffinity\":" << (g.applyAffinity ? L"true" : L"false")
          << L",\"affinityMask\":" << g.affinityMask
          << L",\"performanceMonitor\":" << (g.performanceMonitor ? L"true" : L"false")
          << L",\"performanceCpuTemp\":" << (g.performanceCpuTemp ? L"true" : L"false")
          << L",\"boostFps\":" << (g.boostFps ? L"true" : L"false")
          << L",\"boostDisableCore0\":" << (g.boostDisableCore0 ? L"true" : L"false")
          << L",\"boostHighPriority\":" << (g.boostHighPriority ? L"true" : L"false")
          << L",\"boostStopStats\":" << (g.boostStopStats ? L"true" : L"false")
          << L",\"boostTimerResolution\":" << (g.boostTimerResolution ? L"true" : L"false")
          << L",\"boostSystemResponsiveness\":" << (g.boostSystemResponsiveness ? L"true" : L"false")
          << L",\"boostMmcss\":" << (g.boostMmcss ? L"true" : L"false")
          << L",\"perfSessionCount\":0"
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

static std::wstring BuildStatsUpdateJson() {
    RefreshSharedCaches();
    std::wstringstream j;
    j << L"{\"type\":\"statsUpdate\",\"games\":[";
    for (size_t i = 0; i < g_games.size(); i++) {
        if (i) j << L",";
        auto& g = g_games[i];
        j << L"{\"index\":" << i
          << L",\"totalPlaySeconds\":" << g.totalPlaySeconds
          << L",\"lastPlayedUnix\":" << (unsigned long)g.lastPlayedUnix
          << L",\"playCount\":" << g.playCount
          << L",\"isRunning\":" << (IsGameRunningCached(g.exePath) ? L"true" : L"false")
          << L",\"muted\":" << (IsGameMutedCached(g.exePath) ? L"true" : L"false")
          << L"}";
    }
    j << L"]}";
    return j.str();
}

static void PushStateToJs() {
    if (!g_webview) return;
    LoadGames(g_games);
    g_webview->PostWebMessageAsString(BuildStateJson().c_str());
}

static void PushStatsUpdateToJs() {
    if (!g_webview) return;
    LoadGames(g_games);
    g_webview->PostWebMessageAsString(BuildStatsUpdateJson().c_str());
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
                                const std::vector<ExeScanResult>& items, bool library = false) {
    if (!g_webview) return;
    std::wstringstream j;
    j << L"{\"type\":\"suggestExeList\",\"folder\":\"" << JsonEscape(folderName)
      << L"\",\"library\":" << (library ? L"true" : L"false") << L",\"items\":[";
    const int kMaxItems = library ? 300 : 50;
    int n = (int)items.size(); if (n > kMaxItems) n = kMaxItems;
    for (int i = 0; i < n; i++) {
        if (i) j << L",";
        auto& it = items[i];
        HICON ic = ExtractHighQualityIcon(it.path, 0, 32);
        std::wstring uri = IconToDataUri(ic, 32);
        if (ic) DestroyIcon(ic);
        j << L"{\"name\":\"" << JsonEscape(GetFileNameFromPath(it.path))
          << L"\",\"label\":\"" << JsonEscape(it.displayName.empty() ? MakeGameNameFromPath(it.path) : it.displayName)
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
    Utf8Out f(tmp.c_str(), std::ios::trunc);
    if (!f.is_open()) return false;
    f << L"{\n  \"version\": 6,\n  \"exportedAt\": " << (unsigned long)time(nullptr) << L",\n";
    f << L"  \"games\": [\n";
    for (size_t i = 0; i < g_games.size(); i++) {
        auto& g = g_games[i];
        f << L"    {\n";
        f << L"      \"name\": \"" << JsonEscape(g.name) << L"\",\n";
        f << L"      \"exePath\": \"" << JsonEscape(g.exePath) << L"\",\n";
        f << L"      \"color\": " << (unsigned long)g.color << L",\n";
        f << L"      \"iconPath\": \"" << JsonEscape(g.iconPath) << L"\",\n";
        f << L"      \"iconIndex\": " << g.iconIndex << L",\n";
        f << L"      \"runAsAdmin\": " << (g.runAsAdmin ? L"true" : L"false") << L",\n";
        f << L"      \"launchArgs\": \"" << JsonEscape(g.launchArgs) << L"\",\n";
        f << L"      \"showInRadial\": " << (g.showInRadial ? L"true" : L"false") << L",\n";
        f << L"      \"favorite\": " << (g.favorite ? L"true" : L"false") << L",\n";
        f << L"      \"totalPlaySeconds\": " << g.totalPlaySeconds << L",\n";
        f << L"      \"lastPlayedUnix\": " << (unsigned long)g.lastPlayedUnix << L",\n";
        f << L"      \"playCount\": " << g.playCount << L",\n";
        {
            // سجل جلسات اللعب (آخر 1000 جلسة): أزواج [بداية، مدة بالثواني] متتالية
            const auto sessions = LoadPlaySessions(g.exePath);
            f << L"      \"sessions\": [";
            size_t cnt = 0;
            for (auto& se : sessions) {
                if (cnt >= 1000) break;
                if (cnt) f << L",";
                f << (unsigned long)se.startUnix << L"," << (unsigned long)se.durationSec;
                cnt++;
            }
            f << L"],\n";
        }
        f << L"      \"radialBgPath\": \"" << JsonEscape(g.radialBgPath) << L"\",\n";
        f << L"      \"hideOriginalIcon\": " << (g.hideOriginalIcon ? L"true" : L"false") << L",\n";
        f << L"      \"quickSlot\": " << g.quickSlot << L",\n";
        f << L"      \"gameLanguage\": " << g.gameLanguage << L",\n";
        f << L"      \"processPriority\": " << (int)g.processPriority << L",\n";
        f << L"      \"applyAffinity\": " << (g.applyAffinity ? L"true" : L"false") << L",\n";
        f << L"      \"affinityMask\": " << g.affinityMask << L",\n";
        f << L"      \"performanceMonitor\": " << (g.performanceMonitor ? L"true" : L"false") << L",\n";
        f << L"      \"performanceCpuTemp\": " << (g.performanceCpuTemp ? L"true" : L"false") << L",\n";
        f << L"      \"boostFps\": " << (g.boostFps ? L"true" : L"false") << L",\n";
        f << L"      \"boostDisableCore0\": " << (g.boostDisableCore0 ? L"true" : L"false") << L",\n";
        f << L"      \"boostHighPriority\": " << (g.boostHighPriority ? L"true" : L"false") << L",\n";
        f << L"      \"boostStopStats\": " << (g.boostStopStats ? L"true" : L"false") << L",\n";
        f << L"      \"boostTimerResolution\": " << (g.boostTimerResolution ? L"true" : L"false") << L",\n";
        f << L"      \"boostSystemResponsiveness\": " << (g.boostSystemResponsiveness ? L"true" : L"false") << L",\n";
        f << L"      \"boostMmcss\": " << (g.boostMmcss ? L"true" : L"false") << L",\n";
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
    if (!f.good()) { DeleteFileW(tmp.c_str()); return false; }     // فشلت الكتابة: لا نترك ملفاً ناقصاً
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
    Utf8In f(path.c_str());
    if (!f.is_open()) return false;
    std::wstringstream ss;
    ss << f.rdbuf();
    std::wstring content = ss.str();
    if (content.find(L"\"games\"") == std::wstring::npos) return false;

    std::vector<GameEntry> imported;
    std::vector<std::vector<long long>> importedSessions;      // بنفس ترتيب imported
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
        g.runAsAdmin = JsonGetBool(obj, L"runAsAdmin", false);
        g.launchArgs = JsonGetString(obj, L"launchArgs");
        g.showInRadial = JsonGetBool(obj, L"showInRadial", true);
        g.favorite = JsonGetBool(obj, L"favorite", false);
        g.totalPlaySeconds = (unsigned long long)JsonGetLongLong(obj, L"totalPlaySeconds", 0);
        g.lastPlayedUnix = (DWORD)JsonGetLongLong(obj, L"lastPlayedUnix", 0);
        g.playCount = (unsigned int)JsonGetLongLong(obj, L"playCount", 0);
        g.radialBgPath = JsonGetString(obj, L"radialBgPath");
        g.hideOriginalIcon = JsonGetBool(obj, L"hideOriginalIcon", false);
        g.quickSlot = (int)JsonGetNumber(obj, L"quickSlot", 0);
        if (g.quickSlot < 0 || g.quickSlot > 9) g.quickSlot = 0;
        int gl = (int)JsonGetNumber(obj, L"gameLanguage", 0);
        if (gl < 0 || gl > 2) gl = 0;
        g.gameLanguage = gl;
        g.processPriority = ProcessPriorityFromIndex((int)JsonGetNumber(obj, L"processPriority", 0));
        g.applyAffinity = JsonGetBool(obj, L"applyAffinity", false);
        g.affinityMask = (unsigned long long)JsonGetLongLong(obj, L"affinityMask", 0);
        g.performanceMonitor = JsonGetBool(obj, L"performanceMonitor", false);
        g.performanceCpuTemp = JsonGetBool(obj, L"performanceCpuTemp", false);
        g.boostFps = JsonGetBool(obj, L"boostFps", false);
        g.boostDisableCore0 = JsonGetBool(obj, L"boostDisableCore0", true);
        g.boostHighPriority = JsonGetBool(obj, L"boostHighPriority", true);
        g.boostStopStats = JsonGetBool(obj, L"boostStopStats", true);
        g.boostTimerResolution = JsonGetBool(obj, L"boostTimerResolution", false);
        g.boostSystemResponsiveness = JsonGetBool(obj, L"boostSystemResponsiveness", false);
        g.boostMmcss = JsonGetBool(obj, L"boostMmcss", false);

        size_t cpos = JsonFindValue(obj, L"companions");
        if (cpos != std::wstring::npos && obj[cpos] == L'[') {
            size_t arrStart = cpos;
            {
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

        if (!g.name.empty() && !g.exePath.empty()) {
            imported.push_back(g);
            importedSessions.push_back(JsonGetInt64Array(obj, L"sessions"));
        }
        pos = objEnd + 1;
    }

    if (imported.empty()) return false;
    // نسخة من القائمة الحالية قبل الاستبدال الكامل (تُحفظ بجانب games.cfg)
    CopyFileW(ConfigPath().c_str(), (ConfigPath() + L".before-import.bak").c_str(), FALSE);
    g_games = imported;
    SaveGames(g_games);
    // استعادة سجل الجلسات (إن وُجد في النسخة؛ النسخ القديمة بلا سجل تترك السجل الحالي كما هو)
    for (size_t i = 0; i < imported.size() && i < importedSessions.size(); i++) {
        const auto& sess = importedSessions[i];
        if (sess.size() < 2) continue;
        ClearPlaySessions(imported[i].exePath);
        for (size_t k = 0; k + 1 < sess.size(); k += 2) {
            if (sess[k] <= 0 || sess[k + 1] <= 0) continue;
            AppendPlaySession(imported[i].exePath, imported[i].name, (DWORD)sess[k], (DWORD)sess[k + 1]);
        }
    }
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
    SaveGamesSafe(g_games); PushStateToJs();
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
    SaveGamesSafe(g_games); PushStateToJs();
}

static void RemoveCompanionAt(int gameIdx, int compIdx) {
    if (gameIdx < 0 || gameIdx >= (int)g_games.size()) return;
    auto& comps = g_games[gameIdx].companions;
    if (compIdx < 0 || compIdx >= (int)comps.size()) return;
    comps.erase(comps.begin() + compIdx);
    SaveGamesSafe(g_games); PushStateToJs();
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

// ═════════════════════════════════════════════════════════════
// فحص المجلدات + استيراد الألعاب المثبّتة من المتاجر
// ═════════════════════════════════════════════════════════════
static bool BrowseForFolder(std::wstring& outPath, const wchar_t* title) {
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(FileOpenDialog), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg));
    if (FAILED(hr) || !dlg) return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dlg->SetTitle(title);
    bool ok = false;
    if (SUCCEEDED(dlg->Show(g_hwnd))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item) {
            PWSTR p = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                outPath = p;
                CoTaskMemFree(p);
                ok = true;
            }
            item->Release();
        }
    }
    dlg->Release();
    return ok;
}

static bool DirectoryExists(const std::wstring& p) {
    const DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

// يرجع true إذا كان المجلد "مكتبة" فيها ألعاب كثيرة (مثل steamapps\common أو Epic Games):
// نأخذ أفضل ملف تشغيل لكل مجلد فرعي (باسم المجلد) بدل أن تستهلك أول لعبتين كل النتائج.
// وإلا (مجلد لعبة واحدة) نبحث فيه بالطريقة المعتادة.
static bool CollectGameCandidates(const std::wstring& rootIn, std::vector<ExeScanResult>& out,
                                  ULONGLONG deadlineTick = 0, bool forceLibrary = false) {
    out.clear();
    std::wstring root = rootIn;
    while (root.size() > 3 && (root.back() == L'\\' || root.back() == L'/')) root.pop_back();

    // مجلد Steam أو steamapps: ندخل مباشرة إلى steamapps\common
    if (DirectoryExists(root + L"\\steamapps\\common")) root += L"\\steamapps\\common";
    else if (_wcsicmp(GetFileNameFromPath(root).c_str(), L"steamapps") == 0 && DirectoryExists(root + L"\\common"))
        root += L"\\common";

    const ULONGLONG limitTick = deadlineTick ? deadlineTick : (GetTickCount64() + 8000);
    std::vector<std::wstring> children;
    bool hasDirectExe = false;
    {
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW((root + L"\\*").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // روابط/تقاطعات: نتجنّب الحلقات
                    children.push_back(fd.cFileName);
                } else {
                    const std::wstring n = fd.cFileName;
                    if (n.size() >= 4 && _wcsicmp(n.c_str() + n.size() - 4, L".exe") == 0) hasDirectExe = true;
                }
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
    }
    std::sort(children.begin(), children.end(), [](const std::wstring& a, const std::wstring& b) {
        return _wcsicmp(a.c_str(), b.c_str()) < 0;
    });

    std::vector<ExeScanResult> perChild;
    // forceLibrary: مجلد نعرف أنه مكتبة ألعاب (مثل D:\Games) حتى لو وُجد فيه ملف تنفيذي مبعثر
    if (!hasDirectExe || forceLibrary) {
        static const wchar_t* kSkip[] = { L"steamworks shared", L"_commonredist", L"redist", L"directx",
                                          L"vcredist", L"__redist", L"windowsapps", L"$recycle.bin" };
        const size_t kMaxChildren = 400;
        for (size_t i = 0; i < children.size() && i < kMaxChildren; i++) {
            if (GetTickCount64() > limitTick) break;          // سقف زمني حتى لا تتجمّد اللوحة على قرص بطيء
            bool skip = false;
            for (auto s : kSkip) if (_wcsicmp(children[i].c_str(), s) == 0) { skip = true; break; }
            if (skip) continue;
            std::vector<ExeScanResult> exes;
            FindAllExesInFolder(root + L"\\" + children[i], exes, 4, 30);
            if (exes.empty()) continue;
            ExeScanResult best = exes[0];            // مرتّبة: الأشبه باسم المجلد ثم الأكبر
            best.displayName = children[i];
            perChild.push_back(best);
        }
    }
    if (forceLibrary) {
        // ملفات تنفيذية مباشرة داخل المكتبة (مثل أداة بجانب مجلدات الألعاب): تُضاف كألعاب مستقلة بأسماء ملفاتها
        std::vector<ExeScanResult> direct;
        FindAllExesInFolder(root, direct, 0, 20);
        for (auto& d : direct) perChild.push_back(d);      // displayName فارغ = يُسمّى من اسم الملف
        out = perChild;
        return true;
    }
    if (perChild.size() >= 2) { out = perChild; return true; }

    FindAllExesInFolder(root, out, 4, 80);
    return false;
}

static bool AnyGameUnderDir(const std::wstring& dir) {
    std::wstring prefix = dir;
    if (!prefix.empty() && prefix.back() != L'\\') prefix += L'\\';
    for (auto& g : g_games)
        if (_wcsnicmp(g.exePath.c_str(), prefix.c_str(), prefix.size()) == 0) return true;
    return false;
}

static std::vector<gimport::InstalledGame> g_importCache;   // آخر نتيجة فحص (الواجهة ترسل فهارس فقط)

// ألعاب خارج المتاجر: مجلدات شائعة على كل الأقراص الثابتة (\Games و\Game...) + ألعاب لها لانشر خاص
// (Genshin / HoYoPlay / Wuthering Waves). كل لعبة تُسمّى من اسم ملفها التنفيذي (كما يسمّيها التطبيق عند السحب).
static void FindGamesInCommonFolders(std::vector<gimport::InstalledGame>& out, ULONGLONG deadline) {
    struct Root { const wchar_t* rel; bool library; };
    static const Root kRoots[] = {
        // مكتبات ألعاب: كل مجلد فرعي = لعبة (وتُقبل ملفات تنفيذية مباشرة بجانبها)
        { L"\\Games", true }, { L"\\Game", true }, { L"\\GOG Games", true },
        { L"\\Program Files\\HoYoPlay\\games", true }, { L"\\HoYoPlay\\games", true },
        // مجلد لعبة واحدة لها لانشر خاص: أفضل ملف تشغيل داخله (وليس launcher.exe)
        { L"\\Genshin Impact", false }, { L"\\Program Files\\Genshin Impact", false },
        { L"\\Star Rail", false }, { L"\\Program Files\\Star Rail", false },
        { L"\\Wuthering Waves", false }, { L"\\Program Files\\Wuthering Waves", false },
    };
    const std::vector<gimport::InstalledGame> existing = out;   // ألعاب المتاجر: لتفادي التكرار
    auto alreadyFromStore = [&](const std::wstring& exePath) {
        for (auto& e : existing) {
            if (e.installDir.empty()) continue;
            const std::wstring pre = e.installDir + L"\\";
            if (_wcsnicmp(exePath.c_str(), pre.c_str(), pre.size()) == 0) return true;
        }
        return false;
    };
    auto exists = [&](const std::wstring& exePath) {
        for (auto& e : out) if (_wcsicmp(e.launchPath.c_str(), exePath.c_str()) == 0) return true;
        return false;
    };
    auto parentDir = [](const std::wstring& p) {
        const size_t k = p.find_last_of(L"\\/");
        return k == std::wstring::npos ? p : p.substr(0, k);
    };
    auto add = [&](const std::wstring& exePath, const std::wstring& dir) {
        if (alreadyFromStore(exePath) || exists(exePath)) return;
        gimport::InstalledGame g;
        g.source = L"Games";
        g.name = MakeGameNameFromPath(exePath);
        g.launchPath = exePath;
        g.installDir = dir;
        g.iconExe = exePath;
        out.push_back(g);
    };
    const DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(drives & (1u << i))) continue;
        const std::wstring drive = std::wstring(1, (wchar_t)(L'A' + i)) + L":";
        if (GetDriveTypeW((drive + L"\\").c_str()) != DRIVE_FIXED) continue;
        for (const Root& r : kRoots) {
            if (GetTickCount64() > deadline) return;
            const std::wstring root = drive + r.rel;
            if (!DirectoryExists(root)) continue;
            std::vector<ExeScanResult> exes;
            const bool lib = CollectGameCandidates(root, exes, deadline, r.library);
            if (exes.empty()) continue;
            if (lib) {
                for (auto& e : exes) {
                    const std::wstring dir = e.displayName.empty() ? parentDir(e.path) : (root + L"\\" + e.displayName);
                    if (DirectoryExists(dir + L"\\steamapps")) continue;      // مكتبة Steam: تأتي من قسم Steam
                    add(e.path, dir);
                }
            } else {
                add(exes[0].path, root);
            }
        }
    }
}

static void ScanInstalledGamesFlow() {
    const bool en = (g_lang == Lang::EN);
    PushStatus(en ? L"Searching for installed games..." : L"جاري البحث عن الألعاب المثبّتة...", L"info");
    gimport::FindInstalledGames(g_importCache);
    // ثم ألعاب المجلدات الشائعة (\Games وألعاب اللانشرات الخاصة) — بسقف زمني 12 ثانية
    const size_t storeCount = g_importCache.size();
    FindGamesInCommonFolders(g_importCache, GetTickCount64() + 12000);
    std::sort(g_importCache.begin() + storeCount, g_importCache.end(),
        [](const gimport::InstalledGame& a, const gimport::InstalledGame& b) {
            return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
        });
    if (g_importCache.empty()) {
        PushStatus(en
            ? L"No installed games found (Steam / Epic / GOG / Ubisoft / Games folders). Use \"Scan folder\" or drag your games."
            : L"ما لقيت ألعاباً مثبّتة (Steam أو Epic أو GOG أو Ubisoft أو مجلدات Games). استخدم \"فحص مجلد\" أو اسحب الألعاب.", L"warn");
        return;
    }
    if (!g_webview) return;
    std::wstringstream j;
    j << L"{\"type\":\"installedGames\",\"items\":[";
    const size_t kMax = 600;
    for (size_t i = 0; i < g_importCache.size() && i < kMax; i++) {
        const auto& g = g_importCache[i];
        const bool already = GameExistsWithPath(g.launchPath) ||
                             (!g.installDir.empty() && AnyGameUnderDir(g.installDir));
        std::wstring icon;
        if (!g.iconExe.empty()) {
            HICON ic = ExtractHighQualityIcon(g.iconExe, 0, 32);
            icon = IconToDataUri(ic, 32);
            if (ic) DestroyIcon(ic);
        }
        if (i) j << L",";
        j << L"{\"i\":" << i
          << L",\"source\":\"" << JsonEscape(g.source)
          << L"\",\"name\":\"" << JsonEscape(g.name)
          << L"\",\"path\":\"" << JsonEscape(g.installDir.empty() ? g.launchPath : g.installDir)
          << L"\",\"icon\":\"" << icon
          << L"\",\"added\":" << (already ? L"true" : L"false") << L"}";
    }
    j << L"]}";
    g_webview->PostWebMessageAsString(j.str().c_str());
}

static void AddImportedGames(const std::vector<long>& indices) {
    int added = 0;
    for (long idx : indices) {
        if (idx < 0 || idx >= (long)g_importCache.size()) continue;
        const auto& it = g_importCache[(size_t)idx];
        if (GameExistsWithPath(it.launchPath)) continue;
        GameEntry g;
        g.name = it.name;
        g.exePath = it.launchPath;
        std::wstring iconExe = it.iconExe;
        if (iconExe.empty() && !it.installDir.empty() && IsUriLike(it.launchPath)) {
            std::vector<ExeScanResult> exes;
            FindAllExesInFolder(it.installDir, exes, 4, 40);       // للأيقونة فقط
            if (!exes.empty()) iconExe = exes[0].path;
        }
        // ملف تشغيل مباشر: أيقونته من الملف نفسه بدون تخزين مسار أيقونة مخصص
        g.iconPath = (_wcsicmp(iconExe.c_str(), it.launchPath.c_str()) == 0) ? std::wstring() : iconExe;
        g.iconIndex = 0;
        g.color = PickColorForIndex((int)g_games.size());
        g_games.push_back(g);
        added++;
    }
    if (added > 0) {
        SaveGamesSafe(g_games);
        PushStateToJs();
        PushStatus(g_lang == Lang::EN
            ? (L"Added " + std::to_wstring(added) + L" game(s).")
            : (L"أُضيفت " + std::to_wstring(added) + L" لعبة."), L"info");
    }
}

// يعرض نتيجة فحص مجلد (من السحب أو من زر "فحص مجلد"). يرجع عدد الملفات الجديدة.
static int ShowFolderScanResults(const std::wstring& folder) {
    std::vector<ExeScanResult> allExes;
    const bool library = CollectGameCandidates(folder, allExes);
    std::vector<ExeScanResult> newExes;
    for (auto& e : allExes) if (!GameExistsWithPath(e.path)) newExes.push_back(e);
    if (newExes.empty()) return 0;
    if (newExes.size() == 1 && !library) PushSuggestExe(GetFileNameFromPath(folder), newExes[0].path);
    else PushSuggestExeList(GetFileNameFromPath(folder), newExes, library);
    return (int)newExes.size();
}

static void ScanFolderFlow() {
    const bool en = (g_lang == Lang::EN);
    std::wstring folder;
    if (!BrowseForFolder(folder, en ? L"Choose a folder to scan for games" : L"اختر مجلداً لفحصه بحثاً عن الألعاب")) return;
    PushStatus(en ? L"Scanning the folder..." : L"جاري فحص المجلد...", L"info");
    if (ShowFolderScanResults(folder) == 0)
        PushStatus(en ? L"No new games found in this folder." : L"ما لقيت ألعاب جديدة في هذا المجلد.", L"warn");
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
    SaveGamesSafe(g_games); PushStateToJs();
}

static void AddCompanionFlow(int gameIdx) {
    if (gameIdx < 0 || gameIdx >= (int)g_games.size()) return;
    std::wstring path;
    if (!BrowseForExe(path, g_lang == Lang::EN ? L"Choose the companion program (exe/lnk/url)" : L"اختر البرنامج المرافق (exe/lnk/url)")) return;
    ResolvedDrop rd = ResolveDroppedPath(path);
    if (!rd.valid) { PushStatus(g_lang == Lang::EN ? L"Couldn't understand that file." : L"ما قدرت أفهم هذا الملف.", L"warn"); return; }
    CompanionEntry ce; ce.path = rd.launchPath;
    g_games[gameIdx].companions.push_back(ce);
    SaveGamesSafe(g_games); PushStateToJs();
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
static void NotifyLauncherOverlayChanged() {
    HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
    if (h) PostMessageW(h, WM_APP_RELOAD_OVERLAY, 0, 0);
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
            if (ShowFolderScanResults(p) == 0) { duplicates++; continue; }
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
        SaveGamesSafe(g_games); PushStateToJs();
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

// ✅ تحويل hex string ↔ COLORREF
static COLORREF HexToColorRef(const std::wstring& hex) {
    if (hex.size() < 6) return RGB(124, 58, 237);
    wchar_t r[3] = { hex[0], hex[1], 0 };
    wchar_t g[3] = { hex[2], hex[3], 0 };
    wchar_t b[3] = { hex[4], hex[5], 0 };
    return RGB(
        (BYTE)wcstol(r, nullptr, 16),
        (BYTE)wcstol(g, nullptr, 16),
        (BYTE)wcstol(b, nullptr, 16)
    );
}
static std::wstring ColorRefToHex(COLORREF c) {
    wchar_t buf[8];
    wsprintfW(buf, L"%02x%02x%02x",
        GetRValue(c), GetGValue(c), GetBValue(c));
    return buf;
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
        if (s >= 0 && s <= 20) SaveRadialStyle((RadialStyle)s);
        PushStateToJs();
    }
    else if (action == L"setSharpMode") {
        SaveSharpMode(JsonGetNumber(json, L"enabled", 0) != 0);   // يُطبَّق عند إعادة تشغيل البرنامج
        PushStateToJs();
    }
    else if (action == L"setRadialNoGlow") {
        bool enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveRadialNoGlow(enabled);
        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
        if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
        PushStateToJs();
    }
    else if (action == L"setRadialTransparency") {
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
        auto names = JsonGetStringArray(json, L"names");      // أسماء لطيفة (اسم مجلد اللعبة) بنفس ترتيب المسارات
        int added = 0;
        for (size_t k = 0; k < paths.size(); k++) {
            const std::wstring& p = paths[k];
            if (p.empty() || GameExistsWithPath(p)) continue;
            ResolvedDrop rd = ResolveDroppedPath(p);
            if (!rd.valid) continue;
            AddGameFromResolved(p, rd);
            if (names.size() == paths.size()) {
                std::wstring nm = TrimString(names[k]);
                if (!nm.empty()) g_games.back().name = nm;
            }
            added++;
        }
        if (added > 0) { SaveGamesSafe(g_games); PushStateToJs(); }
    }
    else if (action == L"scanFolder") {
        ScanFolderFlow();
    }
    else if (action == L"scanInstalledGames") {
        ScanInstalledGamesFlow();
    }
    else if (action == L"addImportedGames") {
        AddImportedGames(JsonGetNumberArray(json, L"indices"));
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
        SaveGamesSafe(g_games); PushStateToJs();
    }
    else if (action == L"setAutoStart") {
        SetAutoStartEnabled(JsonGetNumber(json, L"enabled", 0) != 0);
        PushStateToJs();
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
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameLaunchArgs") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].launchArgs = JsonGetString(json, L"args");
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameShowInRadial") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].showInRadial = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameColor") {
        int idx = (int)JsonGetNumber(json, L"index");
        long c = JsonGetNumber(json, L"color", -1);
        if (idx >= 0 && idx < (int)g_games.size() && c >= 0) {
            g_games[idx].color = (COLORREF)(unsigned long)c;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"browseGameCustomColor") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            COLORREF c = g_games[idx].color;
            if (BrowseForCustomColor(c, c)) {
                g_games[idx].color = c;
                SaveGamesSafe(g_games); PushStateToJs();
            }
        }
    }
    else if (action == L"setGameFavorite") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].favorite = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameQuickSlot") {
        int idx = (int)JsonGetNumber(json, L"index");
        int slot = (int)JsonGetNumber(json, L"slot", 0);
        if (idx >= 0 && idx < (int)g_games.size()) {
            if (slot < 0 || slot > 9) slot = 0;
            g_games[idx].quickSlot = slot;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameLanguage") {
        int idx = (int)JsonGetNumber(json, L"index");
        int lang = (int)JsonGetNumber(json, L"language", 0);
        if (lang < 0 || lang > 2) lang = 0;
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].gameLanguage = lang;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    // ✅ Boost FPS — handlers جديدة
    else if (action == L"setGameBoostFps") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].boostFps = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostDisableCore0") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].boostDisableCore0 = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostHighPriority") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].boostHighPriority = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostStopStats") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].boostStopStats = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostTimerResolution") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].boostTimerResolution = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostSystemResponsiveness") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            if (!IsEngineElevated()) {
                PushStatus(g_lang == Lang::EN
                    ? L"SystemResponsiveness requires Administrator rights."
                    : L"SystemResponsiveness يحتاج صلاحيات Admin.", L"warn");
                return;
            }
            g_games[idx].boostSystemResponsiveness = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameBoostMmcss") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            if (!IsEngineElevated()) {
                PushStatus(g_lang == Lang::EN
                    ? L"MMCSS Game priority requires Administrator rights."
                    : L"MMCSS Game Priority يحتاج صلاحيات Admin.", L"warn");
                return;
            }
            g_games[idx].boostMmcss = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"resetGamePlayTime") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].totalPlaySeconds = 0;
            g_games[idx].playCount = 0;
            g_games[idx].lastPlayedUnix = 0;
            SaveGames(g_games); PushStateToJs();      // تصفير متعمّد: بدون دمج إحصائيات القرص
        }
    }
    else if (action == L"browseGameRadialBg") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            std::wstring p;
            if (BrowseForImage(p,
                    g_lang == Lang::EN ? L"Choose a radial background"
                                       : L"اختر خلفية دائرية للعبة")) {
                const bool hadBg = !g_games[idx].radialBgPath.empty();
                g_games[idx].radialBgPath = p;
                // أول صورة تُضاف: "إخفاء الأيقونة الأصلية" يكون مفعّلاً افتراضياً (يقدر المستخدم يطفيه).
                // عند استبدال صورة موجودة نترك اختياره كما هو.
                if (!hadBg) g_games[idx].hideOriginalIcon = true;
                SaveGamesSafe(g_games); PushStateToJs();
            }
        }
    }
    else if (action == L"clearGameRadialBg") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].radialBgPath.clear();
            g_games[idx].hideOriginalIcon = false;
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameHideOriginalIcon") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].hideOriginalIcon = JsonGetNumber(json, L"enabled", 0) != 0
                                             && !g_games[idx].radialBgPath.empty();
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameProcessPriority") {
        int idx = (int)JsonGetNumber(json, L"index");
        int prio = (int)JsonGetNumber(json, L"priority", 0);
        if (idx >= 0 && idx < (int)g_games.size() && prio >= 0 && prio < ProcessPriorityCount()) {
            g_games[idx].processPriority = ProcessPriorityFromIndex(prio);
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setGameAffinity") {
        int idx = (int)JsonGetNumber(json, L"index");
        long long mask = JsonGetLongLong(json, L"mask", 0);
        int enabled = (int)JsonGetNumber(json, L"enabled", 0);
        if (idx >= 0 && idx < (int)g_games.size()) {
            g_games[idx].applyAffinity = (enabled != 0);
            g_games[idx].affinityMask = (unsigned long long)mask;
            SaveGamesSafe(g_games); PushStateToJs();
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
    else if (action == L"setControllerToggleMode") {
        ControllerSettings cs = LoadControllerSettings();
        cs.toggleMode = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveControllerSettings(cs);
        NotifyLauncherControllerChanged();
        PushStateToJs();
    }
    else if (action == L"setAllowControllerDuringGame") {
        ControllerSettings cs = LoadControllerSettings();
        cs.allowControllerDuringGame = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveControllerSettings(cs);
        NotifyLauncherControllerChanged();
        PushStateToJs();
    }
    else if (action == L"setPanelGlassEffect") {
        bool enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SavePanelGlassEffect(enabled);
        ApplyGlassMode(enabled);
        PushStateToJs();
    }
    else if (action == L"setOverlayEnabled") {
        OverlaySettings os = LoadOverlaySettings();
        os.enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveOverlaySettings(os);
        NotifyLauncherOverlayChanged();
        PushStateToJs();
    }
    else if (action == L"changeOverlayHotkey") {
        UINT mods = (UINT)JsonGetNumber(json, L"modifiers", 0);
        UINT vk = (UINT)JsonGetNumber(json, L"vk", 0);
        if (vk != 0) {
            OverlaySettings os = LoadOverlaySettings();
            os.hotkeyModifiers = mods;
            os.hotkeyVk = vk;
            SaveOverlaySettings(os);
            NotifyLauncherOverlayChanged();
            PushStateToJs();
        }
    }
    else if (action == L"setOverlayShowOnGameLaunch") {
        OverlaySettings os = LoadOverlaySettings();
        os.showOnGameLaunch = JsonGetNumber(json, L"enabled", 0) != 0;
        SaveOverlaySettings(os);
        NotifyLauncherOverlayChanged();
        PushStateToJs();
    }
    else if (action == L"setOverlayOpacity") {
        std::wstring vstr = JsonGetString(json, L"value");
        float v = 0.85f;
        if (!vstr.empty()) {
            try { v = std::stof(vstr); } catch (...) { v = 0.85f; }
        }
        OverlaySettings os = LoadOverlaySettings();
        os.opacity = v;
        SaveOverlaySettings(os);
        NotifyLauncherOverlayChanged();
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
                Utf8Out f(tmp.c_str(), std::ios::trunc);
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
                Utf8Out f(tmp.c_str(), std::ios::trunc);
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
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setCompanionLaunchArgs") {
        int gi = (int)JsonGetNumber(json, L"gameIndex");
        int ci = (int)JsonGetNumber(json, L"companionIndex");
        if (gi >= 0 && gi < (int)g_games.size() && ci >= 0 && ci < (int)g_games[gi].companions.size()) {
            g_games[gi].companions[ci].launchArgs = JsonGetString(json, L"args");
            SaveGamesSafe(g_games); PushStateToJs();
        }
    }
    else if (action == L"setCompanionShowInRadial") {
        int gi = (int)JsonGetNumber(json, L"gameIndex");
        int ci = (int)JsonGetNumber(json, L"companionIndex");
        if (gi >= 0 && gi < (int)g_games.size() && ci >= 0 && ci < (int)g_games[gi].companions.size()) {
            g_games[gi].companions[ci].showInRadial = JsonGetNumber(json, L"enabled", 0) != 0;
            SaveGamesSafe(g_games); PushStateToJs();
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
    else if (action == L"setGamePerformanceMonitor") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            bool enabled = JsonGetNumber(json, L"enabled", 0) != 0;
            g_games[idx].performanceMonitor = enabled;
            if (!enabled) g_games[idx].performanceCpuTemp = false;
            SaveGamesSafe(g_games);
            PushStateToJs();
        }
    }
    else if (action == L"setPerformanceGlobalEnabled") {
        bool enabled = JsonGetNumber(json, L"enabled", 0) != 0;
        SavePerformanceGlobalEnabled(enabled);
        PushStateToJs();
    }
    else if (action == L"restartAsAdmin") {
        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
        if (h) {
            PostMessageW(h, WM_APP_RESTART_AS_ADMIN, 0, 0);
            PushStatus(g_lang == Lang::EN
                ? L"Restarting GameLauncher as Administrator..."
                : L"جاري إعادة تشغيل GameLauncher كمسؤول...", L"info");
        }
        else {
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.fMask = SEE_MASK_NOASYNC;
                sei.lpVerb = L"runas";
                sei.lpFile = exePath;
                sei.lpParameters = L"--restart-admin";
                sei.nShow = SW_SHOWNORMAL;
                if (ShellExecuteExW(&sei)) {
                    PushStatus(g_lang == Lang::EN
                        ? L"Engine launching as Administrator..."
                        : L"جاري تشغيل المحرك كمسؤول...", L"info");
                }
                else {
                    DWORD err = GetLastError();
                    if (err == ERROR_CANCELLED) {
                        PushStatus(g_lang == Lang::EN
                            ? L"UAC prompt was cancelled."
                            : L"تم إلغاء نافذة الصلاحيات.", L"warn");
                    }
                    else {
                        PushStatus(g_lang == Lang::EN
                            ? L"Failed to restart as Administrator."
                            : L"فشل إعادة التشغيل كمسؤول.", L"error");
                    }
                }
            }
        }
        }
    else if (action == L"restoreBoostDefaults") {
            HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
            if (h) {
                // نرسل ونستلم النتيجة: 0 = لا شيء لاسترجاعه، 1 = تم، 2 = فشل (غالباً يحتاج صلاحيات مسؤول)
                DWORD_PTR res = 0;
                LRESULT sent = SendMessageTimeoutW(h, WM_APP_RESTORE_BOOST_DEFAULTS, 0, 0,
                                                   SMTO_ABORTIFHUNG, 5000, &res);
                const bool en = (g_lang == Lang::EN);
                if (!sent) {
                    PushStatus(en
                        ? L"Could not reach the engine (it may be running with higher privileges) — open the panel as administrator."
                        : L"تعذّر الوصول للمحرك (قد يعمل بصلاحيات أعلى) — افتح اللوحة كمسؤول.", L"warn");
                }
                else if (res == 1) {
                    PushStatus(en ? L"System defaults restored."
                                  : L"تم استرجاع إعدادات النظام الأصلية.", L"info");
                }
                else if (res == 2) {
                    PushStatus(en
                        ? L"Restore failed — run the engine as administrator and try again."
                        : L"فشل الاسترجاع — شغّل المحرك كمسؤول ثم أعد المحاولة.", L"error");
                }
                else {
                    PushStatus(en
                        ? L"Nothing to restore — system settings are already at their defaults."
                        : L"لا توجد إعدادات معدّلة لاسترجاعها — النظام على قيمه الأصلية.", L"info");
                }
            }
            else {
                PushStatus(g_lang == Lang::EN
                    ? L"Engine not running — nothing to restore."
                    : L"المحرك متوقف — لا شيء لاسترجاعه.", L"warn");
            }
            }

    else if (action == L"getPerformanceSessions") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
            auto sessions = pm.ListSessions(g_games[idx].exePath);
            std::wstringstream j;
            j << L"{\"type\":\"perfSessions\",\"sessions\":[";
            for (size_t i = 0; i < sessions.size(); i++) {
                if (i) j << L",";
                auto& s = sessions[i];
                j << L"{\"filePath\":\"" << JsonEscape(s.filePath) << L"\""
                  << L",\"sessionStartUnix\":" << s.sessionStartUnix
                  << L",\"sessionDurationSec\":" << s.sessionDurationSec
                  << L",\"avgFps\":" << s.avgFps
                  << L",\"avgGpuTemp\":" << s.avgGpuTemp
                  << L",\"avgCpuTemp\":" << s.avgCpuTemp
                  << L",\"stutterCount\":" << s.stutterCount
                  << L"}";
            }
            j << L"]}";
            if (g_webview) g_webview->PostWebMessageAsString(j.str().c_str());
        }
    }
    else if (action == L"getPerformanceReport") {
        std::wstring file = JsonGetString(json, L"file");
        if (!file.empty()) {
            auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
            PerfBlackBox::PerformanceReport rep;
            if (pm.LoadReport(file, rep)) {
                std::wstringstream j;
                j << L"{\"type\":\"perfReport\",\"report\":{"
                    << L"\"gameName\":\"" << JsonEscape(rep.gameName) << L"\""
                    << L",\"sessionStartUnix\":" << rep.sessionStartUnix
                    << L",\"sessionDurationSec\":" << rep.sessionDurationSec
                    << L",\"avgFps\":" << rep.avgFps
                    << L",\"minFps\":" << rep.minFps
                    << L",\"maxFps\":" << rep.maxFps
                    << L",\"fps1PercentLow\":" << rep.fps1PercentLow
                    << L",\"fps01PercentLow\":" << rep.fps01PercentLow
                    << L",\"avgCpuUsage\":" << rep.avgCpuUsage
                    << L",\"maxCpuUsage\":" << rep.maxCpuUsage
                    << L",\"minCpuUsage\":" << rep.minCpuUsage
                    << L",\"avgCpuTemp\":" << rep.avgCpuTemp
                    << L",\"maxCpuTemp\":" << rep.maxCpuTemp
                    << L",\"minCpuTemp\":" << rep.minCpuTemp
                    << L",\"avgGpuUsage\":" << rep.avgGpuUsage
                    << L",\"maxGpuUsage\":" << rep.maxGpuUsage
                    << L",\"minGpuUsage\":" << rep.minGpuUsage
                    << L",\"avgGpuTemp\":" << rep.avgGpuTemp
                    << L",\"maxGpuTemp\":" << rep.maxGpuTemp
                    << L",\"minGpuTemp\":" << rep.minGpuTemp
                    << L",\"avgCpuPower\":" << rep.avgCpuPower
                    << L",\"maxCpuPower\":" << rep.maxCpuPower
                    << L",\"avgGpuPower\":" << rep.avgGpuPower
                    << L",\"maxGpuPower\":" << rep.maxGpuPower
                    << L",\"avgRamMB\":" << rep.avgRamMB
                    << L",\"maxRamMB\":" << rep.maxRamMB
                    << L",\"minRamMB\":" << rep.minRamMB
                    << L",\"avgVramMB\":" << rep.avgVramMB
                    << L",\"maxVramMB\":" << rep.maxVramMB
                    << L",\"stutterCount\":" << rep.stutterCount;

                j << L",\"samples\":[";
                const size_t MAX_SEND = 3600;
                size_t total = rep.samples.size();
                size_t step = (total > MAX_SEND) ? (total / MAX_SEND) : 1;
                bool first = true;
                for (size_t i = 0; i < total; i += step) {
                    if (!first) j << L",";
                    first = false;
                    auto& s = rep.samples[i];
                    j << L"[" << s.timestampMs;
                    wchar_t buf[32];
                    if (s.fps < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.1f", s.fps); j << buf; }
                    if (s.frameTimeMs < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.2f", s.frameTimeMs); j << buf; }
                    if (s.cpuUsage < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.1f", s.cpuUsage); j << buf; }
                    if (s.gpuUsage < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.0f", s.gpuUsage); j << buf; }
                    if (s.cpuTempC < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.0f", s.cpuTempC); j << buf; }
                    if (s.gpuTempC < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.0f", s.gpuTempC); j << buf; }
                    if (s.cpuPowerW < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.0f", s.cpuPowerW); j << buf; }
                    if (s.gpuPowerW < 0) j << L",-1";
                    else { swprintf_s(buf, 32, L",%.0f", s.gpuPowerW); j << buf; }
                    j << L"," << s.ramUsedMB << L"," << s.vramUsedMB;
                    j << L"]";
                }
                j << L"]";
                j << L"}}";
                if (g_webview) g_webview->PostWebMessageAsString(j.str().c_str());
            }
            else {
                if (g_webview) g_webview->PostWebMessageAsString(L"{\"type\":\"perfReport\",\"report\":null}");
            }
        }
    }
    else if (action == L"setRadialScale") {
        RadialScale rs;
        rs.iconSize = (int)JsonGetNumber(json, L"iconSize", 42);
        rs.hubSize = (int)JsonGetNumber(json, L"hubSize", 46);
        rs.orbitDist = (int)JsonGetNumber(json, L"orbitDist", 118);
        SaveRadialScale(rs);
        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
        if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
        PushStateToJs();
    }
    else if (action == L"resetRadialScale") {
        RadialScale rs;  // defaults
        SaveRadialScale(rs);
        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
        if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
        PushStateToJs();
    }
    else if (action == L"saveCustomTheme") {
        CustomTheme ct;
        ct.accentHex = JsonGetString(json, L"accent");
        ct.secondaryHex = JsonGetString(json, L"secondary");
        ct.bgHex = JsonGetString(json, L"bg");
        ct.cardHex = JsonGetString(json, L"card");
        ct.cardOpacity = (float)_wtof(JsonGetString(json, L"cardOpacity").c_str());
        ct.accentStrength = (float)_wtof(JsonGetString(json, L"accentStrength").c_str());
        ct.bgGlow = (float)_wtof(JsonGetString(json, L"bgGlow").c_str());
        if (ct.accentHex.empty())    ct.accentHex = L"7c3aed";
        if (ct.secondaryHex.empty()) ct.secondaryHex = L"a855f7";
        if (ct.bgHex.empty())        ct.bgHex = L"150f2c";
        if (ct.cardHex.empty())      ct.cardHex = L"1e1636";
        ct.isActive = true;
        SaveCustomTheme(ct);
        SavePanelPreset(L"custom");
        PushStateToJs();
        }
    else if (action == L"resetCustomTheme") {
            CustomTheme ct;
            ct.isActive = false;
            SaveCustomTheme(ct);
            if (LoadPanelPreset() == L"custom") SavePanelPreset(L"purple");
            PushStateToJs();
            }
    else if (action == L"browseCustomThemeColor") {
                std::wstring which = JsonGetString(json, L"which");
                CustomTheme ct = LoadCustomTheme();
                COLORREF initial = RGB(124, 58, 237);
                if (which == L"accent")    initial = HexToColorRef(ct.accentHex);
                else if (which == L"secondary") initial = HexToColorRef(ct.secondaryHex);
                else if (which == L"bg")        initial = HexToColorRef(ct.bgHex);
                else if (which == L"card")      initial = HexToColorRef(ct.cardHex);
                COLORREF picked = initial;
                if (BrowseForCustomColor(initial, picked)) {
                    std::wstring hex = ColorRefToHex(picked);
                    std::wstring resp = L"{\"type\":\"customThemeColorResult\",\"which\":\"" +
                        JsonEscape(which) + L"\",\"hex\":\"" + hex + L"\"}";
                    if (g_webview) g_webview->PostWebMessageAsString(resp.c_str());
                }
                }
    else if (action == L"saveCustomRadialStyle") {
                    CustomRadialStyle crs;
                    crs.ringCount = (int)JsonGetNumber(json, L"ringCount", 2);
                    crs.ringThickness = (int)JsonGetNumber(json, L"ringThickness", 2);
                    crs.dashed = JsonGetBool(json, L"dashed", false);
                    crs.glowLayers = (int)JsonGetNumber(json, L"glowLayers", 1);
                    crs.glowStrength = (float)_wtof(JsonGetString(json, L"glowStrength").c_str());
                    crs.isActive = true;
                    SaveCustomRadialStyle(crs);
                    // لو النمط المخصص هو المختار، أعد تحميل اللوحة
                    if (LoadRadialStyle() == RadialStyle::Custom) {
                        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
                        if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
                    }
                    PushStateToJs();
                    }
    else if (action == L"resetCustomRadialStyle") {
                        CustomRadialStyle crs;
                        crs.isActive = false;
                        SaveCustomRadialStyle(crs);
                        HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
                        if (h) PostMessageW(h, WM_APP_RELOAD_CONTROLLER, 0, 0);
                        PushStateToJs();
                        }

    else if (action == L"getPlaySessions") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            std::wstring gameName;
            auto sessions = LoadPlaySessions(g_games[idx].exePath, &gameName);
            std::wstringstream j;
            j << L"{\"type\":\"playSessions\",\"sessions\":[";
            for (size_t i = 0; i < sessions.size(); i++) {
                if (i) j << L",";
                j << L"{\"startUnix\":" << sessions[i].startUnix
                    << L",\"durationSec\":" << sessions[i].durationSec << L"}";
            }
            j << L"]}";
            if (g_webview) g_webview->PostWebMessageAsString(j.str().c_str());
        }
    }
    else if (action == L"clearPlaySessions") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            ClearPlaySessions(g_games[idx].exePath);
        }
        }



    else if (action == L"exportPerformanceCsv") {
        std::wstring file = JsonGetString(json, L"file");
        if (!file.empty()) {
            auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
            PerfBlackBox::PerformanceReport rep;
            if (pm.LoadReport(file, rep)) {
                std::wstring defaultName = rep.gameName + L"-perf.csv";
                std::wstring savePath;
                if (BrowseForJsonSave(savePath,
                        g_lang == Lang::EN ? L"Save performance CSV" : L"حفظ تقرير الأداء CSV",
                        defaultName.c_str())) {
                    if (savePath.size() < 4 ||
                        _wcsicmp(savePath.c_str() + savePath.size() - 4, L".csv") != 0) {
                        savePath += L".csv";
                    }
                    if (pm.ExportCsv(rep, savePath)) {
                        PushStatus(g_lang == Lang::EN ? L"CSV exported successfully." : L"تم تصدير CSV بنجاح.", L"info");
                    } else {
                        PushStatus(g_lang == Lang::EN ? L"Failed to export CSV." : L"فشل تصدير CSV.", L"error");
                    }
                }
            }
        }
    }
    else if (action == L"deletePerformanceSession") {
        std::wstring file = JsonGetString(json, L"file");
        int gi = (int)JsonGetNumber(json, L"gameIndex", -1);
        if (!file.empty()) {
            auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
            if (pm.DeleteSession(file)) {
                PushStatus(g_lang == Lang::EN ? L"Session deleted." : L"تم حذف الجلسة.", L"info");
                if (gi >= 0 && gi < (int)g_games.size()) {
                    auto sessions = pm.ListSessions(g_games[gi].exePath);
                    std::wstringstream j;
                    j << L"{\"type\":\"perfSessions\",\"sessions\":[";
                    for (size_t i = 0; i < sessions.size(); i++) {
                        if (i) j << L",";
                        auto& s = sessions[i];
                        j << L"{\"filePath\":\"" << JsonEscape(s.filePath) << L"\""
                          << L",\"sessionStartUnix\":" << s.sessionStartUnix
                          << L",\"sessionDurationSec\":" << s.sessionDurationSec
                          << L",\"avgFps\":" << s.avgFps
                          << L",\"avgGpuTemp\":" << s.avgGpuTemp
                          << L",\"avgCpuTemp\":" << s.avgCpuTemp
                          << L",\"stutterCount\":" << s.stutterCount
                          << L"}";
                    }
                    j << L"]}";
                    if (g_webview) g_webview->PostWebMessageAsString(j.str().c_str());
                }
            }
        }
    }
    else if (action == L"deleteAllPerformanceSessions") {
        int idx = (int)JsonGetNumber(json, L"index");
        if (idx >= 0 && idx < (int)g_games.size()) {
            auto& pm = PerfBlackBox::PerformanceMonitor::Instance();
            if (pm.DeleteAllSessions(g_games[idx].exePath)) {
                PushStatus(g_lang == Lang::EN ? L"All sessions deleted." : L"تم حذف كل الجلسات.", L"info");
            }
        }
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
#ifdef NDEBUG
                                    // في نسخة Release: بدون أدوات المطوّر (F12) — تبقى متاحة في Debug للتطوير
                                    settings->put_AreDevToolsEnabled(FALSE);
#endif
                                }
                            }
                            if (g_webview) {
                                // الواجهة صفحة محلية واحدة: أي تنقّل إلى موقع خارجي يُلغى (وروابط http/https تُفتح في المتصفح)،
                                // ولا تُفتح نوافذ جديدة. هذا يحمي الجسر بين الواجهة والتطبيق من أي صفحة غريبة.
                                EventRegistrationToken navToken;
                                g_webview->add_NavigationStarting(
                                    Callback<ICoreWebView2NavigationStartingEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                                            LPWSTR uri = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                                const bool local =
                                                    _wcsnicmp(uri, L"file:///", 8) == 0 ||
                                                    _wcsnicmp(uri, L"about:", 6) == 0 ||
                                                    _wcsnicmp(uri, L"data:", 5) == 0;
                                                if (!local) {
                                                    args->put_Cancel(TRUE);
                                                    if (_wcsnicmp(uri, L"https://", 8) == 0 || _wcsnicmp(uri, L"http://", 7) == 0)
                                                        ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
                                                }
                                                CoTaskMemFree(uri);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &navToken);
                                EventRegistrationToken newWinToken;
                                g_webview->add_NewWindowRequested(
                                    Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                                            args->put_Handled(TRUE);
                                            LPWSTR uri = nullptr;
                                            if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                                                if (_wcsnicmp(uri, L"https://", 8) == 0 || _wcsnicmp(uri, L"http://", 7) == 0)
                                                    ShellExecuteW(nullptr, L"open", uri, nullptr, nullptr, SW_SHOWNORMAL);
                                                CoTaskMemFree(uri);
                                            }
                                            return S_OK;
                                        }).Get(),
                                    &newWinToken);
                            }
                            DragAcceptFiles(hwnd, TRUE);
                            ResizeWebView();
                            g_controller->put_IsVisible(TRUE);

                            if (LoadPanelGlassEffect()) {
                                SetWebViewTransparent(true);
                            }

                            std::wstring uiPath;
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

                            bool extractOk = ExtractEmbeddedFiles(uiPath, sigStr);
                            if (!extractOk) {
                                uiPath = GetExeDir() + L"\\panel_ui.html";
                                OutputDebugStringW(L"[GameLauncher] ExtractEmbeddedFiles failed - falling back to external files.\n");
                            }
                            {
                                std::wstring url = L"file:///" + uiPath + L"?v=" + sigStr;
                                for (auto& c : url) if (c == L'\\') c = L'/';
                                if (g_webview) g_webview->Navigate(url.c_str());
                            }

                            EventRegistrationToken token;
                            if (g_webview) {
                                g_webview->add_WebMessageReceived(
                                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                        [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                            // نقبل الرسائل من صفحة الواجهة المحلية فقط
                                            LPWSTR src = nullptr;
                                            if (SUCCEEDED(args->get_Source(&src)) && src) {
                                                const bool fromLocalUi = _wcsnicmp(src, L"file:///", 8) == 0;
                                                CoTaskMemFree(src);
                                                if (!fromLocalUi) return S_OK;
                                            }
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
        if (LoadPanelGlassEffect()) {
            g_glassEffectActive = true;
            SetWindowGlass(hwnd, true);
        }
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
        if (g_glassEffectActive) {
            return 1;
        }
        HBRUSH dark = CreateSolidBrush(RGB(0x0d, 0x0b, 0x18));
        FillRect(hdc, &rc, dark); DeleteObject(dark);
        return 1;
    }
    case WM_TIMER:
        if (wp == (WPARAM)g_engineStatusTimerId) {
            if (IsIconic(hwnd)) return 0;
            bool current = IsEngineRunning();
            bool changed = (g_firstStatusCheck || current != g_lastEngineStatus);
            if (changed) {
                g_lastEngineStatus = current;
                g_firstStatusCheck = false;
                PushStateToJs();
            }
            else {
                static bool lastAdminState = false;
                bool currentAdmin = IsEngineElevated();
                bool adminChanged = (currentAdmin != lastAdminState);
                if (adminChanged) {
                    lastAdminState = currentAdmin;
                    PushStateToJs();
                }
                else {
                    RefreshSharedCaches();
                    if (!g_runningGamesCache.empty()) {
                        PushStatsUpdateToJs();
                    }
                }
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

    // بوضع الحدّة يُكبَّر الحجم الأصلي 1180×760 بمعامل الـ DPI فيبقى تخطيط الصفحة كما كان (ويُقصّ لمساحة العمل).
    // بدون الوضع المعامل = 1.0 فتبقى الأرقام الأصلية بلا أي تغيير.
    int winW = (int)std::lround(1180 * GetDpiScale());
    int winH = (int)std::lround(760 * GetDpiScale());
    if (GetDpiScale() > 1.0f) {
        RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        if (winW > wa.right - wa.left) winW = wa.right - wa.left;
        if (winH > wa.bottom - wa.top) winH = wa.bottom - wa.top;
    }
    g_hwnd = CreateWindowExW(0, CLASS_NAME, L"GameLauncher — Control Panel",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
        nullptr, nullptr, hInst, nullptr);
    if (g_hwnd) {
        // شريط العنوان الداكن ليطابق اللوحة (20 = DWMWA_USE_IMMERSIVE_DARK_MODE، ويندوز 10 2004+ و11).
        // على الإصدارات الأقدم تُتجاهل القيمة بصمت ويبقى الشريط كما هو.
        BOOL darkTitle = TRUE;
        DwmSetWindowAttribute(g_hwnd, 20, &darkTitle, sizeof(darkTitle));
    }
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