# Control socket

`$XDG_RUNTIME_DIR/touch2tablet/control.sock` (`--socket`), user-only. Newline-delimited JSON,
optional `"id"` echoed back. `touch2tabletd --no-device` serves it without hardware.

```bash
printf '{"op":"get"}\n' | nc -U "$XDG_RUNTIME_DIR/touch2tablet/control.sock"
```

| request | reply |
|---|---|
| `{"op":"get"}` | `settings`, `info` (connected, raw range, panel name + mm, node paths, `proto`) |
| `{"op":"apply","settings":{partial}}` | merged + sanitized, live, not persisted |
| `{"op":"save","settings":{partial}}` | apply + write the settings file atomically |
| `{"op":"reset"}` | defaults, not persisted |
| `{"op":"log"}` | last 300 daemon log lines |
| `{"op":"subscribe"}` | `{"proto":1}` then a stream of `{"ev":"pos"}` (≤120 Hz: raw/mm/out/fingers/inside/ignored), `log`, `settings`, `device` |
