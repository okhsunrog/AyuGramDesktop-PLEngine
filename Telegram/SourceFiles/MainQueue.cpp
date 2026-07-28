#include "MainQueue.h"

std::queue<std::function<void()>> MainQueue;
std::mutex queueMutex;
