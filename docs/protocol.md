# Control Socket

The daemon listens on `$XDG_RUNTIME_DIR/touch2tablet/control.sock` by default;
`--socket` selects another path. The socket is user-only and carries one JSON
object per line. Every request may include an `"id"`, which is echoed in its
reply. Successful replies contain `"ok":true`; errors contain `"ok":false` and
an `"error"` string.

`touch2tabletd --no-device` serves the socket without opening touch hardware.

```bash
printf '{"op":"get"}\n' | nc -U "$XDG_RUNTIME_DIR/touch2tablet/control.sock"
```

## Requests

The `settings` values below use the [settings schema](../README.md#settings-and-presets).

| Request | Reply and behavior |
|---|---|
| `{"op":"get"}` | Current `settings` and daemon `info` |
| `{"op":"apply","settings":{...}}` | Merges and sanitizes the partial settings object, applies it live, and returns the resulting `settings`; does not persist it |
| `{"op":"save","settings":{...}}` | Applies the partial settings, writes the complete result atomically, and returns the resulting `settings` |
| `{"op":"reset"}` | Applies and returns the defaults; does not persist them |
| `{"op":"log"}` | The last 300 daemon log entries in `lines` |
| `{"op":"subscribe"}` | Returns `proto`, `settings`, `info`, and recent `lines`, then streams events on the same connection |

The `info` object contains `connected`, `touch_path`, `raw`, `panel`,
`tablet_path`, `settings_path`, and `proto`. Device-specific fields are `null`
while no panel is connected.

## Subscription Events

| Event | Contents |
|---|---|
| `pos` | At most 120 updates per second while touched: `fingers`, `raw`, `mm`, `out`, `inside`, and `ignored`. The release event contains `fingers: 0`. |
| `log` | A new daemon log `line` |
| `settings` | Updated `settings` and `info` |
| `device` | Updated `info` after a panel connects or disconnects |
