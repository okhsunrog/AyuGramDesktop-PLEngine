# PLEngine Linux smoke plugin

This plugin verifies the native Linux PLEngine loader without reading Telegram
messages, account data, or authorization keys.

It records:

- whether PLEngine supplied the application, active user, and active session
  pointers;
- whether PLEngine supplied `addToQueue`;
- the loader-thread ID;
- the thread ID used for the queued main-thread callback.

Build and install:

```bash
cmake -S . -B build -G Ninja
cmake --build build
install -Dm755 \
  build/ayu-plengine-smoke-plugin.so \
  ~/.local/share/AyuGramDesktop/plugins/ayu-plengine-smoke-plugin.so
```

Restart AyuGram and inspect:

```bash
cat ~/.local/share/AyuGramDesktop/plugins/ayu-plengine-smoke-plugin.marker
```
