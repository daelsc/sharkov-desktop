#ifdef _WIN32
#include "process_capture.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/implements.h>

#include <thread>
#include <atomic>
#include <string>
#include <cstring>
#include <cmath>
#include <vector>
#include <sstream>

// Process loopback types — define directly to avoid SDK version issues
#ifndef AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK
typedef enum { AUDIOCLIENT_ACTIVATION_TYPE_DEFAULT = 0, AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK = 1 } AUDIOCLIENT_ACTIVATION_TYPE;
typedef enum { PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE = 0, PROCESS_LOOPBACK_MODE_EXCLUDE_TARGET_PROCESS_TREE = 1 } PROCESS_LOOPBACK_MODE;
typedef struct { DWORD TargetProcessId; PROCESS_LOOPBACK_MODE ProcessLoopbackMode; } AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS;
typedef struct { AUDIOCLIENT_ACTIVATION_TYPE ActivationType; union { AUDIOCLIENT_PROCESS_LOOPBACK_PARAMS ProcessLoopbackParams; }; } AUDIOCLIENT_ACTIVATION_PARAMS;
#endif
#ifndef VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK
#define VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK L"VAD\\Process_Loopback"
#endif

using namespace Microsoft::WRL;

// ---------------------------------------------------------------------------
// ActivateHandler — IActivateAudioInterfaceCompletionHandler via WRL
// ---------------------------------------------------------------------------
class ActivateHandler
    : public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase,
                          IActivateAudioInterfaceCompletionHandler> {
public:
    ActivateHandler() : m_hr(E_FAIL), m_event(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}

    STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* operation) override {
        HRESULT hrActivate = E_FAIL;
        IUnknown* pUnknown = nullptr;

        HRESULT hr = operation->GetActivateResult(&hrActivate, &pUnknown);
        if (SUCCEEDED(hr) && SUCCEEDED(hrActivate) && pUnknown) {
            hr = pUnknown->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void**>(&m_client));
            if (FAILED(hr)) {
                m_hr = hr;
            } else {
                m_hr = S_OK;
            }
            pUnknown->Release();
        } else {
            m_hr = FAILED(hr) ? hr : hrActivate;
        }

        SetEvent(m_event);
        return S_OK;
    }

    HRESULT Wait(DWORD timeoutMs = 5000) {
        DWORD res = WaitForSingleObject(m_event, timeoutMs);
        if (res != WAIT_OBJECT_0) return HRESULT_FROM_WIN32(ERROR_TIMEOUT);
        return m_hr;
    }

    IAudioClient* GetClient() const { return m_client; }

    ~ActivateHandler() {
        if (m_event) CloseHandle(m_event);
        // Do NOT release m_client here — ownership is transferred to caller
    }

private:
    HRESULT m_hr;
    HANDLE m_event;
    IAudioClient* m_client = nullptr;
};

// ---------------------------------------------------------------------------
// Helper: format HRESULT to hex string
// ---------------------------------------------------------------------------
static std::string HrToHex(HRESULT hr) {
    std::ostringstream oss;
    oss << "0x" << std::hex << static_cast<unsigned long>(hr);
    return oss.str();
}

// ---------------------------------------------------------------------------
// ProcessCapture — Napi::ObjectWrap
// ---------------------------------------------------------------------------
class ProcessCapture : public Napi::ObjectWrap<ProcessCapture> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports) {
        Napi::Function func = DefineClass(env, "ProcessCapture", {
            InstanceMethod("start", &ProcessCapture::Start),
            InstanceMethod("stop", &ProcessCapture::Stop),
        });
        exports.Set("ProcessCapture", func);
        return exports;
    }

    ProcessCapture(const Napi::CallbackInfo& info)
        : Napi::ObjectWrap<ProcessCapture>(info) {
        Napi::Env env = info.Env();

        if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
            Napi::TypeError::New(env, "Expected (options, callback)").ThrowAsJavaScriptException();
            return;
        }

        Napi::Object opts = info[0].As<Napi::Object>();
        m_pid = opts.Get("pid").As<Napi::Number>().Uint32Value();
        m_targetSampleRate = opts.Has("sampleRate")
            ? opts.Get("sampleRate").As<Napi::Number>().Uint32Value()
            : 48000;
        m_targetChannels = opts.Has("channels")
            ? opts.Get("channels").As<Napi::Number>().Uint32Value()
            : 2;

        m_tsfn = Napi::ThreadSafeFunction::New(
            env,
            info[1].As<Napi::Function>(),
            "ProcessCaptureCallback",
            0,   // unlimited queue
            1    // one thread
        );
    }

    ~ProcessCapture() {
        StopInternal();
    }

private:
    DWORD m_pid = 0;
    uint32_t m_targetSampleRate = 48000;
    uint32_t m_targetChannels = 2;

    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    HANDLE m_captureEvent = nullptr;
    WAVEFORMATEX* m_captureFormat = nullptr;

    std::thread m_captureThread;
    std::atomic<bool> m_running{false};
    Napi::ThreadSafeFunction m_tsfn;

    // Resampler state — persists across packets for continuous linear interpolation
    double m_resampleFrac = 0.0;
    float m_prevSampleL = 0.0f;
    float m_prevSampleR = 0.0f;

    void Start(const Napi::CallbackInfo& info) {
        Napi::Env env = info.Env();

        if (m_running.load()) {
            Napi::Error::New(env, "Already capturing").ThrowAsJavaScriptException();
            return;
        }

        HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
            Napi::Error::New(env,
                "CoInitializeEx failed: " + HrToHex(hr)).ThrowAsJavaScriptException();
            return;
        }

        // --- Build activation params for process loopback ---
        AUDIOCLIENT_ACTIVATION_PARAMS audioclientParams = {};
        audioclientParams.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
        audioclientParams.ProcessLoopbackParams.TargetProcessId = m_pid;
        audioclientParams.ProcessLoopbackParams.ProcessLoopbackMode =
            PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

        PROPVARIANT activateParams = {};
        activateParams.vt = VT_BLOB;
        activateParams.blob.cbSize = sizeof(audioclientParams);
        activateParams.blob.pBlobData = reinterpret_cast<BYTE*>(&audioclientParams);

        // --- Activate async ---
        ComPtr<ActivateHandler> handler = Make<ActivateHandler>();
        IActivateAudioInterfaceAsyncOperation* asyncOp = nullptr;


        hr = ActivateAudioInterfaceAsync(
            VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
            __uuidof(IAudioClient),
            &activateParams,
            handler.Get(),
            &asyncOp);

        if (FAILED(hr)) {
            std::string msg = "ActivateAudioInterfaceAsync failed for PID " +
                              std::to_string(m_pid) + ": " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            return;
        }

        hr = handler->Wait(10000);
        if (asyncOp) asyncOp->Release();

        if (FAILED(hr)) {
            std::string msg = "Audio activation failed for PID " +
                              std::to_string(m_pid) + ": " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            return;
        }

        m_audioClient = handler->GetClient();
        if (!m_audioClient) {
            Napi::Error::New(env, "Got null IAudioClient").ThrowAsJavaScriptException();
            return;
        }


        // --- Set up capture format (process loopback doesn't support GetMixFormat) ---
        // Use 48kHz stereo float32 and let Windows handle conversion via AUTOCONVERTPCM
        WAVEFORMATEX captureFormat = {};
        captureFormat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        captureFormat.nChannels = 2;
        captureFormat.nSamplesPerSec = 48000;
        captureFormat.wBitsPerSample = 32;
        captureFormat.nBlockAlign = captureFormat.nChannels * captureFormat.wBitsPerSample / 8;
        captureFormat.nAvgBytesPerSec = captureFormat.nSamplesPerSec * captureFormat.nBlockAlign;
        captureFormat.cbSize = 0;
        // Allocate format on heap so it persists for the capture loop
        m_captureFormat = (WAVEFORMATEX*)CoTaskMemAlloc(sizeof(WAVEFORMATEX));
        if (m_captureFormat) *m_captureFormat = captureFormat;

        // --- Initialize audio client ---
        REFERENCE_TIME hnsBufferDuration = 200000; // 20ms in 100ns units
        hr = m_audioClient->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
            AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
            hnsBufferDuration,
            0,
            &captureFormat,
            nullptr);

        if (FAILED(hr)) {
            std::string msg = "IAudioClient::Initialize failed for PID " +
                              std::to_string(m_pid) + ": " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            ReleaseResources();
            return;
        }

        // --- Set event handle ---
        m_captureEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!m_captureEvent) {
            Napi::Error::New(env, "CreateEvent failed").ThrowAsJavaScriptException();
            ReleaseResources();
            return;
        }

        hr = m_audioClient->SetEventHandle(m_captureEvent);
        if (FAILED(hr)) {
            std::string msg = "SetEventHandle failed: " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            ReleaseResources();
            return;
        }

        // --- Get capture client ---
        hr = m_audioClient->GetService(
            __uuidof(IAudioCaptureClient),
            reinterpret_cast<void**>(&m_captureClient));
        if (FAILED(hr)) {
            std::string msg = "GetService(IAudioCaptureClient) failed: " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            ReleaseResources();
            return;
        }

        // --- Start capture ---
        hr = m_audioClient->Start();
        if (FAILED(hr)) {
            std::string msg = "IAudioClient::Start failed for PID " +
                              std::to_string(m_pid) + ": " + HrToHex(hr);
            Napi::Error::New(env, msg).ThrowAsJavaScriptException();
            ReleaseResources();
            return;
        }

        // Reset resampler state
        m_resampleFrac = 0.0;
        m_prevSampleL = 0.0f;
        m_prevSampleR = 0.0f;

        m_running.store(true);
        m_captureThread = std::thread(&ProcessCapture::CaptureLoop, this);
    }

    void Stop(const Napi::CallbackInfo&) {
        StopInternal();
    }

    // ---------------------------------------------------------------------------
    // Convert captured buffer to float stereo samples
    // ---------------------------------------------------------------------------
    void ConvertToFloatStereo(const BYTE* pData, UINT32 numFrames,
                              const WAVEFORMATEX* fmt,
                              std::vector<float>& outStereo) {
        WORD srcChannels = fmt->nChannels;
        WORD bitsPerSample = fmt->wBitsPerSample;
        WORD tag = fmt->wFormatTag;

        // Check for extensible format
        if (tag == WAVE_FORMAT_EXTENSIBLE && fmt->cbSize >= 22) {
            auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(fmt);
            // KSDATAFORMAT_SUBTYPE_IEEE_FLOAT
            static const GUID kFloat = {0x00000003, 0x0000, 0x0010,
                {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
            // KSDATAFORMAT_SUBTYPE_PCM
            static const GUID kPcm = {0x00000001, 0x0000, 0x0010,
                {0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71}};
            if (ext->SubFormat == kFloat) {
                tag = 0x0003; // IEEE float
            } else if (ext->SubFormat == kPcm) {
                tag = WAVE_FORMAT_PCM;
            }
        }

        outStereo.resize(numFrames * 2);

        for (UINT32 i = 0; i < numFrames; i++) {
            float left = 0.0f, right = 0.0f;

            if (tag == 0x0003 && bitsPerSample == 32) {
                // IEEE 32-bit float
                const float* samples = reinterpret_cast<const float*>(pData) + i * srcChannels;
                left = samples[0];
                right = (srcChannels >= 2) ? samples[1] : samples[0];
            } else if (tag == WAVE_FORMAT_PCM && bitsPerSample == 16) {
                // 16-bit PCM
                const int16_t* samples = reinterpret_cast<const int16_t*>(pData) + i * srcChannels;
                left = samples[0] / 32768.0f;
                right = (srcChannels >= 2) ? samples[1] / 32768.0f : left;
            } else if (tag == WAVE_FORMAT_PCM && bitsPerSample == 24) {
                // 24-bit PCM (packed)
                size_t frameOffset = i * srcChannels * 3;
                auto read24 = [&](size_t byteIdx) -> float {
                    int32_t val = static_cast<int32_t>(pData[byteIdx])
                                | (static_cast<int32_t>(pData[byteIdx + 1]) << 8)
                                | (static_cast<int32_t>(static_cast<int8_t>(pData[byteIdx + 2])) << 16);
                    return val / 8388608.0f;
                };
                left = read24(frameOffset);
                right = (srcChannels >= 2) ? read24(frameOffset + 3) : left;
            } else if (tag == WAVE_FORMAT_PCM && bitsPerSample == 32) {
                // 32-bit PCM
                const int32_t* samples = reinterpret_cast<const int32_t*>(pData) + i * srcChannels;
                left = samples[0] / 2147483648.0f;
                right = (srcChannels >= 2) ? samples[1] / 2147483648.0f : left;
            } else {
                // Unknown format — output silence
                left = 0.0f;
                right = 0.0f;
            }

            // Downmix: for mono duplicate, for multi-channel take L/R (channels 0,1)
            outStereo[i * 2] = left;
            outStereo[i * 2 + 1] = right;
        }
    }

    // ---------------------------------------------------------------------------
    // Resample stereo float data using linear interpolation with fractional state
    // ---------------------------------------------------------------------------
    void ResampleStereo(const std::vector<float>& input, uint32_t srcRate,
                        uint32_t dstRate, std::vector<float>& output) {
        if (srcRate == dstRate) {
            output = input;
            return;
        }

        size_t srcFrames = input.size() / 2;
        if (srcFrames == 0) return;

        double ratio = static_cast<double>(srcRate) / static_cast<double>(dstRate);

        // Estimate output size
        size_t estOutputFrames = static_cast<size_t>(
            std::ceil(static_cast<double>(srcFrames) / ratio)) + 2;
        output.reserve(estOutputFrames * 2);

        while (m_resampleFrac < static_cast<double>(srcFrames)) {
            size_t idx = static_cast<size_t>(m_resampleFrac);
            double frac = m_resampleFrac - static_cast<double>(idx);

            float l0, r0, l1, r1;

            if (idx == 0 && m_resampleFrac < 1.0) {
                // Use previous packet's last sample for interpolation at boundary
                l0 = (idx == 0) ? m_prevSampleL : input[(idx - 1) * 2];
                r0 = (idx == 0) ? m_prevSampleR : input[(idx - 1) * 2 + 1];
                // But if idx==0 and frac==0, we want the first sample exactly
                if (frac == 0.0) {
                    l0 = input[0];
                    r0 = input[1];
                }
            } else {
                l0 = input[idx * 2];
                r0 = input[idx * 2 + 1];
            }

            if (idx + 1 < srcFrames) {
                l1 = input[(idx + 1) * 2];
                r1 = input[(idx + 1) * 2 + 1];
            } else {
                l1 = l0;
                r1 = r0;
            }

            float outL = static_cast<float>(l0 + (l1 - l0) * frac);
            float outR = static_cast<float>(r0 + (r1 - r0) * frac);

            output.push_back(outL);
            output.push_back(outR);

            m_resampleFrac += ratio;
        }

        // Save last sample for next packet boundary interpolation
        if (srcFrames > 0) {
            m_prevSampleL = input[(srcFrames - 1) * 2];
            m_prevSampleR = input[(srcFrames - 1) * 2 + 1];
        }

        // Subtract consumed frames so fractional state carries over
        m_resampleFrac -= static_cast<double>(srcFrames);
    }

    // ---------------------------------------------------------------------------
    // Capture loop — runs on a background thread
    // ---------------------------------------------------------------------------
    void CaptureLoop() {
        // COM must be initialized on this thread
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        while (m_running.load()) {
            DWORD waitResult = WaitForSingleObject(m_captureEvent, 100);
            if (!m_running.load()) break;
            if (waitResult != WAIT_OBJECT_0) continue;

            UINT32 packetLength = 0;
            HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) {
                SendError("GetNextPacketSize failed: " + HrToHex(hr));
                break;
            }

            while (packetLength > 0 && m_running.load()) {
                BYTE* pData = nullptr;
                UINT32 numFrames = 0;
                DWORD flags = 0;

                hr = m_captureClient->GetBuffer(&pData, &numFrames, &flags, nullptr, nullptr);
                if (FAILED(hr)) {
                    SendError("GetBuffer failed: " + HrToHex(hr));
                    goto loopEnd;
                }

                if (numFrames > 0) {
                    std::vector<float> stereoFloat;

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        // Silent buffer — send zeros
                        stereoFloat.resize(numFrames * 2, 0.0f);
                    } else {
                        ConvertToFloatStereo(pData, numFrames, m_captureFormat, stereoFloat);
                    }

                    // Resample to target rate
                    std::vector<float> resampled;
                    ResampleStereo(stereoFloat, m_captureFormat->nSamplesPerSec,
                                   m_targetSampleRate, resampled);

                    if (!resampled.empty()) {
                        // Copy to heap for ThreadSafeFunction
                        size_t byteLen = resampled.size() * sizeof(float);
                        float* buf = new float[resampled.size()];
                        std::memcpy(buf, resampled.data(), byteLen);
                        size_t floatCount = resampled.size();

                        m_tsfn.BlockingCall(buf,
                            [floatCount](Napi::Env env, Napi::Function callback, float* data) {
                                auto ab = Napi::ArrayBuffer::New(env,
                                    floatCount * sizeof(float));
                                std::memcpy(ab.Data(), data, floatCount * sizeof(float));
                                delete[] data;

                                auto f32 = Napi::Float32Array::New(env,
                                    floatCount, ab, 0);
                                callback.Call({f32});
                            });
                    }
                }

                m_captureClient->ReleaseBuffer(numFrames);

                hr = m_captureClient->GetNextPacketSize(&packetLength);
                if (FAILED(hr)) {
                    SendError("GetNextPacketSize failed: " + HrToHex(hr));
                    goto loopEnd;
                }
            }
        }

    loopEnd:
        CoUninitialize();
    }

    void SendError(const std::string& msg) {
        std::string* errMsg = new std::string(msg);
        m_tsfn.BlockingCall(errMsg,
            [](Napi::Env env, Napi::Function callback, std::string* message) {
                callback.Call({Napi::Error::New(env, *message).Value()});
                delete message;
            });
    }

    void StopInternal() {
        if (!m_running.exchange(false)) return;

        // Signal event to unblock wait
        if (m_captureEvent) SetEvent(m_captureEvent);

        if (m_captureThread.joinable()) {
            m_captureThread.join();
        }

        m_tsfn.Release();
        ReleaseResources();
    }

    void ReleaseResources() {
        if (m_audioClient) {
            m_audioClient->Stop();
            m_audioClient->Release();
            m_audioClient = nullptr;
        }
        if (m_captureClient) {
            m_captureClient->Release();
            m_captureClient = nullptr;
        }
        if (m_captureFormat) {
            CoTaskMemFree(m_captureFormat);
            m_captureFormat = nullptr;
        }
        if (m_captureEvent) {
            CloseHandle(m_captureEvent);
            m_captureEvent = nullptr;
        }
    }
};

void InitProcessCapture(Napi::Env env, Napi::Object exports) {
    ProcessCapture::Init(env, exports);
}
#endif
