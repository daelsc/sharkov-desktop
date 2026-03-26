#ifdef _WIN32
#include "audio_session_enum.h"
#include <windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <psapi.h>
#include <string>
#include <vector>

static std::string WideToUtf8(const wchar_t* wide) {
    if (!wide || !wide[0]) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string result(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], len, nullptr, nullptr);
    return result;
}

static std::string GetProcessName(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return "";
    wchar_t exePath[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    std::string name;
    if (QueryFullProcessImageNameW(hProcess, 0, exePath, &size)) {
        name = WideToUtf8(exePath);
    }
    CloseHandle(hProcess);
    return name;
}

static std::string ExtractFileName(const std::string& path) {
    auto pos = path.find_last_of("\\/");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

Napi::Value ListAudioSessions(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool comInitialized = SUCCEEDED(hr) || hr == S_FALSE;
    // RPC_E_CHANGED_MODE means COM is already initialized with a different model — that's fine
    if (hr == RPC_E_CHANGED_MODE) comInitialized = false;

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioSessionManager2* pSessionManager = nullptr;
    IAudioSessionEnumerator* pSessionEnumerator = nullptr;

    Napi::Array result = Napi::Array::New(env);
    uint32_t idx = 0;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnumerator));
    if (FAILED(hr)) goto cleanup;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) goto cleanup;

    hr = pDevice->Activate(
        __uuidof(IAudioSessionManager2), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(&pSessionManager));
    if (FAILED(hr)) goto cleanup;

    hr = pSessionManager->GetSessionEnumerator(&pSessionEnumerator);
    if (FAILED(hr)) goto cleanup;

    {
        int count = 0;
        hr = pSessionEnumerator->GetCount(&count);
        if (FAILED(hr)) goto cleanup;

        for (int i = 0; i < count; i++) {
            IAudioSessionControl* pSessionControl = nullptr;
            IAudioSessionControl2* pSessionControl2 = nullptr;

            hr = pSessionEnumerator->GetSession(i, &pSessionControl);
            if (FAILED(hr)) continue;

            hr = pSessionControl->QueryInterface(
                __uuidof(IAudioSessionControl2),
                reinterpret_cast<void**>(&pSessionControl2));
            if (FAILED(hr)) {
                pSessionControl->Release();
                continue;
            }

            AudioSessionState state;
            hr = pSessionControl->GetState(&state);
            if (FAILED(hr) || state == AudioSessionStateExpired) {
                pSessionControl2->Release();
                pSessionControl->Release();
                continue;
            }

            DWORD pid = 0;
            hr = pSessionControl2->GetProcessId(&pid);
            if (FAILED(hr) || pid == 0) {
                pSessionControl2->Release();
                pSessionControl->Release();
                continue;
            }

            std::string exePath = GetProcessName(pid);
            std::string name = ExtractFileName(exePath);

            Napi::Object session = Napi::Object::New(env);
            session.Set("pid", Napi::Number::New(env, static_cast<double>(pid)));
            session.Set("name", Napi::String::New(env, name));
            session.Set("exePath", Napi::String::New(env, exePath));
            result.Set(idx++, session);

            pSessionControl2->Release();
            pSessionControl->Release();
        }
    }

cleanup:
    if (pSessionEnumerator) pSessionEnumerator->Release();
    if (pSessionManager) pSessionManager->Release();
    if (pDevice) pDevice->Release();
    if (pEnumerator) pEnumerator->Release();
    if (comInitialized) CoUninitialize();

    return result;
}
#endif
