// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// PerformanceMonitor.cpp
#include "PerformanceMonitor.h"
#include "Config.h"
#include "EtwFps.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cmath>

namespace PerfBlackBox {

    // ============================================================
    // NVML (NVIDIA Management Library) — Dynamic Loading
    // ============================================================
    typedef void* NvmlDevice_t;

    struct NvmlUtilization {
        unsigned int gpu;
        unsigned int memory;
    };

    struct NvmlMemory {
        unsigned long long total;
        unsigned long long free;
        unsigned long long used;
    };

    typedef int (*pfnNvmlInit_t)(void);
    typedef int (*pfnNvmlShutdown_t)(void);
    typedef int (*pfnNvmlDeviceGetHandleByIndex_t)(unsigned int, NvmlDevice_t*);
    typedef int (*pfnNvmlDeviceGetTemperature_t)(NvmlDevice_t, int, unsigned int*);
    typedef int (*pfnNvmlDeviceGetPowerUsage_t)(NvmlDevice_t, unsigned int*);
    typedef int (*pfnNvmlDeviceGetUtilizationRates_t)(NvmlDevice_t, NvmlUtilization*);
    typedef int (*pfnNvmlDeviceGetMemoryInfo_t)(NvmlDevice_t, NvmlMemory*);

    struct PerformanceMonitor::NvmlState {
        HMODULE hDll = nullptr;
        NvmlDevice_t device = nullptr;

        pfnNvmlInit_t                      nvmlInit = nullptr;
        pfnNvmlShutdown_t                  nvmlShutdown = nullptr;
        pfnNvmlDeviceGetHandleByIndex_t    nvmlDeviceGetHandleByIndex = nullptr;
        pfnNvmlDeviceGetTemperature_t      nvmlDeviceGetTemperature = nullptr;
        pfnNvmlDeviceGetPowerUsage_t       nvmlDeviceGetPowerUsage = nullptr;
        pfnNvmlDeviceGetUtilizationRates_t nvmlDeviceGetUtilizationRates = nullptr;
        pfnNvmlDeviceGetMemoryInfo_t       nvmlDeviceGetMemoryInfo = nullptr;

        bool initialized = false;
    };

    static const int NVML_SUCCESS = 0;
    static const int NVML_TEMPERATURE_GPU = 0;

    // ============================================================
    // Logging helper
    // ============================================================
    static void PerfLog(const std::wstring& msg) {
        std::wstring logPath = GetAppDataDir() + L"\\launcher.log";
        Utf8Out lf(logPath.c_str(), std::ios::app);
        if (!lf.is_open()) return;
        SYSTEMTIME st; GetLocalTime(&st);
        wchar_t ts[32];
        wsprintfW(ts, L"[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
        lf << ts << L"[PerfMonitor] " << msg << L"\n";
    }

    // ============================================================
    // Singleton
    // ============================================================
    PerformanceMonitor& PerformanceMonitor::Instance() {
        static PerformanceMonitor instance;
        return instance;
    }

    PerformanceMonitor::PerformanceMonitor()
        : m_running(false), m_stopRequested(false) {
    }

    PerformanceMonitor::~PerformanceMonitor() {
        StopSession();

        if (m_etwFps) {
            m_etwFps->Stop();
            delete m_etwFps;
            m_etwFps = nullptr;
        }

        ShutdownNvml();
    }

    // ============================================================
    // NVML Init/Shutdown
    // ============================================================
    void PerformanceMonitor::InitNvml() {
        if (m_nvml) return;

        m_nvml = new NvmlState();

        // System32 أولاً وبمسار آمن (بدل البحث بالاسم في مجلد البرنامج — خطر DLL مزيّفة)
        m_nvml->hDll = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!m_nvml->hDll) {
            m_nvml->hDll = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
        }
        if (!m_nvml->hDll) {
            m_nvml->hDll = LoadLibraryW(L"C:\\Windows\\System32\\nvml.dll");
        }
        if (!m_nvml->hDll) {
            delete m_nvml;
            m_nvml = nullptr;
            return;
        }

        m_nvml->nvmlInit = (pfnNvmlInit_t)GetProcAddress(m_nvml->hDll, "nvmlInit_v2");
        if (!m_nvml->nvmlInit)
            m_nvml->nvmlInit = (pfnNvmlInit_t)GetProcAddress(m_nvml->hDll, "nvmlInit");

        m_nvml->nvmlShutdown = (pfnNvmlShutdown_t)GetProcAddress(m_nvml->hDll, "nvmlShutdown");

        m_nvml->nvmlDeviceGetHandleByIndex =
            (pfnNvmlDeviceGetHandleByIndex_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetHandleByIndex_v2");
        if (!m_nvml->nvmlDeviceGetHandleByIndex)
            m_nvml->nvmlDeviceGetHandleByIndex =
            (pfnNvmlDeviceGetHandleByIndex_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetHandleByIndex");

        m_nvml->nvmlDeviceGetTemperature =
            (pfnNvmlDeviceGetTemperature_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetTemperature");
        m_nvml->nvmlDeviceGetPowerUsage =
            (pfnNvmlDeviceGetPowerUsage_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetPowerUsage");
        m_nvml->nvmlDeviceGetUtilizationRates =
            (pfnNvmlDeviceGetUtilizationRates_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetUtilizationRates");
        m_nvml->nvmlDeviceGetMemoryInfo =
            (pfnNvmlDeviceGetMemoryInfo_t)GetProcAddress(m_nvml->hDll, "nvmlDeviceGetMemoryInfo");

        if (!m_nvml->nvmlInit || !m_nvml->nvmlDeviceGetHandleByIndex) {
            FreeLibrary(m_nvml->hDll);
            delete m_nvml;
            m_nvml = nullptr;
            return;
        }

        if (m_nvml->nvmlInit() != NVML_SUCCESS) {
            FreeLibrary(m_nvml->hDll);
            delete m_nvml;
            m_nvml = nullptr;
            return;
        }

        if (m_nvml->nvmlDeviceGetHandleByIndex(0, &m_nvml->device) != NVML_SUCCESS) {
            if (m_nvml->nvmlShutdown) m_nvml->nvmlShutdown();
            FreeLibrary(m_nvml->hDll);
            delete m_nvml;
            m_nvml = nullptr;
            return;
        }

        m_nvml->initialized = true;
    }

    void PerformanceMonitor::ShutdownNvml() {
        if (!m_nvml) return;
        if (m_nvml->initialized && m_nvml->nvmlShutdown) {
            m_nvml->nvmlShutdown();
        }
        if (m_nvml->hDll) FreeLibrary(m_nvml->hDll);
        delete m_nvml;
        m_nvml = nullptr;
    }

    void PerformanceMonitor::CaptureGpuSample(PerformanceSample& s) {
        if (!m_nvml || !m_nvml->initialized || !m_nvml->device) return;

        if (m_nvml->nvmlDeviceGetTemperature) {
            unsigned int temp = 0;
            if (m_nvml->nvmlDeviceGetTemperature(m_nvml->device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
                s.gpuTempC = (float)temp;
            }
        }

        if (m_nvml->nvmlDeviceGetPowerUsage) {
            unsigned int powerMw = 0;
            if (m_nvml->nvmlDeviceGetPowerUsage(m_nvml->device, &powerMw) == NVML_SUCCESS) {
                s.gpuPowerW = (float)powerMw / 1000.0f;
            }
        }

        if (m_nvml->nvmlDeviceGetUtilizationRates) {
            NvmlUtilization util = {};
            if (m_nvml->nvmlDeviceGetUtilizationRates(m_nvml->device, &util) == NVML_SUCCESS) {
                s.gpuUsage = (float)util.gpu;
            }
        }

        if (m_nvml->nvmlDeviceGetMemoryInfo) {
            NvmlMemory mem = {};
            if (m_nvml->nvmlDeviceGetMemoryInfo(m_nvml->device, &mem) == NVML_SUCCESS) {
                s.vramUsedMB = mem.used / (1024ULL * 1024ULL);
            }
        }
    }

    // ============================================================
    // Helpers
    // ============================================================
    static std::wstring FormatSessionFileName(DWORD unixTime) {
        time_t t = (time_t)unixTime;
        struct tm localTm;
        localtime_s(&localTm, &t);
        wchar_t buf[64];
        swprintf_s(buf, 64, L"%04d%02d%02d_%02d%02d%02d.glperf",
            localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
            localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
        return buf;
    }

    static std::wstring EscapeField(const std::wstring& in) {
        std::wstring out;
        out.reserve(in.size() + 8);
        for (wchar_t c : in) {
            if (c == L'\\') out += L"\\\\";
            else if (c == L'\n') out += L"\\n";
            else if (c == L'\r') out += L"\\r";
            else if (c == L'=') out += L"\\e";
            else out += c;
        }
        return out;
    }

    static std::wstring UnescapeField(const std::wstring& in) {
        std::wstring out;
        out.reserve(in.size());
        for (size_t i = 0; i < in.size(); i++) {
            if (in[i] == L'\\' && i + 1 < in.size()) {
                wchar_t n = in[i + 1];
                if (n == L'\\') { out += L'\\'; i++; }
                else if (n == L'n') { out += L'\n'; i++; }
                else if (n == L'r') { out += L'\r'; i++; }
                else if (n == L'e') { out += L'='; i++; }
                else out += in[i];
            }
            else out += in[i];
        }
        return out;
    }

    static bool EnsureDirectoryRecursive(const std::wstring& dir) {
        if (dir.empty()) return false;
        DWORD attr = GetFileAttributesW(dir.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
            return true;
        size_t pos = dir.find_last_of(L"\\/");
        if (pos != std::wstring::npos && pos > 0) {
            EnsureDirectoryRecursive(dir.substr(0, pos));
        }
        CreateDirectoryW(dir.c_str(), nullptr);
        attr = GetFileAttributesW(dir.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    // ============================================================
    // CPU baseline
    // ============================================================
    void PerformanceMonitor::InitCpuBaseline() {
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            m_lastCpuIdle = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
            m_lastCpuKernel = ((ULONGLONG)kernel.dwHighDateTime) << 32 | kernel.dwLowDateTime;
            m_lastCpuUser = ((ULONGLONG)user.dwHighDateTime) << 32 | user.dwLowDateTime;
            m_cpuInitialized = true;
        }
        else {
            m_cpuInitialized = false;
        }
        m_smoothedCpu = 0.0f;
    }

    // ============================================================
    // CaptureSample
    // ============================================================
    void PerformanceMonitor::CaptureSample(PerformanceSample& s) {
        s = PerformanceSample();
        s.timestampMs = GetTickCount64() - m_sessionStartTick;

        if (m_cpuInitialized) {
            FILETIME idle, kernel, user;
            if (GetSystemTimes(&idle, &kernel, &user)) {
                ULONGLONG i = ((ULONGLONG)idle.dwHighDateTime << 32) | idle.dwLowDateTime;
                ULONGLONG k = ((ULONGLONG)kernel.dwHighDateTime << 32) | kernel.dwLowDateTime;
                ULONGLONG u = ((ULONGLONG)user.dwHighDateTime << 32) | user.dwLowDateTime;
                ULONGLONG dIdle = i - m_lastCpuIdle;
                ULONGLONG dKernel = k - m_lastCpuKernel;
                ULONGLONG dUser = u - m_lastCpuUser;
                ULONGLONG dTotal = dKernel + dUser;
                if (dTotal > 0) {
                    float busy = (float)(dTotal - dIdle) / (float)dTotal * 100.0f;
                    m_smoothedCpu = m_smoothedCpu * 0.5f + busy * 0.5f;
                    if (m_smoothedCpu < 0) m_smoothedCpu = 0;
                    if (m_smoothedCpu > 100) m_smoothedCpu = 100;
                    s.cpuUsage = m_smoothedCpu;
                }
                m_lastCpuIdle = i; m_lastCpuKernel = k; m_lastCpuUser = u;
            }
        }

        MEMORYSTATUSEX ms = { sizeof(ms) };
        if (GlobalMemoryStatusEx(&ms)) {
            ULONGLONG usedBytes = ms.ullTotalPhys - ms.ullAvailPhys;
            s.ramUsedMB = usedBytes / (1024ULL * 1024ULL);
        }

        CaptureGpuSample(s);

        if (m_etwFps && m_etwFps->IsRunning()) {
            float fps = m_etwFps->GetFps();
            float ft = m_etwFps->GetLastFrameTimeMs();
            s.fps = (fps >= 0) ? fps : INVALID_METRIC;
            s.frameTimeMs = (ft >= 0) ? ft : INVALID_METRIC;
        }
        else {
            s.fps = INVALID_METRIC;
            s.frameTimeMs = INVALID_METRIC;
        }

        s.cpuTempC = INVALID_METRIC;
        s.cpuPowerW = INVALID_METRIC;
    }

    // ============================================================
    // Sampler Loop
    // ============================================================
    DWORD WINAPI PerformanceMonitor::ThreadProc(LPVOID param) {
        auto* self = (PerformanceMonitor*)param;
        if (self) self->SamplerLoop();
        return 0;
    }

    void PerformanceMonitor::SamplerLoop() {
        ULONGLONG lastSampleTick = 0;
        while (true) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_stopRequested.load()) break;
            }
            ULONGLONG now = GetTickCount64();
            if (now - lastSampleTick >= SAMPLE_INTERVAL_MS) {
                PerformanceSample s;
                CaptureSample(s);
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    if (m_samples.size() >= (size_t)MAX_SAMPLES_PER_SESSION) {
                        m_samples.pop_front();
                    }
                    m_samples.push_back(s);
                }
                lastSampleTick = now;
            }
            Sleep(100);
        }
    }

    // ============================================================
    // StartSession / StopSession
    // ============================================================
    bool PerformanceMonitor::StartSession(const std::wstring& gamePath,
        const std::wstring& gameName,
        bool collectCpuTemp,
        DWORD gamePid) {
        // ✅ تنظيف أي جلسة سابقة معلّقة
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_running.load()) {
                    PerfLog(L"StartSession REJECTED — another session is active: '" +
                        m_gameName + L"' (" + m_gamePath + L")");
                    return false;
                }
            }

            EnsureDirectoryRecursive(PerformanceGameDir(gamePath));
            InitNvml();

            HANDLE oldThread = nullptr;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_gamePath = gamePath;
                m_gameName = gameName;
                m_collectCpuTemp = collectCpuTemp;
                m_sessionStartUnix = (DWORD)time(nullptr);
                m_sessionStartTick = GetTickCount64();
                m_samples.clear();
                m_stopRequested = false;

                InitCpuBaseline();

                m_running = true;
                m_thread = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
                if (!m_thread) {
                    m_running = false;
                    PerfLog(L"StartSession FAILED — CreateThread failed");
                    return false;
                }
                SetThreadPriority(m_thread, THREAD_PRIORITY_BELOW_NORMAL);
                oldThread = m_thread;
            }
            (void)oldThread;

            PerfLog(L"StartSession OK → game='" + gameName + L"' pid=" +
                std::to_wstring(gamePid) + L" path=" + gamePath);

            if (gamePid != 0) {
                if (!m_etwFps) m_etwFps = new EtwFps();
                m_etwFps->Start(gamePid);
            }

            return true;
    }

    void PerformanceMonitor::StopSession() {
        if (m_etwFps) {
            m_etwFps->Stop();
        }

        HANDLE threadToWait = nullptr;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running.load()) return;
            m_stopRequested = true;
            threadToWait = m_thread;
        }
        if (threadToWait) {
            WaitForSingleObject(threadToWait, 3000);
            CloseHandle(threadToWait);
        }
        PerformanceReport report;
        bool haveReport = false;
        std::wstring stoppedGame;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_thread = nullptr;
            m_running = false;

            if (!m_samples.empty()) {
                report = BuildReportLocked();
                haveReport = true;
                stoppedGame = report.gameName;
            }
            m_samples.clear();
            m_gamePath.clear();
            m_gameName.clear();
        }
        if (haveReport) {
            if (report.sessionDurationSec >= (DWORD)MIN_SESSION_DURATION_SEC) {
                WriteReportLocked(report);
                PerfLog(L"StopSession → saved report for '" + stoppedGame +
                    L"' (" + std::to_wstring(report.sessionDurationSec) + L"s)");
            }
            else {
                PerfLog(L"StopSession → skipped short session '" + stoppedGame +
                    L"' (" + std::to_wstring(report.sessionDurationSec) + L"s < " +
                    std::to_wstring(MIN_SESSION_DURATION_SEC) + L"s)");
            }
        }
    }

    bool PerformanceMonitor::IsMonitoring() const {
        return m_running.load();
    }

    void PerformanceMonitor::UpdateGamePid(DWORD newPid) {
        if (m_etwFps) m_etwFps->UpdateTargetPid(newPid);
    }

    std::wstring PerformanceMonitor::CurrentGamePath() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_gamePath;
    }

    std::wstring PerformanceMonitor::CurrentGameName() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_gameName;
    }

    // ============================================================
    // BuildReport + ComputeStats
    // ============================================================
    PerformanceReport PerformanceMonitor::BuildReportLocked() const {
        PerformanceReport r;
        r.gamePath = m_gamePath;
        r.gameName = m_gameName;
        r.sessionStartUnix = m_sessionStartUnix;
        r.sessionDurationSec = (DWORD)((GetTickCount64() - m_sessionStartTick) / 1000);
        r.samples.assign(m_samples.begin(), m_samples.end());
        ComputeStats(r);
        return r;
    }

    void PerformanceMonitor::ComputeStats(PerformanceReport& r) const {
        if (r.samples.empty()) return;

        {
            std::vector<float> vals;
            vals.reserve(r.samples.size());
            for (auto& s : r.samples) if (s.fps >= 0) vals.push_back(s.fps);
            if (!vals.empty()) {
                double sum = 0; float mn = vals[0], mx = vals[0];
                for (float v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgFps = (float)(sum / vals.size());
                r.minFps = mn;
                r.maxFps = mx;

                std::vector<float> sorted = vals;
                std::sort(sorted.begin(), sorted.end());
                size_t n1 = (std::max)((size_t)1, sorted.size() / 100);
                size_t n01 = (std::max)((size_t)1, sorted.size() / 1000);
                double s1 = 0; for (size_t i = 0; i < n1; i++) s1 += sorted[i];
                double s01 = 0; for (size_t i = 0; i < n01; i++) s01 += sorted[i];
                r.fps1PercentLow = (float)(s1 / n1);
                r.fps01PercentLow = (float)(s01 / n01);
            }
        }

        {
            std::vector<float> vals;
            for (auto& s : r.samples) if (s.cpuUsage >= 0) vals.push_back(s.cpuUsage);
            if (!vals.empty()) {
                double sum = 0; float mn = vals[0], mx = vals[0];
                for (float v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgCpuUsage = (float)(sum / vals.size());
                r.minCpuUsage = mn;
                r.maxCpuUsage = mx;
            }
        }

        {
            std::vector<float> vals;
            for (auto& s : r.samples) if (s.cpuTempC >= 0) vals.push_back(s.cpuTempC);
            if (!vals.empty()) {
                double sum = 0; float mn = vals[0], mx = vals[0];
                for (float v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgCpuTemp = (float)(sum / vals.size());
                r.minCpuTemp = mn;
                r.maxCpuTemp = mx;
            }
        }

        {
            std::vector<float> vals;
            for (auto& s : r.samples) if (s.gpuUsage >= 0) vals.push_back(s.gpuUsage);
            if (!vals.empty()) {
                double sum = 0; float mn = vals[0], mx = vals[0];
                for (float v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgGpuUsage = (float)(sum / vals.size());
                r.minGpuUsage = mn;
                r.maxGpuUsage = mx;
            }
        }

        {
            std::vector<float> vals;
            for (auto& s : r.samples) if (s.gpuTempC >= 0) vals.push_back(s.gpuTempC);
            if (!vals.empty()) {
                double sum = 0; float mn = vals[0], mx = vals[0];
                for (float v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgGpuTemp = (float)(sum / vals.size());
                r.minGpuTemp = mn;
                r.maxGpuTemp = mx;
            }
        }

        {
            std::vector<float> cpuP, gpuP;
            for (auto& s : r.samples) {
                if (s.cpuPowerW >= 0) cpuP.push_back(s.cpuPowerW);
                if (s.gpuPowerW >= 0) gpuP.push_back(s.gpuPowerW);
            }
            if (!cpuP.empty()) {
                double sum = 0; float mx = cpuP[0];
                for (float v : cpuP) { sum += v; if (v > mx) mx = v; }
                r.avgCpuPower = (float)(sum / cpuP.size());
                r.maxCpuPower = mx;
            }
            if (!gpuP.empty()) {
                double sum = 0; float mx = gpuP[0];
                for (float v : gpuP) { sum += v; if (v > mx) mx = v; }
                r.avgGpuPower = (float)(sum / gpuP.size());
                r.maxGpuPower = mx;
            }
        }

        {
            std::vector<ULONGLONG> vals;
            for (auto& s : r.samples) if (s.ramUsedMB > 0) vals.push_back(s.ramUsedMB);
            if (!vals.empty()) {
                unsigned long long sum = 0;
                ULONGLONG mn = vals[0], mx = vals[0];
                for (auto v : vals) { sum += v; if (v < mn) mn = v; if (v > mx) mx = v; }
                r.avgRamMB = sum / vals.size();
                r.minRamMB = mn;
                r.maxRamMB = mx;
            }
        }

        {
            std::vector<ULONGLONG> vals;
            for (auto& s : r.samples) if (s.vramUsedMB > 0) vals.push_back(s.vramUsedMB);
            if (!vals.empty()) {
                unsigned long long sum = 0;
                ULONGLONG mx = vals[0];
                for (auto v : vals) { sum += v; if (v > mx) mx = v; }
                r.avgVramMB = sum / vals.size();
                r.maxVramMB = mx;
            }
        }

        {
            std::vector<float> fts;
            for (auto& s : r.samples) if (s.frameTimeMs >= 0) fts.push_back(s.frameTimeMs);
            if (!fts.empty()) {
                double sum = 0;
                for (float v : fts) sum += v;
                float avg = (float)(sum / fts.size());
                int count = 0;
                for (float v : fts) if (v > avg * 2.0f) count++;
                r.stutterCount = count;
            }
        }
    }

    // ============================================================
    // Persistence
    // ============================================================
    bool PerformanceMonitor::WriteReportLocked(const PerformanceReport& r) const {
        std::wstring gameDir = PerformanceGameDir(r.gamePath);
        EnsureDirectoryRecursive(gameDir);

        std::wstring fileName = FormatSessionFileName(r.sessionStartUnix);
        std::wstring fullPath = gameDir + L"\\" + fileName;
        std::wstring tmpPath = fullPath + L".tmp";
        Utf8Out f(tmpPath.c_str(), std::ios::trunc);
        if (!f.is_open()) {
            PerfLog(L"WriteReport FAILED — cannot open: " + tmpPath);
            return false;
        }

        f << L"GLPERF-V1\n";
        f << L"gamePath=" << EscapeField(r.gamePath) << L"\n";
        f << L"gameName=" << EscapeField(r.gameName) << L"\n";
        f << L"sessionStartUnix=" << r.sessionStartUnix << L"\n";
        f << L"sessionDurationSec=" << r.sessionDurationSec << L"\n";
        f << L"avgFps=" << r.avgFps << L"\n";
        f << L"minFps=" << r.minFps << L"\n";
        f << L"maxFps=" << r.maxFps << L"\n";
        f << L"fps1PercentLow=" << r.fps1PercentLow << L"\n";
        f << L"fps01PercentLow=" << r.fps01PercentLow << L"\n";
        f << L"avgCpuUsage=" << r.avgCpuUsage << L"\n";
        f << L"maxCpuUsage=" << r.maxCpuUsage << L"\n";
        f << L"minCpuUsage=" << r.minCpuUsage << L"\n";
        f << L"avgCpuTemp=" << r.avgCpuTemp << L"\n";
        f << L"maxCpuTemp=" << r.maxCpuTemp << L"\n";
        f << L"minCpuTemp=" << r.minCpuTemp << L"\n";
        f << L"avgGpuUsage=" << r.avgGpuUsage << L"\n";
        f << L"maxGpuUsage=" << r.maxGpuUsage << L"\n";
        f << L"minGpuUsage=" << r.minGpuUsage << L"\n";
        f << L"avgGpuTemp=" << r.avgGpuTemp << L"\n";
        f << L"maxGpuTemp=" << r.maxGpuTemp << L"\n";
        f << L"minGpuTemp=" << r.minGpuTemp << L"\n";
        f << L"avgCpuPower=" << r.avgCpuPower << L"\n";
        f << L"maxCpuPower=" << r.maxCpuPower << L"\n";
        f << L"avgGpuPower=" << r.avgGpuPower << L"\n";
        f << L"maxGpuPower=" << r.maxGpuPower << L"\n";
        f << L"avgRamMB=" << r.avgRamMB << L"\n";
        f << L"maxRamMB=" << r.maxRamMB << L"\n";
        f << L"minRamMB=" << r.minRamMB << L"\n";
        f << L"avgVramMB=" << r.avgVramMB << L"\n";
        f << L"maxVramMB=" << r.maxVramMB << L"\n";
        f << L"stutterCount=" << r.stutterCount << L"\n";
        f << L"SAMPLES\n";
        f << L"timestampMs,fps,frameTimeMs,cpuUsage,gpuUsage,cpuTempC,gpuTempC,cpuPowerW,gpuPowerW,ramUsedMB,vramUsedMB\n";
        for (auto& s : r.samples) {
            f << s.timestampMs << L","
                << s.fps << L","
                << s.frameTimeMs << L","
                << s.cpuUsage << L","
                << s.gpuUsage << L","
                << s.cpuTempC << L","
                << s.gpuTempC << L","
                << s.cpuPowerW << L","
                << s.gpuPowerW << L","
                << s.ramUsedMB << L","
                << s.vramUsedMB << L"\n";
        }
        f.flush();
        f.close();

        MoveFileExW(tmpPath.c_str(), fullPath.c_str(), MOVEFILE_REPLACE_EXISTING);

        // ✅ سجّل كل كتابة تقرير لمعرفة المسار الحقيقي
        PerfLog(L"WriteReport OK game='" + r.gameName + L"' path=" + r.gamePath +
            L" | dir=" + gameDir +
            L" | dur=" + std::to_wstring(r.sessionDurationSec) + L"s");

        TrimOldSessions(gameDir);
        return true;
    }

    void PerformanceMonitor::TrimOldSessions(const std::wstring& gameDir) const {
        std::wstring pattern = gameDir + L"\\*.glperf";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return;

        std::vector<std::wstring> files;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            files.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);

        if ((int)files.size() <= MAX_SESSIONS_PER_GAME) return;
        std::sort(files.begin(), files.end());

        int toDelete = (int)files.size() - MAX_SESSIONS_PER_GAME;
        for (int i = 0; i < toDelete; i++) {
            std::wstring full = gameDir + L"\\" + files[i];
            DeleteFileW(full.c_str());
        }
    }

    // ============================================================
    // قراءة التقارير
    // ============================================================
    int PerformanceMonitor::CountSessions(const std::wstring& gamePath) const {
        std::wstring dir = PerformanceGameDir(gamePath);
        std::wstring pattern = dir + L"\\*.glperf";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return 0;
        int count = 0;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) count++;
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        return count;
    }

    std::vector<SessionMeta> PerformanceMonitor::ListSessions(const std::wstring& gamePath) const {
        std::vector<SessionMeta> result;
        std::wstring dir = PerformanceGameDir(gamePath);
        std::wstring pattern = dir + L"\\*.glperf";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return result;

        std::vector<std::wstring> files;
        do {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            files.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);

        std::sort(files.rbegin(), files.rend());

        for (auto& name : files) {
            std::wstring full = dir + L"\\" + name;
            Utf8In f(full.c_str());
            if (!f.is_open()) continue;

            SessionMeta meta;
            meta.filePath = full;
            std::wstring line;
            bool headerFound = false;
            while (std::getline(f, line)) {
                if (line == L"SAMPLES") break;
                if (line == L"GLPERF-V1") { headerFound = true; continue; }
                size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) continue;
                std::wstring key = line.substr(0, eq);
                std::wstring val = line.substr(eq + 1);
                if (key == L"gamePath") meta.gamePath = UnescapeField(val);
                else if (key == L"gameName") meta.gameName = UnescapeField(val);
                else if (key == L"sessionStartUnix") meta.sessionStartUnix = (DWORD)_wtoi(val.c_str());
                else if (key == L"sessionDurationSec") meta.sessionDurationSec = (DWORD)_wtoi(val.c_str());
                else if (key == L"avgFps") meta.avgFps = (float)_wtof(val.c_str());
                else if (key == L"fps1PercentLow") meta.fps1PercentLow = (float)_wtof(val.c_str());
                else if (key == L"avgCpuUsage") meta.avgCpuUsage = (float)_wtof(val.c_str());
                else if (key == L"maxCpuUsage") meta.maxCpuUsage = (float)_wtof(val.c_str());
                else if (key == L"avgCpuTemp") meta.avgCpuTemp = (float)_wtof(val.c_str());
                else if (key == L"maxCpuTemp") meta.maxCpuTemp = (float)_wtof(val.c_str());
                else if (key == L"avgGpuTemp") meta.avgGpuTemp = (float)_wtof(val.c_str());
                else if (key == L"maxGpuTemp") meta.maxGpuTemp = (float)_wtof(val.c_str());
                else if (key == L"maxRamMB") meta.maxRamMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"stutterCount") meta.stutterCount = _wtoi(val.c_str());
            }
            if (!headerFound) continue;

            // ✅ فلترة: الملف لازم يكون لنفس اللعبة
            if (!meta.gamePath.empty() &&
                _wcsicmp(meta.gamePath.c_str(), gamePath.c_str()) != 0) {
                PerfLog(L"ListSessions SKIP foreign file: " + full +
                    L" (file belongs to: " + meta.gamePath + L")");
                continue;
            }

            result.push_back(meta);
        }
        return result;
    }

    bool PerformanceMonitor::LoadReport(const std::wstring& filePath, PerformanceReport& out) const {
        Utf8In f(filePath.c_str());
        if (!f.is_open()) return false;

        out = PerformanceReport();
        std::wstring line;
        bool headerFound = false;
        bool inSamples = false;
        bool skippedSampleHeader = false;
        while (std::getline(f, line)) {
            if (!inSamples) {
                if (line == L"GLPERF-V1") { headerFound = true; continue; }
                if (line == L"SAMPLES") { inSamples = true; continue; }
                size_t eq = line.find(L'=');
                if (eq == std::wstring::npos) continue;
                std::wstring key = line.substr(0, eq);
                std::wstring val = line.substr(eq + 1);
                if (key == L"gamePath") out.gamePath = UnescapeField(val);
                else if (key == L"gameName") out.gameName = UnescapeField(val);
                else if (key == L"sessionStartUnix") out.sessionStartUnix = (DWORD)_wtoi(val.c_str());
                else if (key == L"sessionDurationSec") out.sessionDurationSec = (DWORD)_wtoi(val.c_str());
                else if (key == L"avgFps") out.avgFps = (float)_wtof(val.c_str());
                else if (key == L"minFps") out.minFps = (float)_wtof(val.c_str());
                else if (key == L"maxFps") out.maxFps = (float)_wtof(val.c_str());
                else if (key == L"fps1PercentLow") out.fps1PercentLow = (float)_wtof(val.c_str());
                else if (key == L"fps01PercentLow") out.fps01PercentLow = (float)_wtof(val.c_str());
                else if (key == L"avgCpuUsage") out.avgCpuUsage = (float)_wtof(val.c_str());
                else if (key == L"maxCpuUsage") out.maxCpuUsage = (float)_wtof(val.c_str());
                else if (key == L"minCpuUsage") out.minCpuUsage = (float)_wtof(val.c_str());
                else if (key == L"avgCpuTemp") out.avgCpuTemp = (float)_wtof(val.c_str());
                else if (key == L"maxCpuTemp") out.maxCpuTemp = (float)_wtof(val.c_str());
                else if (key == L"minCpuTemp") out.minCpuTemp = (float)_wtof(val.c_str());
                else if (key == L"avgGpuUsage") out.avgGpuUsage = (float)_wtof(val.c_str());
                else if (key == L"maxGpuUsage") out.maxGpuUsage = (float)_wtof(val.c_str());
                else if (key == L"minGpuUsage") out.minGpuUsage = (float)_wtof(val.c_str());
                else if (key == L"avgGpuTemp") out.avgGpuTemp = (float)_wtof(val.c_str());
                else if (key == L"maxGpuTemp") out.maxGpuTemp = (float)_wtof(val.c_str());
                else if (key == L"minGpuTemp") out.minGpuTemp = (float)_wtof(val.c_str());
                else if (key == L"avgCpuPower") out.avgCpuPower = (float)_wtof(val.c_str());
                else if (key == L"maxCpuPower") out.maxCpuPower = (float)_wtof(val.c_str());
                else if (key == L"avgGpuPower") out.avgGpuPower = (float)_wtof(val.c_str());
                else if (key == L"maxGpuPower") out.maxGpuPower = (float)_wtof(val.c_str());
                else if (key == L"avgRamMB") out.avgRamMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"maxRamMB") out.maxRamMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"minRamMB") out.minRamMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"avgVramMB") out.avgVramMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"maxVramMB") out.maxVramMB = _wcstoui64(val.c_str(), nullptr, 10);
                else if (key == L"stutterCount") out.stutterCount = _wtoi(val.c_str());
            }
            else {
                if (!skippedSampleHeader) { skippedSampleHeader = true; continue; }
                if (line.empty()) continue;
                PerformanceSample s;
                std::wstringstream ss(line);
                std::wstring tok;
                int idx = 0;
                while (std::getline(ss, tok, L',')) {
                    double v = _wtof(tok.c_str());
                    switch (idx) {
                    case 0: s.timestampMs = (ULONGLONG)v; break;
                    case 1: s.fps = (float)v; break;
                    case 2: s.frameTimeMs = (float)v; break;
                    case 3: s.cpuUsage = (float)v; break;
                    case 4: s.gpuUsage = (float)v; break;
                    case 5: s.cpuTempC = (float)v; break;
                    case 6: s.gpuTempC = (float)v; break;
                    case 7: s.cpuPowerW = (float)v; break;
                    case 8: s.gpuPowerW = (float)v; break;
                    case 9: s.ramUsedMB = (ULONGLONG)v; break;
                    case 10: s.vramUsedMB = (ULONGLONG)v; break;
                    default: break;
                    }
                    idx++;
                }
                if (idx >= 3) out.samples.push_back(s);
            }
        }
        return headerFound;
    }

    bool PerformanceMonitor::DeleteAllSessions(const std::wstring& gamePath) const {
        std::wstring dir = PerformanceGameDir(gamePath);
        std::wstring pattern = dir + L"\\*.glperf";
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return true;
        std::vector<std::wstring> toDel;
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                toDel.push_back(dir + L"\\" + fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        bool ok = true;
        for (auto& p : toDel) {
            if (!DeleteFileW(p.c_str())) ok = false;
        }
        PerfLog(L"DeleteAllSessions path=" + gamePath +
            L" | deleted=" + std::to_wstring(toDel.size()) + L" files");
        return ok;
    }

    bool PerformanceMonitor::DeleteSession(const std::wstring& filePath) const {
        bool ok = DeleteFileW(filePath.c_str()) != 0;
        PerfLog(L"DeleteSession " + filePath + L" | ok=" + std::to_wstring(ok ? 1 : 0));
        return ok;
    }

    // ============================================================
    // تصدير CSV
    // ============================================================
    bool PerformanceMonitor::ExportCsv(const PerformanceReport& report, const std::wstring& csvPath) const {
        std::wstring tmp = csvPath + L".tmp";
        Utf8Out f(tmp.c_str(), std::ios::trunc);
        if (!f.is_open()) return false;

        f << (wchar_t)0xFEFF;   // BOM ليفتح Excel الأسماء العربية بشكل صحيح
        f << L"# GameLauncher Performance Report\n";
        f << L"# Game: " << EscapeField(report.gameName) << L"\n";
        f << L"# Path: " << EscapeField(report.gamePath) << L"\n";
        f << L"# Date: " << report.sessionStartUnix << L"\n";
        f << L"# Duration (sec): " << report.sessionDurationSec << L"\n";
        f << L"# Avg FPS: " << report.avgFps << L"\n";
        f << L"# 1% Low FPS: " << report.fps1PercentLow << L"\n";
        f << L"# Avg GPU Temp: " << report.avgGpuTemp << L"\n";
        f << L"# Max GPU Temp: " << report.maxGpuTemp << L"\n";
        f << L"# Avg CPU Temp: " << report.avgCpuTemp << L"\n";
        f << L"# Max CPU Temp: " << report.maxCpuTemp << L"\n";
        f << L"#\n";
        f << L"timestampMs,fps,frameTimeMs,cpuUsage,gpuUsage,cpuTempC,gpuTempC,cpuPowerW,gpuPowerW,ramUsedMB,vramUsedMB\n";

        for (auto& s : report.samples) {
            f << s.timestampMs << L","
                << s.fps << L","
                << s.frameTimeMs << L","
                << s.cpuUsage << L","
                << s.gpuUsage << L","
                << s.cpuTempC << L","
                << s.gpuTempC << L","
                << s.cpuPowerW << L","
                << s.gpuPowerW << L","
                << s.ramUsedMB << L","
                << s.vramUsedMB << L"\n";
        }
        f.flush();
        f.close();
        return MoveFileExW(tmp.c_str(), csvPath.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    }

    // ============================================================
    // آخر عينة للعرض المباشر
    // ============================================================
    PerformanceSample PerformanceMonitor::GetLatestSample() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_samples.empty()) {
            PerformanceSample empty;
            return empty;
        }
        return m_samples.back();
    }

    bool PerformanceMonitor::HasLiveData() const {
        if (!m_running.load()) return false;
        std::lock_guard<std::mutex> lock(m_mutex);
        return !m_samples.empty();
    }

} // namespace PerfBlackBox