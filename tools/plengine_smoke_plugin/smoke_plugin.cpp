#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <string>
#include <string_view>
#include <sys/syscall.h>
#include <unistd.h>

#include "plengine/plugin_api.h"

static_assert(sizeof(MemData) == 32);
static_assert(offsetof(AyuPlugin, memData) == 2048);
static_assert(sizeof(AyuPlugin) == 2080);

namespace {

auto Plugin = AyuPlugin{
    L"PLEngine Linux Smoke Test",
    L"Verifies Linux plugin loading and main-thread queue dispatch.",
    L"ayu-plengine-smoke-plugin.so",
    {},
};

[[nodiscard]] auto MarkerPath() -> std::string {
  const auto dataHome = std::getenv("XDG_DATA_HOME");
  if (dataHome && *dataHome) {
    return std::string(dataHome) +
           "/AyuGramDesktop/plugins/ayu-plengine-smoke-plugin.marker";
  }
  const auto home = std::getenv("HOME");
  if (home && *home) {
    return std::string(home) + "/.local/share/AyuGramDesktop/plugins/"
                               "ayu-plengine-smoke-plugin.marker";
  }
  return "/tmp/ayu-plengine-smoke-plugin.marker";
}

[[nodiscard]] auto ThreadId() -> long { return syscall(SYS_gettid); }

void AppendMarker(std::string_view stage) {
  const auto line =
      std::string(stage) + " pid=" + std::to_string(getpid()) +
      " tid=" + std::to_string(ThreadId()) + " application=" +
      (Plugin.memData.applicationAddr ? "present" : "missing") +
      " user=" + (Plugin.memData.activeUserPtr ? "present" : "missing") +
      " session=" + (Plugin.memData.activeSessionPtr ? "present" : "missing") +
      " queue=" + (Plugin.memData.addToQueue ? "present" : "missing") + "\n";
  const auto path = MarkerPath();
  const auto descriptor =
      open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
  if (descriptor < 0) {
    return;
  }

  auto written = std::size_t(0);
  while (written < line.size()) {
    const auto result =
        write(descriptor, line.data() + written, line.size() - written);
    if (result > 0) {
      written += result;
    } else if (result < 0 && errno == EINTR) {
      continue;
    } else {
      break;
    }
  }
  close(descriptor);
}

} // namespace

extern "C" __attribute__((visibility("default"))) auto pluginInfo()
    -> AyuPlugin * {
  return &Plugin;
}

extern "C" __attribute__((visibility("default"))) void internalSetup() {
  AppendMarker("internalSetup");
  if (const auto addToQueue = Plugin.memData.addToQueue) {
    addToQueue([] { AppendMarker("mainQueueCallback"); });
  }
}
