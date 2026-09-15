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

// تعريفات مسبقة (معرّفة في Launcher.cpp و Panel.cpp)
int RunEngineApp(HINSTANCE hInst);
int RunPanelApp(HINSTANCE hInst);

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR lpCmdLine, int) {
    // 1) طلب Panel صريح؟
    if (lpCmdLine && wcsstr(lpCmdLine, L"--panel")) {
        return RunPanelApp(hInst);
    }

    // 2) هل Engine شغال مسبقًا؟ لو نعم → افتح Panel
    HANDLE hEngine = OpenMutexW(SYNCHRONIZE, FALSE, SINGLE_INSTANCE_MUTEX_NAME);
    if (hEngine) {
        CloseHandle(hEngine);
        return RunPanelApp(hInst);
    }

    // 3) الافتراضي: شغّل Engine
    return RunEngineApp(hInst);
}