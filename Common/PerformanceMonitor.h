// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// PerformanceMonitor.h
// كلاس "صندوق الأداء الأسود" — يجمع بيانات الأداء أثناء اللعب.
//
// 🛡️ مبادئ السلامة:
// - لا حقن DLL
// - لا قراءة ذاكرة اللعبة
// - لا Hook على DirectX/OpenGL/Vulkan
// - لا كتابة في ملفات اللعبة
// - APIs ويندوز رسمية + ETW (DXGI) + NVML (NVIDIA)
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
#include <deque>
#include <mutex>
#include <atomic>

#include "PerformanceMetrics.h"
#include "EtwFps.h"

namespace PerfBlackBox {

// ============================================================
// ملخص جلسة (للعرض في الجدول — بدون العينات الكاملة)
// ============================================================
struct SessionMeta {
    std::wstring filePath;
    std::wstring gamePath;
    std::wstring gameName;
    DWORD        sessionStartUnix   = 0;
    DWORD        sessionDurationSec = 0;
    float        avgFps             = INVALID_METRIC;
    float        fps1PercentLow     = INVALID_METRIC;
    float        avgCpuTemp         = INVALID_METRIC;
    float        maxCpuTemp         = INVALID_METRIC;
    float        avgGpuTemp         = INVALID_METRIC;
    float        maxGpuTemp         = INVALID_METRIC;
    float        avgCpuUsage        = INVALID_METRIC;
    float        maxCpuUsage        = INVALID_METRIC;
    ULONGLONG    maxRamMB           = 0;
    int          stutterCount       = 0;
};

// ============================================================
// كلاس PerformanceMonitor — Singleton
// ============================================================
class PerformanceMonitor {
public:
    static PerformanceMonitor& Instance();

    // ---------- دورة حياة الجلسة ----------
    // gamePid: PID اللعبة (0 = لا نُفعّل ETW FPS)
    bool StartSession(const std::wstring& gamePath,
                      const std::wstring& gameName,
                      bool collectCpuTemp,
                      DWORD gamePid = 0);
    void StopSession();
    bool IsMonitoring() const;

    // تحديث PID اللعبة أثناء الجلسة (مثلاً كشف process الـ URI لاحقًا)
    void UpdateGamePid(DWORD newPid);

    std::wstring CurrentGamePath() const;
    std::wstring CurrentGameName() const;
    // ✅ آخر عينة مجمّعة (للعرض المباشر في الـ Overlay)
    PerformanceSample GetLatestSample() const;

    // ✅ هل هناك جلسة نشطة الآن؟
    bool HasLiveData() const;

    // ---------- قراءة التقارير ----------
    int CountSessions(const std::wstring& gamePath) const;
    std::vector<SessionMeta> ListSessions(const std::wstring& gamePath) const;
    bool LoadReport(const std::wstring& filePath, PerformanceReport& out) const;
    bool DeleteAllSessions(const std::wstring& gamePath) const;
    bool DeleteSession(const std::wstring& filePath) const;

    // ---------- تصدير ----------
    bool ExportCsv(const PerformanceReport& report, const std::wstring& csvPath) const;

private:
    PerformanceMonitor();
    ~PerformanceMonitor();
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    static DWORD WINAPI ThreadProc(LPVOID param);
    void SamplerLoop();

    void CaptureSample(PerformanceSample& s);
    void InitCpuBaseline();

    // NVML (NVIDIA)
    struct NvmlState;
    NvmlState* m_nvml = nullptr;
    void InitNvml();
    void ShutdownNvml();
    void CaptureGpuSample(PerformanceSample& s);

    // ✅ ETW FPS
    EtwFps* m_etwFps = nullptr;

    PerformanceReport BuildReportLocked() const;
    void ComputeStats(PerformanceReport& r) const;

    bool WriteReportLocked(const PerformanceReport& r) const;
    void TrimOldSessions(const std::wstring& gameDir) const;

    // ---- State ----
    mutable std::mutex  m_mutex;
    std::atomic<bool>   m_running;
    std::atomic<bool>   m_stopRequested;
    HANDLE              m_thread = nullptr;

    std::wstring        m_gamePath;
    std::wstring        m_gameName;
    DWORD               m_sessionStartUnix = 0;
    ULONGLONG           m_sessionStartTick = 0;
    bool                m_collectCpuTemp = false;

    std::deque<PerformanceSample> m_samples;

    // CPU baseline
    ULONGLONG           m_lastCpuIdle   = 0;
    ULONGLONG           m_lastCpuKernel = 0;
    ULONGLONG           m_lastCpuUser   = 0;
    bool                m_cpuInitialized = false;
    float               m_smoothedCpu = 0.0f;
};

} // namespace PerfBlackBox