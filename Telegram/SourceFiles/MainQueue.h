#include <queue>
#include <functional>
#include <mutex>

#ifdef _WIN32
#define AYUPL_EXPORT __declspec(dllexport)
#else
#define AYUPL_EXPORT __attribute__((visibility("default")))
#endif

extern std::queue<std::function<void()>> MainQueue;
extern std::mutex queueMutex;

AYUPL_EXPORT std::queue<std::function<void()>> *GetQueuePtr();

AYUPL_EXPORT void AddToQueue(const std::function<void()> &func);
