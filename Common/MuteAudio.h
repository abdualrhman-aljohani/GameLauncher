// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// MuteAudio.h
// تحكم بمستوى صوت التطبيقات على ويندوز عبر Core Audio API.
// لا يستخدم Hooks ولا حقن، فقط واجهات ويندوز الرسمية.
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
#include <vector>

namespace MuteAudio {

// ----------------------------- معلومات تطبيق مكتوم -----------------------------
struct MutedAppInfo {
    std::wstring exeName;     // "brave.exe"
    std::wstring displayName; // "brave"
};

// 0 = غير معروف (لا يوجد session صوتي)
// 1 = غير مكتوم
// 2 = مكتوم
int GetMuteStateByPid(DWORD pid);
bool SetMuteByPid(DWORD pid, bool mute);

// يبحث عن أي عملية تحمل اسم ملف exe (مثل "Game.exe") ويضبط كتمها.
bool SetMuteByExeName(const std::wstring& exeName, bool mute);
int  GetMuteStateByExeName(const std::wstring& exeName);

// PID للعملية التي تملك النافذة الأمامية (Foreground). 0 إن لم توجد.
DWORD GetForegroundProcessId();
DWORD GetProcessIdFromWindow(HWND hwnd);

// أول نافذة علوية مرئية تخص الـ PID المحدد.
HWND FindMainWindowForProcess(DWORD pid);

// ----------------------------- قائمة التطبيقات المكتومة -----------------------------
// تُعيد قائمة بكل التطبيقات التي عندها جلسة صوت مكتومة حاليًا.
// كل تطبيق يظهر مرة واحدة فقط (حتى لو عنده أكثر من عملية).
std::vector<MutedAppInfo> GetMutedApps();

// إلغاء كتم كل التطبيقات (يرجع true لو تم تغيير شيء على الأقل).
bool UnmuteAll();

} // namespace MuteAudio