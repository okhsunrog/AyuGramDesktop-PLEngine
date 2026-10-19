# PLEngine channel reader

Read-only AyuGram PLEngine plugin that polls recent posts from a configurable
list of public Telegram channels. It reuses the already authorized AyuGram
session and never exposes the session auth key.

The plugin only sends these MTProto requests:

- `contacts.resolveUsername`;
- `messages.getHistory`.

It does not send, edit, delete, or mark messages as read.

## Configuration

Copy `channel-reader.example.json` to:

```text
~/.config/AyuGramDesktop/channel-reader.json
```

The path can be overridden with `AYUGRAM_CHANNEL_READER_CONFIG`.

- `channels`: public channel usernames, with or without `@`;
- `history_limit`: number of recent posts fetched per poll, from 1 to 100;
- `poll_interval_seconds`: polling interval, from 10 to 86400 seconds;
- `output_path`: optional JSONL destination. An empty value uses
  `~/.local/share/AyuGramDesktop/plugins/channel-reader-posts.jsonl`.

The status file is written next to the default output as
`channel-reader-status.json`. Output and status files are owner-only.

Each JSONL record uses schema version 2. `links` preserves visible URL
entities, hidden text URLs, inline URL buttons, and web-page preview URLs:

```json
{
  "schema_version": 2,
  "channel_id": 123,
  "channel_username": "example",
  "channel_title": "Example",
  "message_id": 42,
  "posted_at": "2026-07-28T12:00:00Z",
  "edited_at": null,
  "text": "Original post text",
  "links": [
    {
      "kind": "inline_button",
      "url": "https://example.com/apply/42",
      "text": "Apply"
    }
  ],
  "source_url": "https://t.me/example/42"
}
```

Duplicates can appear after AyuGram restarts. Consumers must use
`(channel_id, message_id)` or `(channel_username, message_id)` as the
idempotency key.

## Build and install

Point `AYUGRAM_BUILD_DIR` at a configured build matching the installed AyuGram:

```bash
cmake -S . -B build -G Ninja \
  -DAYUGRAM_BUILD_DIR=/path/to/ayugram-build
cmake --build build
install -Dm755 \
  build/ayu-plengine-channel-reader.so \
  ~/.local/share/AyuGramDesktop/plugins/ayu-plengine-channel-reader.so
```

Restart AyuGram, then inspect:

```bash
cat ~/.local/share/AyuGramDesktop/plugins/channel-reader-status.json
tail ~/.local/share/AyuGramDesktop/plugins/channel-reader-posts.jsonl
```
