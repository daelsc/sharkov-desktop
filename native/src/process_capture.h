#pragma once
#ifdef _WIN32
#include <napi.h>
void InitProcessCapture(Napi::Env env, Napi::Object exports);
#endif
