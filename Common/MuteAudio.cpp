// -----------------------------------------------------------------------
// GameLauncher
// Copyright (c) 2026 Abdualrhman Aljohani (https://github.com/abdualrhman-aljohani)
// Licensed under the PolyForm Noncommercial License 1.0.0.
// See LICENSE.txt in the project root. Commercial use, sale, or
// redistribution as part of a paid product is NOT permitted without
// prior written permission from the copyright holder.
// -----------------------------------------------------------------------
// MuteAudio.cpp
#include "MuteAudio.h"
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <tlhelp32.h>
#include <set>
#include <algorithm>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")

namespace MuteAudio {

// ----------------------------- أدوات داخلية -----------------------------
struct SessionEntry {
    DWORD pid;
    ISimpleAudioVolume* vol;
};

static bool CollectSessions(std::vector<SessionEntry>& out) {
    out.clear();
    IMMDeviceEnumerator* pEnum = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&pEnum));
    if (FAILED(hr) || !pEnum) return false;

    IMMDevice* pDevice = nullptr;
    hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr) || !pDevice) { pEnum->Release(); return false; }

    IAudioSessionManager2* pMgr = nullptr;
    hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
                            nullptr, (void**)&pMgr);
    if (FAILED(hr) || !pMgr) { pDevice->Release(); pEnum->Release(); return false; }

    IAudioSessionEnumerator* pSessions = nullptr;
    hr = pMgr->GetSessionEnumerator(&pSessions);
    if (FAILED(hr) || !pSessions) {
        pMgr->Release(); pDevice->Release(); pEnum->Release(); return false;
    }

    int count = 0;
    pSessions->GetCount(&count);
    for (int i = 0; i < count; i++) {
        IAudioSessionControl* pCtrl = nullptr;
        if (FAILED(pSessions->GetSession(i, &pCtrl)) || !pCtrl) continue;

        IAudioSessionControl2* pCtrl2 = nullptr;
        if (SUCCEEDED(pCtrl->QueryInterface(IID_PPV_ARGS(&pCtrl2))) && pCtrl2) {
            DWORD pid = 0;
            if (SUCCEEDED(pCtrl2->GetProcessId(&pid)) && pid != 0) {
                ISimpleAudioVolume* pVol = nullptr;
                if (SUCCEEDED(pCtrl2->QueryInterface(IID_PPV_ARGS(&pVol))) && pVol) {
                    out.push_back({ pid, pVol });
                }
            }
            pCtrl2->Release();
        }
        pCtrl->Release();
    }

    pSessions->Release();
    pMgr->Release();
    pDevice->Release();
    pEnum->Release();
    return true;
}

static void ReleaseSessions(std::vector<SessionEntry>& sessions) {
    for (auto& s : sessions) if (s.vol) s.vol->Release();
    sessions.clear();
}

// نسخة محلية من GetFileNameFromPath لتجنب الاعتماد على Config.cpp
static std::wstring GetFileNameLocal(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? path : path.substr(pos + 1);
}

// احصل على اسم ملف exe من PID
static std::wstring GetExeNameFromPid(DWORD pid) {
    if (pid == 0) return L"";
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProc) return L"";
    wchar_t path[MAX_PATH] = L"";
    DWORD size = MAX_PATH;
    std::wstring result;
    if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
        result = GetFileNameLocal(path);
    }
    CloseHandle(hProc);
    return result;
}

// ----------------------------- API العام -----------------------------
int GetMuteStateByPid(DWORD pid) {
    if (pid == 0) return 0;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hr == S_OK);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return 0;

    int result = 0;
    std::vector<SessionEntry> sessions;
    if (CollectSessions(sessions)) {
        for (auto& s : sessions) {
            if (s.pid == pid) {
                BOOL muted = FALSE;
                if (SUCCEEDED(s.vol->GetMute(&muted))) {
                    result = muted ? 2 : 1;
                    break;
                }
            }
        }
        ReleaseSessions(sessions);
    }
    if (needUninit) CoUninitialize();
    return result;
}

bool SetMuteByPid(DWORD pid, bool mute) {
    if (pid == 0) return false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hr == S_OK);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    bool ok = false;
    std::vector<SessionEntry> sessions;
    if (CollectSessions(sessions)) {
        for (auto& s : sessions) {
            if (s.pid == pid) {
                if (SUCCEEDED(s.vol->SetMute(mute ? TRUE : FALSE, nullptr))) ok = true;
            }
        }
        ReleaseSessions(sessions);
    }
    if (needUninit) CoUninitialize();
    return ok;
}

static std::vector<DWORD> FindPidsByExeName(const std::wstring& exeName) {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0) pids.push_back(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pids;
}

bool SetMuteByExeName(const std::wstring& exeName, bool mute) {
    auto pids = FindPidsByExeName(exeName);
    bool any = false;
    for (DWORD pid : pids) if (SetMuteByPid(pid, mute)) any = true;
    return any;
}

int GetMuteStateByExeName(const std::wstring& exeName) {
    auto pids = FindPidsByExeName(exeName);
    int lastKnown = 0;
    for (DWORD pid : pids) {
        int s = GetMuteStateByPid(pid);
        if (s == 2) return 2;
        if (s == 1) lastKnown = 1;
    }
    return lastKnown;
}

DWORD GetForegroundProcessId() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

DWORD GetProcessIdFromWindow(HWND hwnd) {
    if (!hwnd) return 0;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    return pid;
}

struct EnumWindowCtx {
    DWORD pid;
    HWND best;
    LONG bestArea;
};

static BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    EnumWindowCtx* ctx = (EnumWindowCtx*)lParam;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != ctx->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) return TRUE;

    RECT rc;
    if (!GetWindowRect(hwnd, &rc)) return TRUE;
    LONG w = rc.right - rc.left;
    LONG h = rc.bottom - rc.top;
    if (w < 200 || h < 150) return TRUE;

    LONG area = w * h;
    if (!ctx->best || area > ctx->bestArea) {
        ctx->best = hwnd;
        ctx->bestArea = area;
    }
    return TRUE;
}

HWND FindMainWindowForProcess(DWORD pid) {
    if (pid == 0) return nullptr;
    EnumWindowCtx ctx = { pid, nullptr, 0 };
    EnumWindows(EnumWindowsProc, (LPARAM)&ctx);
    return ctx.best;
}

// ----------------------------- قائمة التطبيقات المكتومة -----------------------------
std::vector<MutedAppInfo> GetMutedApps() {
    std::vector<MutedAppInfo> result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hr == S_OK);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return result;

    std::vector<SessionEntry> sessions;
    if (CollectSessions(sessions)) {
        std::set<std::wstring> seenExeNames;
        for (auto& s : sessions) {
            BOOL muted = FALSE;
            if (FAILED(s.vol->GetMute(&muted)) || !muted) continue;

            std::wstring exeName = GetExeNameFromPid(s.pid);
            if (exeName.empty()) continue;
            if (seenExeNames.count(exeName)) continue;
            seenExeNames.insert(exeName);

            MutedAppInfo info;
            info.exeName = exeName;
            info.displayName = exeName;
            size_t dot = info.displayName.find_last_of(L'.');
            if (dot != std::wstring::npos) info.displayName = info.displayName.substr(0, dot);
            result.push_back(info);
        }
        ReleaseSessions(sessions);
    }

    if (needUninit) CoUninitialize();

    std::sort(result.begin(), result.end(),
              [](const MutedAppInfo& a, const MutedAppInfo& b) {
                  return _wcsicmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
              });
    return result;
}

bool UnmuteAll() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needUninit = (hr == S_OK);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    bool any = false;
    std::vector<SessionEntry> sessions;
    if (CollectSessions(sessions)) {
        for (auto& s : sessions) {
            BOOL muted = FALSE;
            if (SUCCEEDED(s.vol->GetMute(&muted)) && muted) {
                if (SUCCEEDED(s.vol->SetMute(FALSE, nullptr))) any = true;
            }
        }
        ReleaseSessions(sessions);
    }
    if (needUninit) CoUninitialize();
    return any;
}

} // namespace MuteAudio