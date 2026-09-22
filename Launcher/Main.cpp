// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// Main.cpp — نقطة الدخول الموحدة للـ Engine والـ Panel
#include "../Common/Config.h"
#include <fstream>
#include <string>

// تعريفات مسبقة (معرّفة في Launcher.cpp و Panel.cpp)
int RunEngineApp(HINSTANCE hInst);
int RunPanelApp(HINSTANCE hInst);
int RunUninstallCleanup();   // معرّفة في Launcher.cpp — يستدعيها المثبّت عند الإزالة

// ✅ هل العملية الحالية admin؟
static bool IsCurrentProcessAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuth, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

// ✅ هل الـ Engine الحالي admin؟ (نقرأ من engine_admin.txt)
static bool IsEngineAdmin() {
    std::wstring adminFile = GetAppDataDir() + L"\\engine_admin.txt";
    Utf8In f(adminFile.c_str());
    if (!f.is_open()) return false;
    std::wstring line;
    if (!std::getline(f, line)) return false;
    return TrimString(line) == L"1";
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int) {
    // تنظيف قبل الإزالة (يستدعيه المثبّت): استرجاع قيم Boost + حذف التشغيل التلقائي. بدون أي نافذة، ثم خروج.
    if (lpCmdLine && wcsstr(lpCmdLine, L"--uninstall-cleanup")) {
        return RunUninstallCleanup();
    }

    // وضع الحدّة (اختياري، مطفي افتراضياً): يعلن للنظام أن البرنامج يدعم الشاشات المكبّرة فتُرسم الدائرة واللوحة بدقة
    // الشاشة الحقيقية بدل تكبيرها كصورة ضبابية. يجب أن يسبق إنشاء أي نافذة.
    if (LoadSharpMode()) {
        SetProcessDPIAware();
        SetSharpModeActive(true);
    }

    // 1) طلب Panel صريح؟
    if (lpCmdLine && wcsstr(lpCmdLine, L"--panel")) {
        return RunPanelApp(hInst);
    }

    // 2) إعادة تشغيل كمسؤول — ننتظر حتى يتحرر الـ mutex
    //    (النسخة القديمة من Engine قد تكون ما زالت تُغلق نفسها)
    if (lpCmdLine && wcsstr(lpCmdLine, L"--restart-admin")) {
        for (int i = 0; i < 50; i++) {  // 50 × 200ms = 10 ثوانٍ كحد أقصى
            HANDLE hEngine = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
            if (!hEngine) break;  // تحرر — نستطيع البدء
            CloseHandle(hEngine);
            Sleep(200);
        }
        return RunEngineApp(hInst);
    }

    // 3) هل Engine شغال مسبقًا؟ لو نعم → افتح Panel (أو أعد تشغيله admin)
    HANDLE hEngine = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
    if (hEngine) {
        CloseHandle(hEngine);

        // ✅ إذا نحن admin لكن الـ Engine ليس admin → أعِد تشغيله تلقائياً
        if (IsCurrentProcessAdmin() && !IsEngineAdmin()) {
            HWND h = FindWindowExW(HWND_MESSAGE, nullptr, HIDDEN_WINDOW_CLASS_NAME, nullptr);
            if (h) {
                PostMessageW(h, WM_CLOSE, 0, 0);
                // انتظر حتى يتحرر الـ mutex (حد أقصى 5 ثوان)
                for (int i = 0; i < 25; i++) {
                    Sleep(200);
                    HANDLE h2 = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
                    if (!h2) break;
                    CloseHandle(h2);
                }
            }
            // شغّل الـ Engine من جديد (نحن admin بالفعل، فلا يحتاج UAC)
            return RunEngineApp(hInst);
        }

        return RunPanelApp(hInst);
    }

    // 4) الافتراضي: شغّل Engine
    return RunEngineApp(hInst);
}