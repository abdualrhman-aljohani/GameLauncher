// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// GameImportParse.h — محللات نصية بحتة (بدون Windows) لملفات المتاجر، فتُختبر على أي نظام:
//   - Steam: libraryfolders.vdf و appmanifest_*.acf (صيغة VDF)
//   - Epic : بناء رابط التشغيل
#pragma once
#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>
#include <cstdlib>
#include <algorithm>

namespace gimport {

enum class VdfTok { Str, Open, Close };
struct VdfToken { VdfTok kind; std::wstring text; };

inline std::wstring LowerW(std::wstring s) {
    for (auto& c : s) c = (wchar_t)towlower(c);
    return s;
}

// يقسّم نص VDF إلى: نصوص مقتبسة (مع فك \\ و \") وأقواس { }، ويتجاهل التعليقات //
inline std::vector<VdfToken> VdfTokenize(const std::wstring& s) {
    std::vector<VdfToken> out;
    size_t i = 0;
    const size_t n = s.size();
    while (i < n) {
        const wchar_t c = s[i];
        if (c == L'"') {
            std::wstring t;
            i++;
            while (i < n && s[i] != L'"') {
                if (s[i] == L'\\' && i + 1 < n) {
                    const wchar_t d = s[i + 1];
                    if (d == L'\\') t += L'\\';
                    else if (d == L'"') t += L'"';
                    else if (d == L'n') t += L'\n';
                    else if (d == L't') t += L'\t';
                    else { t += L'\\'; t += d; }
                    i += 2;
                } else {
                    t += s[i++];
                }
            }
            i++;   // الاقتباس الختامي
            out.push_back({ VdfTok::Str, t });
        } else if (c == L'{') { out.push_back({ VdfTok::Open, L"" }); i++; }
        else if (c == L'}') { out.push_back({ VdfTok::Close, L"" }); i++; }
        else if (c == L'/' && i + 1 < n && s[i + 1] == L'/') { while (i < n && s[i] != L'\n') i++; }
        else i++;
    }
    return out;
}

inline bool LooksLikeWinPath(const std::wstring& v) {
    if (v.size() >= 3 && v[1] == L':' && (v[2] == L'\\' || v[2] == L'/')) return true;
    if (v.size() >= 2 && v[0] == L'\\' && v[1] == L'\\') return true;
    return false;
}
inline bool AllDigits(const std::wstring& v) {
    if (v.empty()) return false;
    for (wchar_t c : v) if (c < L'0' || c > L'9') return false;
    return true;
}
inline std::wstring TrimTrailingSlashes(std::wstring p) {
    while (p.size() > 3 && (p.back() == L'\\' || p.back() == L'/')) p.pop_back();
    return p;
}

// مسارات مكتبات Steam من libraryfolders.vdf (الصيغة الجديدة: "path" داخل كل مكتبة، والقديمة: "1" "D:\\Lib")
inline std::vector<std::wstring> ParseSteamLibraryPaths(const std::wstring& vdfText) {
    std::vector<std::wstring> out;
    auto add = [&](const std::wstring& p) {
        std::wstring t = TrimTrailingSlashes(p);
        if (t.empty()) return;
        const std::wstring lo = LowerW(t);
        for (auto& e : out) if (LowerW(e) == lo) return;
        out.push_back(t);
    };
    const auto tok = VdfTokenize(vdfText);
    for (size_t i = 0; i + 1 < tok.size(); i++) {
        if (tok[i].kind != VdfTok::Str || tok[i + 1].kind != VdfTok::Str) continue;
        const std::wstring key = LowerW(tok[i].text);
        const std::wstring& val = tok[i + 1].text;
        if (key == L"path" && !val.empty()) add(val);
        else if (AllDigits(key) && LooksLikeWinPath(val)) add(val);
        i++;   // تخطّي القيمة
    }
    return out;
}

struct SteamManifest {
    std::wstring appId;
    std::wstring name;
    std::wstring installDir;
    long stateFlags = 0;
};

// appmanifest_*.acf : الحقول على المستوى الأول داخل "AppState" فقط
inline bool ParseSteamManifest(const std::wstring& acfText, SteamManifest& out) {
    out = SteamManifest();
    const auto tok = VdfTokenize(acfText);
    int depth = 0;
    for (size_t i = 0; i < tok.size(); i++) {
        if (tok[i].kind == VdfTok::Open) { depth++; continue; }
        if (tok[i].kind == VdfTok::Close) { depth--; continue; }
        if (depth == 1 && i + 1 < tok.size() && tok[i + 1].kind == VdfTok::Str) {
            const std::wstring key = LowerW(tok[i].text);
            const std::wstring& val = tok[i + 1].text;
            if (key == L"appid" && out.appId.empty()) out.appId = val;
            else if (key == L"name" && out.name.empty()) out.name = val;
            else if (key == L"installdir" && out.installDir.empty()) out.installDir = val;
            else if (key == L"stateflags") out.stateFlags = wcstol(val.c_str(), nullptr, 10);
            i++;
        }
    }
    return !out.appId.empty() && !out.installDir.empty();
}

// ما يظهر في مكتبة Steam وليس لعبة (أدوات وبيئات تشغيل)
inline bool IsSteamNonGame(const SteamManifest& m) {
    if (m.appId == L"228980") return true;   // Steamworks Common Redistributables
    if (m.appId == L"480") return true;      // Spacewar (تطبيق تجريبي من Valve للمطوّرين)
    const std::wstring n = LowerW(m.name);
    if (n.empty()) return true;
    const wchar_t* prefixes[] = { L"proton", L"steam linux runtime", L"steamworks", L"steamvr" };
    for (auto p : prefixes) if (n.compare(0, wcslen(p), p) == 0) return true;
    return false;
}

// رابط تشغيل لعبة عبر Epic Games Launcher (يضمن تسجيل الدخول والتحديثات)
inline std::wstring MakeEpicLaunchUri(const std::wstring& ns, const std::wstring& itemId,
                                      const std::wstring& appName) {
    return L"com.epicgames.launcher://apps/" + ns + L"%3A" + itemId + L"%3A" + appName +
           L"?action=launch&silent=true";
}

inline std::wstring LastPathComponent(std::wstring p) {
    p = TrimTrailingSlashes(p);
    const size_t s = p.find_last_of(L"\\/");
    return (s == std::wstring::npos) ? p : p.substr(s + 1);
}

} // namespace gimport
