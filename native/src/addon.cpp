#include <napi.h>
#ifdef _WIN32
#include "audio_session_enum.h"
#include "process_capture.h"
#endif

Napi::Object Init(Napi::Env env, Napi::Object exports) {
#ifdef _WIN32
    exports.Set("listAudioSessions", Napi::Function::New(env, ListAudioSessions));
    InitProcessCapture(env, exports);
#endif
    return exports;
}

NODE_API_MODULE(process_audio_capture, Init)
