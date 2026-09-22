// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// Utf8Stream.h
//
// بديلان جاهزان (Drop-in) لـ std::wifstream و std::wofstream:
//
//   Utf8In   — يقرأ الملف كاملاً ثم يفكّ ترميزه. يقبل UTF-8 (مع/بدون BOM) و UTF-16LE،
//              وإن لم يكن UTF-8 صالحاً يعامل كل بايت كحرف (Latin-1) — وهذا هو
//              سلوك الملفات القديمة التي كتبها std::wofstream بلغة "C"،
//              فلا تضيع أي إعدادات موجودة عند المستخدمين.
//   Utf8Out  — يكتب UTF-8 دائماً. المشكلة القديمة: std::wofstream بلغة "C" في MSVC
//              يتوقف عن الكتابة عند أول حرف أكبر من 0xFF (أي حرف عربي)،
//              فتُقطع games.cfg وملفات النسخ الاحتياطي والسجلات.
//
// كلاهما يحافظ على "الوضع النصي" القديم: \n تُكتب \r\n، و\r\n تُقرأ \n.
// الواجهة نفسها (is_open / close / flush / << / >> / getline / rdbuf) فلا حاجة
// لتغيير باقي الكود.
#pragma once

#include <windows.h>
#include <string>
#include <sstream>
#include <istream>
#include <ostream>
#include <streambuf>
#include <cstddef>

namespace u8io {

// ---------------------------------------------------------------- فك الترميز
inline std::wstring DecodeText(const std::string& bytes) {
    const size_t n = bytes.size();
    const unsigned char* b = (const unsigned char*)bytes.data();

    // UTF-16 LE مع BOM
    if (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) {
        std::wstring w;
        w.reserve((n - 2) / 2);
        for (size_t i = 2; i + 1 < n; i += 2)
            w.push_back((wchar_t)(b[i] | (b[i + 1] << 8)));
        return w;
    }

    size_t off = 0;
    if (n >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF) off = 3;   // UTF-8 BOM
    if (n == off) return std::wstring();

    const int len = (int)(n - off);
    const int wl = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                       (LPCCH)(b + off), len, nullptr, 0);
    if (wl > 0) {
        std::wstring w((size_t)wl, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCCH)(b + off), len, &w[0], wl);
        return w;
    }

    // ليس UTF-8 صالحاً => ملف قديم: كل بايت = حرف واحد (كما كان يقرأ wifstream)
    std::wstring w;
    w.reserve((size_t)len);
    for (size_t i = off; i < n; i++) w.push_back((wchar_t)b[i]);
    return w;
}

// \r\n => \n
inline void NormalizeNewlines(std::wstring& w) {
    size_t o = 0;
    for (size_t i = 0; i < w.size(); i++) {
        if (w[i] == L'\r' && i + 1 < w.size() && w[i + 1] == L'\n') continue;
        w[o++] = w[i];
    }
    w.resize(o);
}

// ---------------------------------------------------------------- الترميز
// يحوّل [w, w+len) إلى UTF-8 مع \n => \r\n
inline std::string EncodeText(const wchar_t* w, size_t len) {
    std::wstring t;
    t.reserve(len + len / 8 + 2);
    for (size_t i = 0; i < len; i++) {
        if (w[i] == L'\n') t.push_back(L'\r');
        t.push_back(w[i]);
    }
    if (t.empty()) return std::string();
    const int bl = WideCharToMultiByte(CP_UTF8, 0, t.data(), (int)t.size(),
                                       nullptr, 0, nullptr, nullptr);
    if (bl <= 0) return std::string();
    std::string out((size_t)bl, '\0');
    WideCharToMultiByte(CP_UTF8, 0, t.data(), (int)t.size(), &out[0], bl, nullptr, nullptr);
    return out;
}

// ---------------------------------------------------------------- القراءة
class Utf8In : public std::wistringstream {
public:
    Utf8In() {}
    explicit Utf8In(const wchar_t* path, std::ios_base::openmode = std::ios_base::in) { open(path); }
    explicit Utf8In(const std::wstring& path, std::ios_base::openmode = std::ios_base::in) { open(path.c_str()); }

    void open(const wchar_t* path) {
        clear();
        str(std::wstring());
        m_open = false;
        HANDLE h = CreateFileW(path, GENERIC_READ,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) { setstate(std::ios_base::failbit); return; }

        LARGE_INTEGER sz;
        sz.QuadPart = 0;
        if (!GetFileSizeEx(h, &sz) || sz.QuadPart < 0 || sz.QuadPart > (LONGLONG)(512LL * 1024 * 1024)) {
            CloseHandle(h);
            setstate(std::ios_base::failbit);
            return;
        }
        std::string bytes((size_t)sz.QuadPart, '\0');
        size_t got = 0;
        while (got < bytes.size()) {
            size_t left = bytes.size() - got;
            DWORD chunk = (DWORD)(left > (size_t)(16 * 1024 * 1024) ? (size_t)(16 * 1024 * 1024) : left);
            DWORD rd = 0;
            if (!ReadFile(h, &bytes[got], chunk, &rd, nullptr) || rd == 0) break;
            got += rd;
        }
        CloseHandle(h);
        bytes.resize(got);

        std::wstring w = DecodeText(bytes);
        NormalizeNewlines(w);
        str(w);
        m_open = true;
    }
    bool is_open() const { return m_open; }
    void close() { m_open = false; }

private:
    bool m_open = false;
};

// ---------------------------------------------------------------- الكتابة
class Utf8OutBuf : public std::wstreambuf {
public:
    Utf8OutBuf() {}
    ~Utf8OutBuf() { CloseFile(); }

    bool Open(const wchar_t* path, bool append) {
        CloseFile();
        m_failed = false;
        m_buf.clear();
        const DWORD access = append ? FILE_APPEND_DATA : GENERIC_WRITE;
        const DWORD disp   = append ? OPEN_ALWAYS : CREATE_ALWAYS;
        m_h = CreateFileW(path, access,
                          FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, disp, FILE_ATTRIBUTE_NORMAL, nullptr);
        return m_h != INVALID_HANDLE_VALUE;
    }
    bool IsOpen() const { return m_h != INVALID_HANDLE_VALUE; }
    bool Failed() const { return m_failed; }

    // final=true يكتب كل ما تبقى؛ وإلا نُبقي نصف الزوج البديل (surrogate) للدفعة التالية
    bool Flush(bool final) {
        if (m_h == INVALID_HANDLE_VALUE) { m_buf.clear(); return false; }
        size_t len = m_buf.size();
        if (!final && len > 0 && m_buf[len - 1] >= 0xD800 && m_buf[len - 1] <= 0xDBFF) len--;
        if (len == 0) return !m_failed;

        std::string bytes = EncodeText(m_buf.data(), len);
        m_buf.erase(0, len);
        size_t done = 0;
        while (done < bytes.size()) {
            DWORD wr = 0;
            DWORD chunk = (DWORD)(bytes.size() - done);
            if (!WriteFile(m_h, bytes.data() + done, chunk, &wr, nullptr) || wr == 0) {
                m_failed = true;
                break;
            }
            done += wr;
        }
        return !m_failed;
    }

    // يكتب كل شيء ويُجبر النظام على تثبيته في القرص (للملفات المهمة مثل games.cfg)
    bool Commit() {
        if (!Flush(true)) return false;
        if (!FlushFileBuffers(m_h)) m_failed = true;
        return !m_failed;
    }

    void CloseFile() {
        if (m_h != INVALID_HANDLE_VALUE) {
            Flush(true);
            CloseHandle(m_h);
            m_h = INVALID_HANDLE_VALUE;
        }
    }

protected:
    int_type overflow(int_type ch) override {
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            m_buf.push_back(traits_type::to_char_type(ch));
            if (m_buf.size() >= kFlushChars) Flush(false);
        }
        return m_failed ? traits_type::eof() : traits_type::not_eof(ch);
    }
    std::streamsize xsputn(const wchar_t* s, std::streamsize n) override {
        if (n <= 0) return 0;
        m_buf.append(s, (size_t)n);
        if (m_buf.size() >= kFlushChars) Flush(false);
        return m_failed ? 0 : n;
    }
    int sync() override { return Flush(false) ? 0 : -1; }

private:
    static const size_t kFlushChars = 16384;
    HANDLE m_h = INVALID_HANDLE_VALUE;
    std::wstring m_buf;
    bool m_failed = false;
};

class Utf8Out : private Utf8OutBuf, public std::wostream {
public:
    Utf8Out() : Utf8OutBuf(), std::wostream(static_cast<std::wstreambuf*>(this)) {}
    explicit Utf8Out(const wchar_t* path, std::ios_base::openmode mode = std::ios_base::out)
        : Utf8OutBuf(), std::wostream(static_cast<std::wstreambuf*>(this)) { open(path, mode); }
    explicit Utf8Out(const std::wstring& path, std::ios_base::openmode mode = std::ios_base::out)
        : Utf8OutBuf(), std::wostream(static_cast<std::wstreambuf*>(this)) { open(path.c_str(), mode); }
    ~Utf8Out() { Utf8OutBuf::CloseFile(); }

    void open(const wchar_t* path, std::ios_base::openmode mode = std::ios_base::out) {
        clear();
        if (!Utf8OutBuf::Open(path, (mode & std::ios_base::app) != 0))
            setstate(std::ios_base::failbit);
    }
    bool is_open() const { return Utf8OutBuf::IsOpen(); }
    void close() {
        Utf8OutBuf::CloseFile();
        if (Utf8OutBuf::Failed()) setstate(std::ios_base::badbit);
    }
    // كتابة كل شيء + تثبيت في القرص. يرجع true فقط إذا نجح كل شيء.
    bool commit() {
        const bool ok = Utf8OutBuf::Commit();
        if (!ok) setstate(std::ios_base::badbit);
        return ok && !fail();
    }
};

} // namespace u8io

using u8io::Utf8In;
using u8io::Utf8Out;
