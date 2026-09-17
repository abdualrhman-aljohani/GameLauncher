// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// EtwFps.cpp
#include "EtwFps.h"

#include <evntrace.h>
#include <evntcons.h>
#include <vector>
#include <map>
#include <cstdio>
#include <fstream>

#pragma comment(lib, "advapi32.lib")

namespace PerfBlackBox {

static std::wstring DiagLogPath() {
    wchar_t appData[MAX_PATH] = L"";
    DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    std::wstring dir = std::wstring(appData) + L"\\GameLauncher";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\etw_diag.log";
}

static void EtwDiag(const std::wstring& msg) {
    std::wstring path = DiagLogPath();
    if (path.empty()) return;
    std::wofstream f(path.c_str(), std::ios::app);
    if (!f.is_open()) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ts[48];
    wsprintfW(ts, L"[%02d:%02d:%02d.%03d] ",
              st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    f << ts << msg << L"\n";
    f.flush();
    f.close();
}

// ============================================================
// ثوابت
// ============================================================
static const GUID kDxgiProviderGuid = {
    0xCA11C036, 0x0102, 0x4A2D,
    {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}
};

static const USHORT kPresentStartEventId    = 1;
static const USHORT kPresentStopEventId     = 2;
static const USHORT kPresentMpoStartEventId = 42;
static const USHORT kPresentMpoStopEventId  = 43;
static const USHORT kPresentLegacyEventId   = 73;
static const USHORT kPresentTaskId    = 1;
static const USHORT kPresentMpoTaskId = 9;

static const ULONG kBufferSizeKB = 32;
static const ULONG kMinBuffers   = 4;
static const ULONG kMaxBuffers   = 32;

static const ULONGLONG kFpsWindowMs = 1000;
static const ULONGLONG kPidIdleTimeoutMs = 3000;

// ✅ Warmup: تجاهل أول 2.5 ثانية من حساب FPS/frame time
static const ULONGLONG kWarmupMs = 2500;

static const wchar_t* kSessionPrefix = L"GameLauncherFps_";

// ============================================================
// Impl
// ============================================================
struct EtwFps::Impl {
    std::wstring sessionName;

    TRACEHANDLE sessionHandle  = 0;
    TRACEHANDLE traceHandle    = 0;
    bool        sessionStarted = false;
    bool        traceOpened    = false;

    std::atomic<DWORD> preferredPid{0};
    std::atomic<DWORD> activePid{0};

    HANDLE processThread = nullptr;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> running{false};

    // ✅ Warmup
    ULONGLONG warmupEndTick = 0;  // GetTickCount64 عند انتهاء الـ warmup

    struct PidTrack {
        ULONGLONG lastStartTs = 0;
        ULONGLONG lastSeenTick = 0;
        int       presentsInWindow = 0;
        float     lastFrameTimeMs = -1.0f;
    };

    mutable std::mutex tracksMutex;
    std::map<DWORD, PidTrack> tracks;
    ULONGLONG windowStartTick = 0;

    mutable std::mutex calcMutex;
    float currentFps = -1.0f;
    float lastFrameTimeMs = -1.0f;

    std::atomic<ULONGLONG> totalPresents{0};

    std::atomic<int> callbackCallCount{0};
    std::atomic<int> presentStartCount{0};
    std::atomic<int> presentMpoStartCount{0};
    std::atomic<int> presentLegacyCount{0};
    std::atomic<int> presentStopCount{0};
};

// ============================================================
// ETW Callback
// ============================================================
static VOID WINAPI EventRecordCallback(PEVENT_RECORD eventRecord) {
    if (!eventRecord || !eventRecord->UserContext) return;
    auto* impl = (EtwFps::Impl*)eventRecord->UserContext;

    impl->callbackCallCount.fetch_add(1, std::memory_order_relaxed);

    USHORT eventId = eventRecord->EventHeader.EventDescriptor.Id;
    USHORT task    = eventRecord->EventHeader.EventDescriptor.Task;
    USHORT opcode  = eventRecord->EventHeader.EventDescriptor.Opcode;
    DWORD  eventPid = eventRecord->EventHeader.ProcessId;

    bool isPresentStart = false;
    bool isPresentStop  = false;
    int  presentKind    = 0;

    if (eventId == kPresentStartEventId && task == kPresentTaskId) {
        isPresentStart = true; presentKind = 1;
    } else if (eventId == kPresentStopEventId && task == kPresentTaskId) {
        isPresentStop = true;
    } else if (eventId == kPresentMpoStartEventId && task == kPresentMpoTaskId) {
        isPresentStart = true; presentKind = 2;
    } else if (eventId == kPresentMpoStopEventId && task == kPresentMpoTaskId) {
        isPresentStop = true;
    } else if (eventId == kPresentLegacyEventId && task == kPresentTaskId) {
        isPresentStart = true; presentKind = 3;
    } else {
        return;
    }

    if (eventPid == 0) return;

    ULONGLONG ts = eventRecord->EventHeader.TimeStamp.QuadPart;
    ULONGLONG nowTick = GetTickCount64();

    if (isPresentStop) {
        impl->presentStopCount.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (!isPresentStart) return;

    if (presentKind == 1) impl->presentStartCount.fetch_add(1, std::memory_order_relaxed);
    else if (presentKind == 2) impl->presentMpoStartCount.fetch_add(1, std::memory_order_relaxed);
    else if (presentKind == 3) impl->presentLegacyCount.fetch_add(1, std::memory_order_relaxed);

    impl->totalPresents.fetch_add(1, std::memory_order_relaxed);

    // ✅ هل نحن في مرحلة الـ warmup؟
    bool inWarmup = (nowTick < impl->warmupEndTick);

    std::lock_guard<std::mutex> lock(impl->tracksMutex);

    auto& t = impl->tracks[eventPid];
    t.presentsInWindow++;
    t.lastSeenTick = nowTick;

    // ✅ Warmup: تحديث lastStartTs لكن لا نحسب frame time
    if (!inWarmup) {
        if (t.lastStartTs > 0 && ts > t.lastStartTs) {
            ULONGLONG delta100ns = ts - t.lastStartTs;
            float ft = (float)(delta100ns / 10000.0);
            // رفض قيم غير منطقية (> 500ms أو < 0.5ms)
            if (ft >= 0.5f && ft <= 500.0f) {
                t.lastFrameTimeMs = ft;
            }
        }
    }
    t.lastStartTs = ts;

    if (impl->windowStartTick == 0) {
        impl->windowStartTick = nowTick;
        return;
    }
    if (nowTick - impl->windowStartTick < kFpsWindowMs) return;

    ULONGLONG elapsed = nowTick - impl->windowStartTick;

    DWORD preferred = impl->preferredPid.load();
    DWORD chosenPid = 0;
    int chosenCount = 0;

    if (preferred != 0) {
        auto it = impl->tracks.find(preferred);
        if (it != impl->tracks.end() &&
            it->second.presentsInWindow > 0 &&
            (nowTick - it->second.lastSeenTick) < kPidIdleTimeoutMs) {
            chosenPid = preferred;
            chosenCount = it->second.presentsInWindow;
        }
    }

    if (chosenPid == 0) {
        for (auto& kv : impl->tracks) {
            if ((nowTick - kv.second.lastSeenTick) >= kPidIdleTimeoutMs) continue;
            if (kv.second.presentsInWindow > chosenCount) {
                chosenCount = kv.second.presentsInWindow;
                chosenPid = kv.first;
            }
        }
    }

    // ✅ لا نحدّث currentFps أثناء الـ warmup (يبقى -1)
    if (!inWarmup && chosenPid != 0 && chosenCount > 0) {
        float fps = (float)chosenCount * 1000.0f / (float)elapsed;
        // رفض قيم غير منطقية
        if (fps >= 1.0f && fps <= 1000.0f) {
            float ft = impl->tracks[chosenPid].lastFrameTimeMs;

            DWORD oldActive = impl->activePid.exchange(chosenPid);
            if (oldActive != chosenPid) {
                wchar_t buf[200];
                swprintf_s(buf, 200,
                    L"Active PID: %u -> %u (fps=%.1f, presents=%d)",
                    oldActive, chosenPid, (double)fps, chosenCount);
                EtwDiag(buf);
            }

            std::lock_guard<std::mutex> lock2(impl->calcMutex);
            impl->currentFps = fps;
            impl->lastFrameTimeMs = ft;
        }
    }

    for (auto& kv : impl->tracks) {
        kv.second.presentsInWindow = 0;
    }
    impl->windowStartTick = nowTick;
}

// ============================================================
// ProcessTrace Thread
// ============================================================
static DWORD WINAPI ProcessThreadProc(LPVOID param) {
    auto* impl = (EtwFps::Impl*)param;
    if (!impl) return 0;
    EtwDiag(L"ProcessTrace started");
    ULONG result = ProcessTrace(&impl->traceHandle, 1, nullptr, nullptr);
    wchar_t buf[64];
    swprintf_s(buf, 64, L"ProcessTrace exited, code=%u", result);
    EtwDiag(buf);
    return 0;
}

// ============================================================
// تنظيف جلسات ETW قديمة
// ============================================================
static int KillAllStaleSessions() {
    const ULONG MAX_SESSIONS = 64;
    std::vector<BYTE> buffer(sizeof(EVENT_TRACE_PROPERTIES) * MAX_SESSIONS + 32 * 1024);
    auto* props = (EVENT_TRACE_PROPERTIES*)buffer.data();
    ULONG sessionCount = MAX_SESSIONS;

    ULONG qResult = QueryAllTracesW(&props, 1, &sessionCount);
    if (qResult != ERROR_SUCCESS) {
        wchar_t b[128];
        swprintf_s(b, 128, L"QueryAllTraces result=%u (skipping cleanup)", qResult);
        EtwDiag(b);
        return 0;
    }

    int killed = 0;
    for (ULONG i = 0; i < sessionCount; i++) {
        const wchar_t* loggerName = (const wchar_t*)((BYTE*)&props[i] + props[i].LoggerNameOffset);
        if (!loggerName) continue;

        if (wcsncmp(loggerName, kSessionPrefix, wcslen(kSessionPrefix)) == 0) {
            ULONG stopResult = ControlTraceW(0, loggerName, &props[i], EVENT_TRACE_CONTROL_STOP);
            wchar_t b[256];
            swprintf_s(b, 256, L"Stale cleanup: '%s' stop result=%u", loggerName, stopResult);
            EtwDiag(b);
            if (stopResult == ERROR_SUCCESS) killed++;
        }
    }
    return killed;
}

// ============================================================
// Constructor / Destructor
// ============================================================
EtwFps::EtwFps() { m_impl = new Impl(); }
EtwFps::~EtwFps() { Stop(); delete m_impl; m_impl = nullptr; }

// ============================================================
// Start
// ============================================================
bool EtwFps::Start(DWORD targetPid) {
    if (!m_impl) return false;
    if (m_impl->running.load()) return false;

    m_impl->preferredPid.store(targetPid);
    m_impl->activePid.store(0);
    m_impl->stopRequested.store(false);

    // ✅ Warmup timer
    m_impl->warmupEndTick = GetTickCount64() + kWarmupMs;

    wchar_t nameBuf[96];
    swprintf_s(nameBuf, 96, L"%s%u_%llu",
               kSessionPrefix,
               (unsigned)GetCurrentProcessId(),
               (unsigned long long)GetTickCount64());
    m_impl->sessionName = nameBuf;

    {
        wchar_t buf[200];
        swprintf_s(buf, 200, L"=== EtwFps::Start preferredPid=%u session=%s warmup=%llums ===",
                  targetPid, m_impl->sessionName.c_str(), (unsigned long long)kWarmupMs);
        EtwDiag(buf);
    }

    int killed = KillAllStaleSessions();
    if (killed > 0) {
        wchar_t b[128];
        swprintf_s(b, 128, L"Killed %d stale ETW session(s)", killed);
        EtwDiag(b);
        Sleep(300);
    }

    size_t nameLen = (m_impl->sessionName.size() + 1) * sizeof(wchar_t);
    size_t propsSize = sizeof(EVENT_TRACE_PROPERTIES) + nameLen;

    bool started = false;
    for (int attempt = 1; attempt <= 3 && !started; attempt++) {
        if (attempt > 1) Sleep(500);

        {
            std::vector<BYTE> buf(propsSize, 0);
            auto* props = (EVENT_TRACE_PROPERTIES*)buf.data();
            props->Wnode.BufferSize    = (ULONG)propsSize;
            props->Wnode.Flags         = WNODE_FLAG_TRACED_GUID;
            props->Wnode.ClientContext = 1;
            props->LogFileMode         = EVENT_TRACE_REAL_TIME_MODE
                                       | EVENT_TRACE_INDEPENDENT_SESSION_MODE;
            props->LoggerNameOffset    = sizeof(EVENT_TRACE_PROPERTIES);
            props->BufferSize          = kBufferSizeKB;
            props->MinimumBuffers      = kMinBuffers;
            props->MaximumBuffers      = kMaxBuffers;
            props->FlushTimer          = 1;

            ULONG result = StartTraceW(&m_impl->sessionHandle,
                                        m_impl->sessionName.c_str(), props);
            wchar_t b[160];
            swprintf_s(b, 160, L"StartTrace[INDEPENDENT] attempt=%d result=%u",
                       attempt, result);
            EtwDiag(b);
            if (result == ERROR_SUCCESS) { started = true; break; }
        }

        {
            std::vector<BYTE> buf(propsSize, 0);
            auto* props = (EVENT_TRACE_PROPERTIES*)buf.data();
            props->Wnode.BufferSize    = (ULONG)propsSize;
            props->Wnode.Flags         = WNODE_FLAG_TRACED_GUID;
            props->Wnode.ClientContext = 1;
            props->LogFileMode         = EVENT_TRACE_REAL_TIME_MODE;
            props->LoggerNameOffset    = sizeof(EVENT_TRACE_PROPERTIES);
            props->BufferSize          = kBufferSizeKB;
            props->MinimumBuffers      = kMinBuffers;
            props->MaximumBuffers      = kMaxBuffers;
            props->FlushTimer          = 1;

            ULONG result = StartTraceW(&m_impl->sessionHandle,
                                        m_impl->sessionName.c_str(), props);
            wchar_t b[160];
            swprintf_s(b, 160, L"StartTrace[NORMAL] attempt=%d result=%u",
                       attempt, result);
            EtwDiag(b);
            if (result == ERROR_SUCCESS) { started = true; break; }
        }
    }

    if (!started) {
        EtwDiag(L"StartTrace FAILED after 3 attempts — ETW FPS disabled.");
        return false;
    }
    m_impl->sessionStarted = true;

    ULONG enableResult = EnableTraceEx2(
        m_impl->sessionHandle,
        &kDxgiProviderGuid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0, 0, 0, nullptr
    );

    {
        wchar_t b[256];
        swprintf_s(b, 256,
            L"EnableTraceEx2(DXGI) result=%u (0=OK, 5=ACCESS_DENIED, 87=BAD_PARAM, 4201=NO_PROVIDER, 183=ALREADY_EXISTS)",
            enableResult);
        EtwDiag(b);
    }

    if (enableResult != ERROR_SUCCESS) {
        ControlTraceW(m_impl->sessionHandle, m_impl->sessionName.c_str(),
                      nullptr, EVENT_TRACE_CONTROL_STOP);
        m_impl->sessionStarted = false;
        m_impl->sessionHandle = 0;
        EtwDiag(L"DXGI provider enable FAILED — ETW FPS disabled.");
        return false;
    }

    EVENT_TRACE_LOGFILEW logFile = {};
    logFile.LoggerName          = (LPWSTR)m_impl->sessionName.c_str();
    logFile.ProcessTraceMode    = PROCESS_TRACE_MODE_REAL_TIME
                                | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.EventRecordCallback = &EventRecordCallback;
    logFile.Context             = m_impl;

    m_impl->traceHandle = OpenTraceW(&logFile);
    if (m_impl->traceHandle == INVALID_PROCESSTRACE_HANDLE) {
        ULONG gle = GetLastError();
        wchar_t b[128];
        swprintf_s(b, 128, L"OpenTrace FAILED (GetLastError=%u)", gle);
        EtwDiag(b);
        ControlTraceW(m_impl->sessionHandle, m_impl->sessionName.c_str(),
                      nullptr, EVENT_TRACE_CONTROL_STOP);
        m_impl->sessionStarted = false;
        m_impl->sessionHandle = 0;
        m_impl->traceHandle = 0;
        return false;
    }
    m_impl->traceOpened = true;
    EtwDiag(L"OpenTrace OK");

    m_impl->running.store(true);
    m_impl->processThread = CreateThread(nullptr, 0, ProcessThreadProc, m_impl, 0, nullptr);
    if (!m_impl->processThread) {
        EtwDiag(L"CreateThread FAILED");
        CloseTrace(m_impl->traceHandle);
        ControlTraceW(m_impl->sessionHandle, m_impl->sessionName.c_str(),
                      nullptr, EVENT_TRACE_CONTROL_STOP);
        m_impl->traceOpened = false;
        m_impl->sessionStarted = false;
        m_impl->traceHandle = 0;
        m_impl->sessionHandle = 0;
        m_impl->running.store(false);
        return false;
    }

    SetThreadPriority(m_impl->processThread, THREAD_PRIORITY_BELOW_NORMAL);
    EtwDiag(L"=== ETW session STARTED successfully ===");
    return true;
}

// ============================================================
// Stop
// ============================================================
void EtwFps::Stop() {
    if (!m_impl) return;
    if (!m_impl->running.load()) {
        if (m_impl->sessionStarted) {
            ControlTraceW(m_impl->sessionHandle, m_impl->sessionName.c_str(),
                          nullptr, EVENT_TRACE_CONTROL_STOP);
            m_impl->sessionStarted = false;
            m_impl->sessionHandle = 0;
        }
        return;
    }

    EtwDiag(L"EtwFps::Stop called");
    m_impl->stopRequested.store(true);

    if (m_impl->sessionStarted) {
        ControlTraceW(m_impl->sessionHandle, m_impl->sessionName.c_str(),
                      nullptr, EVENT_TRACE_CONTROL_STOP);
        m_impl->sessionStarted = false;
        m_impl->sessionHandle = 0;
    }

    if (m_impl->processThread) {
        WaitForSingleObject(m_impl->processThread, 3000);
        CloseHandle(m_impl->processThread);
        m_impl->processThread = nullptr;
    }

    if (m_impl->traceOpened) {
        CloseTrace(m_impl->traceHandle);
        m_impl->traceOpened = false;
        m_impl->traceHandle = 0;
    }

    {
        wchar_t b[400];
        float fps = -1.0f;
        {
            std::lock_guard<std::mutex> lock(m_impl->calcMutex);
            fps = m_impl->currentFps;
        }
        swprintf_s(b, 400,
            L"=== STOP: Callbacks=%d, Present(1)=%d, MPO(42)=%d, Legacy(73)=%d, Stop=%d, activePid=%u, lastFps=%.1f ===",
            m_impl->callbackCallCount.load(),
            m_impl->presentStartCount.load(),
            m_impl->presentMpoStartCount.load(),
            m_impl->presentLegacyCount.load(),
            m_impl->presentStopCount.load(),
            m_impl->activePid.load(),
            (double)fps);
        EtwDiag(b);
    }

    m_impl->running.store(false);
    m_impl->preferredPid.store(0);
    m_impl->activePid.store(0);

    {
        std::lock_guard<std::mutex> lock(m_impl->tracksMutex);
        m_impl->tracks.clear();
        m_impl->windowStartTick = 0;
    }
    {
        std::lock_guard<std::mutex> lock(m_impl->calcMutex);
        m_impl->currentFps = -1.0f;
        m_impl->lastFrameTimeMs = -1.0f;
    }
    m_impl->totalPresents.store(0);
    m_impl->callbackCallCount.store(0);
    m_impl->presentStartCount.store(0);
    m_impl->presentMpoStartCount.store(0);
    m_impl->presentLegacyCount.store(0);
    m_impl->presentStopCount.store(0);
}

// ============================================================
// Query
// ============================================================
bool EtwFps::IsRunning() const {
    return m_impl && m_impl->running.load();
}

void EtwFps::UpdateTargetPid(DWORD newPid) {
    if (!m_impl) return;
    m_impl->preferredPid.store(newPid);
}

float EtwFps::GetFps() const {
    if (!m_impl) return -1.0f;
    std::lock_guard<std::mutex> lock(m_impl->calcMutex);
    return m_impl->currentFps;
}

float EtwFps::GetLastFrameTimeMs() const {
    if (!m_impl) return -1.0f;
    std::lock_guard<std::mutex> lock(m_impl->calcMutex);
    return m_impl->lastFrameTimeMs;
}

ULONGLONG EtwFps::GetTotalPresents() const {
    if (!m_impl) return 0;
    return m_impl->totalPresents.load();
}

} // namespace PerfBlackBox