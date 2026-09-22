// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// GameImport.h — اكتشاف الألعاب المثبّتة من متاجر الألعاب (Steam / Epic / GOG / Ubisoft Connect)
#pragma once
#include <string>
#include <vector>

namespace gimport {

struct InstalledGame {
    std::wstring source;       // "Steam" | "Epic" | "GOG" | "Ubisoft"
    std::wstring name;         // اسم اللعبة
    std::wstring launchPath;   // steam://... أو com.epicgames.launcher://... أو مسار exe
    std::wstring installDir;   // مجلد التثبيت (لمعرفة المكرر والبحث عن أيقونة)
    std::wstring iconExe;      // ملف تنفيذي معروف للأيقونة (قد يكون فارغاً: يُبحث عنه عند الإضافة)
};

// يقرأ ملفات المتاجر المحلية فقط (لا اتصال بالإنترنت ولا تسجيل دخول). أي متجر غير مثبّت يُتخطّى بصمت.
void FindInstalledGames(std::vector<InstalledGame>& out);

} // namespace gimport
