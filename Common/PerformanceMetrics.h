// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// PerformanceMetrics.h
// تعريفات بنية "صندوق الأداء الأسود" (Performance Black Box).
// هذا الملف يحتوي على structs وثوابت فقط — لا منطق.
// مبدأ السلامة: لا حقن، لا Hooks، لا Memory Reading، لا كتابة في ملفات اللعبة.
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

namespace PerfBlackBox {

// ============================================================
// ثوابت عامة
// ============================================================
constexpr ULONGLONG SAMPLE_INTERVAL_MS       = 1000;          // عينة كل ثانية (لا كل إطار)
constexpr ULONGLONG BATCH_WRITE_INTERVAL_MS  = 60000;         // كتابة snapshot كل دقيقة
constexpr int       MAX_SAMPLES_PER_SESSION  = 3600 * 4;      // 4 ساعات حد أقصى
constexpr int       MAX_SESSIONS_PER_GAME    = 30;            // حد أقصى جلسات محفوظة
constexpr ULONGLONG MIN_SESSION_DURATION_SEC = 30;            // لا نحفظ جلسة أقصر من 30ث
constexpr ULONGLONG MAX_SESSION_FILE_BYTES   = 5ULL * 1024 * 1024;  // 5 MB حد أقصى للملف

// قيمة تعني "هذه القيمة غير متوفرة" (مثلاً: حرارة CPU بدون Admin)
constexpr float INVALID_METRIC = -1.0f;

// ============================================================
// نقطة قياس واحدة (Sample) — تُسجَّل كل ثانية
// ============================================================
struct PerformanceSample {
    ULONGLONG timestampMs = 0;                 // منذ بداية الجلسة
    float     fps         = INVALID_METRIC;    // FPS الفورية
    float     frameTimeMs = INVALID_METRIC;    // زمن الإطار الأخير (ms)
    float     cpuUsage    = INVALID_METRIC;    // % استهلاك المعالج الكلي
    float     gpuUsage    = INVALID_METRIC;    // % استهلاك كرت الشاشة
    float     cpuTempC    = INVALID_METRIC;    // °C حرارة المعالج (اختياري - Admin)
    float     gpuTempC    = INVALID_METRIC;    // °C حرارة الكرت (NVML - آمن)
    float     cpuPowerW   = INVALID_METRIC;    // W استهلاك المعالج (اختياري)
    float     gpuPowerW   = INVALID_METRIC;    // W استهلاك الكرت (NVML)
    ULONGLONG ramUsedMB   = 0;                 // RAM مستخدمة
    ULONGLONG vramUsedMB  = 0;                 // VRAM مستخدمة
};

// ============================================================
// ملخص الجلسة الكامل (Report) — يُحسب عند انتهاء اللعبة
// ============================================================
struct PerformanceReport {
    // ---------- بيانات أساسية ----------
    std::wstring gamePath;
    std::wstring gameName;
    DWORD        sessionStartUnix   = 0;
    DWORD        sessionDurationSec = 0;

    // ---------- FPS ----------
    float avgFps          = INVALID_METRIC;
    float minFps          = INVALID_METRIC;
    float maxFps          = INVALID_METRIC;
    float fps1PercentLow  = INVALID_METRIC;   // متوسط أبطأ 1% من الإطارات
    float fps01PercentLow = INVALID_METRIC;   // متوسط أبطأ 0.1% من الإطارات

    // ---------- CPU Usage ----------
    float avgCpuUsage = INVALID_METRIC;
    float maxCpuUsage = INVALID_METRIC;
    float minCpuUsage = INVALID_METRIC;

    // ---------- CPU Temp (قد تكون -1 لو مو مفعّلة) ----------
    float avgCpuTemp = INVALID_METRIC;
    float maxCpuTemp = INVALID_METRIC;
    float minCpuTemp = INVALID_METRIC;

    // ---------- GPU Usage ----------
    float avgGpuUsage = INVALID_METRIC;
    float maxGpuUsage = INVALID_METRIC;
    float minGpuUsage = INVALID_METRIC;

    // ---------- GPU Temp ----------
    float avgGpuTemp = INVALID_METRIC;
    float maxGpuTemp = INVALID_METRIC;
    float minGpuTemp = INVALID_METRIC;

    // ---------- Power (استهلاك الطاقة) ----------
    float avgCpuPower = INVALID_METRIC;
    float maxCpuPower = INVALID_METRIC;
    float avgGpuPower = INVALID_METRIC;
    float maxGpuPower = INVALID_METRIC;

    // ---------- RAM ----------
    ULONGLONG avgRamMB = 0;
    ULONGLONG maxRamMB = 0;
    ULONGLONG minRamMB = 0;

    // ---------- VRAM ----------
    ULONGLONG avgVramMB = 0;
    ULONGLONG maxVramMB = 0;

    // ---------- Stutters ----------
    // عدد الإطارات اللي زمنها > ضعف متوسط زمن الإطار
    int stutterCount = 0;

    // ---------- العينات الكاملة (للرسم البياني + CSV) ----------
    std::vector<PerformanceSample> samples;

    // ---------- مساعدات ----------
    bool hasFps()      const { return avgFps      != INVALID_METRIC; }
    bool hasCpuTemp()  const { return avgCpuTemp  != INVALID_METRIC; }
    bool hasGpuTemp()  const { return avgGpuTemp  != INVALID_METRIC; }
    bool hasGpuPower() const { return avgGpuPower != INVALID_METRIC; }
};

// ============================================================
// حالة محاولة تفعيل قراءة حرارة CPU
// ============================================================
enum class CpuTempStatus {
    NotRequested,    // لم يُطلب من المستخدم
    Available,       // متوفر ويعمل
    NeedsAdmin,      // يحتاج صلاحيات Admin (المستخدم لم يوافق أو لم يُطلب)
    Unsupported,     // المعالج أو النظام لا يدعم القراءة
    DriverMissing,   // PawnIO أو ما شابهه غير مثبّت
};

} // namespace PerfBlackBox