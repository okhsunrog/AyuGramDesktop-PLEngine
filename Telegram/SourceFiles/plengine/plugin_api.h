#pragma once

#include <cstdint>
#include <functional>

#ifdef AYUPLUGIN
#ifdef _WIN32
#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define EXTERN_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#endif
#endif

using ExAddToQueue = void (*)(const std::function<void()> &);

struct MemData {
	std::uintptr_t applicationAddr;
	std::uintptr_t activeUserPtr;
	std::uintptr_t activeSessionPtr;
	ExAddToQueue addToQueue;
};

struct AyuPlugin {
	wchar_t name[128];
	wchar_t description[255];
	wchar_t moduleName[128];
	MemData memData;
};
