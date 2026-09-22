// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// EtwFps.h
// قياس FPS + Frame Time عبر ETW (Event Tracing for Windows).
// يستخدم نفس طريقة Intel PresentMon — بدون حقن، بدون Hooks.
//
// 🛡️ الأمان:
// - لا حقن DLL
// - لا قراءة ذاكرة اللعبة
// - لا Hook على DirectX
// - فقط مراقبة أحداث DXGI Present من ETW (وهي مرئية للنظام)
//
// ⚠️ المتطلبات:
// - Windows 10 2004+ للعمل بدون Admin
// - ويندوز أقدم: يحتاج Admin، وإلا يفشل بشكل آمن (FPS = -1)
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
#include <atomic>
#include <mutex>

namespace PerfBlackBox {

class EtwFps {
public:
    EtwFps();
    ~EtwFps();

    EtwFps(const EtwFps&) = delete;
    EtwFps& operator=(const EtwFps&) = delete;

    // ---------- دورة الحياة ----------
    // يبدأ التقاط أحداث DXGI Present لعملية محددة.
    // يُرجع true عند نجاح بدء الجلسة.
    bool Start(DWORD targetPid);

    // يوقف الجلسة وينظّف كل الموارد.
    void Stop();

    // هل هو نشط الآن؟
    bool IsRunning() const;

    // ---------- تغيير PID الهدف (مثلاً: كشف عملية اللعبة لاحقًا) ----------
    // يعمل حتى لو كانت الجلسة نشطة.
    void UpdateTargetPid(DWORD newPid);

    // ---------- قراءة القيم ----------
    // FPS الحالي (آخر نافذة ~1 ثانية). -1 إذا لا توجد بيانات.
    float GetFps() const;

    // آخر frame time بالمللي ثانية. -1 إذا لا توجد بيانات.
    float GetLastFrameTimeMs() const;

    // إجمالي عدد الإطارات منذ بدء الجلسة.
    ULONGLONG GetTotalPresents() const;

    // ✅ Impl يجب أن يكون public لكي تصل إليه دوال ETW الحرة
    // (EventRecordCallback و ProcessThreadProc في EtwFps.cpp)
    struct Impl;

private:
    Impl* m_impl = nullptr;
};

} // namespace PerfBlackBox