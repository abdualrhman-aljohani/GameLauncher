// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// MiniJson.h — قراءة JSON خفيفة ودقيقة (تحترم النصوص والتداخل). كانت داخل Panel.cpp؛
// نُقلت هنا (بدون تغيير في منطقها) لتُستعمل أيضاً في قراءة ملفات المتاجر (Epic) وتُختبر منفصلة.
#pragma once
#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>
#include <cstdlib>
#ifndef _MSC_VER
#define _wcstoi64 wcstoll
#endif

inline bool JsonIsWs(wchar_t c) { return c == L' ' || c == L'\t' || c == L'\n' || c == L'\r'; }

// يتخطّى قيمة JSON كاملة (نص / كائن / مصفوفة / رقم / true / false / null)
// ويرجع الموضع الذي يليها، أو npos لو النص ناقص. يحترم النصوص فلا تخدعه أقواس داخلها.
inline size_t JsonSkipValue(const std::wstring& s, size_t i) {
    const size_t npos = std::wstring::npos;
    const size_t n = s.size();
    if (i >= n) return npos;
    if (s[i] == L'"') {
        for (i++; i < n; i++) {
            if (s[i] == L'\\') i++;
            else if (s[i] == L'"') return i + 1;
        }
        return npos;
    }
    if (s[i] == L'{' || s[i] == L'[') {
        int depth = 0;
        for (; i < n; i++) {
            const wchar_t c = s[i];
            if (c == L'"') {
                size_t e = JsonSkipValue(s, i);
                if (e == npos) return npos;
                i = e - 1;
            } else if (c == L'{' || c == L'[') {
                depth++;
            } else if (c == L'}' || c == L']') {
                if (--depth == 0) return i + 1;
            }
        }
        return npos;
    }
    while (i < n && s[i] != L',' && s[i] != L'}' && s[i] != L']' && !JsonIsWs(s[i])) i++;
    return i;
}

// يبحث عن المفتاح على المستوى الأول من الكائن فقط ويرجع موضع بداية قيمته.
// لا يخدعه نص داخل قيمة (مثل قيمة اسمها "name")، ولا مفتاح بنفس الاسم داخل كائن/مصفوفة متداخلة.
inline size_t JsonFindValue(const std::wstring& json, const std::wstring& key) {
    const size_t npos = std::wstring::npos;
    const size_t n = json.size();
    size_t i = json.find(L'{');
    if (i == npos) return npos;
    i++;
    while (i < n) {
        while (i < n && (JsonIsWs(json[i]) || json[i] == L',')) i++;
        if (i >= n || json[i] != L'"') return npos;      // نهاية الكائن أو نص غير صالح
        const size_t ks = ++i;
        bool escaped = false;
        while (i < n && json[i] != L'"') {
            if (json[i] == L'\\') { escaped = true; i++; }
            i++;
        }
        if (i >= n) return npos;
        const size_t ke = i++;
        while (i < n && JsonIsWs(json[i])) i++;
        if (i >= n || json[i] != L':') return npos;
        i++;
        while (i < n && JsonIsWs(json[i])) i++;
        if (i >= n) return npos;
        if (!escaped && ke - ks == key.size() && json.compare(ks, key.size(), key) == 0) return i;
        i = JsonSkipValue(json, i);
        if (i == npos) return npos;
    }
    return npos;
}

// يفكّ نصاً يبدأ عند p (أول حرف بعد علامة الاقتباس الافتتاحية)، وينتهي عند الاقتباس الختامي أو limit.
// عند الخروج يكون p عند الاقتباس الختامي.
inline std::wstring JsonDecodeString(const std::wstring& json, size_t& p, size_t limit) {
    std::wstring out;
    while (p < limit && json[p] != L'"') {
        if (json[p] == L'\\' && p + 1 < limit) {
            p++;
            wchar_t c = json[p];
            switch (c) {
            case L'n': out += L'\n'; break;
            case L't': out += L'\t'; break;
            case L'r': out += L'\r'; break;
            case L'b': out += L'\b'; break;
            case L'f': out += L'\f'; break;
            case L'"': out += L'"'; break;
            case L'\\': out += L'\\'; break;
            case L'/': out += L'/'; break;
            case L'u':
                if (p + 4 < limit) { out += (wchar_t)wcstol(json.substr(p + 1, 4).c_str(), nullptr, 16); p += 4; }
                break;
            default: out += c;
            }
        } else out += json[p];
        p++;
    }
    return out;
}

inline std::wstring JsonGetString(const std::wstring& json, const std::wstring& key) {
    size_t p = JsonFindValue(json, key);
    if (p == std::wstring::npos || p >= json.size() || json[p] != L'"') return L"";
    p++;
    return JsonDecodeString(json, p, json.size());
}

inline long long JsonGetLongLong(const std::wstring& json, const std::wstring& key, long long def = 0) {
    size_t p = JsonFindValue(json, key);
    if (p == std::wstring::npos) return def;
    size_t start = p;
    if (p < json.size() && json[p] == L'-') p++;
    while (p < json.size() && iswdigit(json[p])) p++;
    if (p == start || (p == start + 1 && json[start] == L'-')) return def;
    return _wcstoi64(json.substr(start, p - start).c_str(), nullptr, 10);
}

inline long JsonGetNumber(const std::wstring& json, const std::wstring& key, long def = -1) {
    return (long)JsonGetLongLong(json, key, def);
}

inline bool JsonGetBool(const std::wstring& json, const std::wstring& key, bool def) {
    size_t p = JsonFindValue(json, key);
    if (p == std::wstring::npos) return def;
    if (p + 4 <= json.size() && json.compare(p, 4, L"true") == 0) return true;
    if (p + 5 <= json.size() && json.compare(p, 5, L"false") == 0) return false;
    long n = JsonGetNumber(json, key, def ? 1 : 0);
    return n != 0;
}

// موضع الـ ] الذي يغلق المصفوفة المطلوبة (يحترم النصوص: مسار مثل D:\Games\[Repack]\x.exe لا يقطع المصفوفة)
inline bool JsonArrayBounds(const std::wstring& json, const std::wstring& key, size_t& begin, size_t& end) {
    size_t p = JsonFindValue(json, key);
    if (p == std::wstring::npos || p >= json.size() || json[p] != L'[') return false;
    size_t after = JsonSkipValue(json, p);
    if (after == std::wstring::npos || after == 0) return false;
    begin = p;
    end = after - 1;     // موضع ]
    return true;
}

inline std::vector<std::wstring> JsonGetStringArray(const std::wstring& json, const std::wstring& key) {
    std::vector<std::wstring> out;
    size_t p = 0, end = 0;
    if (!JsonArrayBounds(json, key, p, end)) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (JsonIsWs(json[i]) || json[i] == L',')) i++;
        if (i >= end || json[i] != L'"') break;
        i++;
        out.push_back(JsonDecodeString(json, i, end));
        i++;
    }
    return out;
}

inline std::vector<long> JsonGetNumberArray(const std::wstring& json, const std::wstring& key) {
    std::vector<long> out;
    size_t p = 0, end = 0;
    if (!JsonArrayBounds(json, key, p, end)) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (JsonIsWs(json[i]) || json[i] == L',')) i++;
        if (i >= end) break;
        size_t start = i;
        if (json[i] == L'-') i++;
        while (i < end && iswdigit(json[i])) i++;
        if (i == start) break;
        out.push_back(wcstol(json.substr(start, i - start).c_str(), nullptr, 10));
    }
    return out;
}

// مصفوفة أرقام صحيحة كبيرة (64 بت) — مثل أوقات يونكس وثواني اللعب
inline std::vector<long long> JsonGetInt64Array(const std::wstring& json, const std::wstring& key) {
    std::vector<long long> out;
    size_t p = 0, end = 0;
    if (!JsonArrayBounds(json, key, p, end)) return out;
    size_t i = p + 1;
    while (i < end) {
        while (i < end && (JsonIsWs(json[i]) || json[i] == L',')) i++;
        if (i >= end) break;
        size_t start = i;
        if (json[i] == L'-') i++;
        while (i < end && iswdigit(json[i])) i++;
        if (i == start) break;
        out.push_back(_wcstoi64(json.substr(start, i - start).c_str(), nullptr, 10));
    }
    return out;
}
