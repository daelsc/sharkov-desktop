#pragma once
#ifdef _WIN32
#include <napi.h>
Napi::Value ListAudioSessions(const Napi::CallbackInfo& info);
#endif
