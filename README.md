# hyprnotice

Notification daemon for Hyprland with an action-retaining inbox.

## What hyprnotice is

- A `org.freedesktop.Notifications` D-Bus server, like mako/dunst/swaync.
- **Keeps notifications alive after the popup expires.** The freedesktop spec lets clients (Slack, browsers, etc.) register actions on a notification — but those handlers go dead the moment the daemon emits `NotificationClosed`. mako and dunst close on timeout, so action buttons in their history are dead. swaync solves this by keeping notifications alive in an internal queue. hyprnotice does the same.
- Built on the hyprwm stack: hyprutils, hyprlang, sdbus-c++. C++23.
- Themed by [lmtt](https://github.com/MasonRhodesDev/linux-multi-theme-toggle) — the active palette is read from a generated config and re-rendered on theme switch.

## What hyprnotice is not

- Not portable beyond Hyprland (this is intentional — wlroots layer-shell + Hyprland conventions are baked in).
- Not a control center. The inbox is exposed via a CLI/IPC; an external picker (wofi/fuzzel) renders it.
- Not feature-complete. See the roadmap below.

## Status: v1.0.0 — persistent inbox, image-data rendering

What works:
- Full `org.freedesktop.Notifications` D-Bus server (Notify / CloseNotification / GetCapabilities / GetServerInformation)
- **`notify-send` works** — reply signature fix in v0.3 (was emitting `(ssss)` struct-wrapped, libnotify wanted plain `ssss`)
- **Layer-shell popups** via hyprtoolkit (top-right, OVERLAY, no kbd focus, auto-sized to content)
- **Action buttons** in popups — click invokes the action's D-Bus signal and closes the popup
- **Action retention via inbox state** — popup auto-close demotes (not closes), so the source app's action handlers stay alive. You can invoke a Slack notification's "Reply" from the inbox an hour after the popup vanished.
- **`org.hyprnotice.Inbox`** custom D-Bus interface for inbox introspection (List / Invoke / Dismiss / DismissAll + `Changed` signal)
- **`hyprnotice-ctl`** CLI: `list`, `invoke <id> [action]`, `dismiss <id>`, `dismiss-all`
- **lmtt theming** — popups read `~/.config/matugen/lmtt-colors.css` directly. SIGHUP triggers a re-read; an lmtt module sends the SIGHUP after every theme switch.
- **hyprlang config** at `~/.config/hypr/hyprnotice.conf` for popup geometry (anchor, layer, width, height, margins) and timing (default_timeout). Reloaded on SIGHUP. See `assets/example.conf`.
- **All actions render as buttons**, including the spec's `default` action — clicking the labeled button invokes its handler over D-Bus and dismisses.
- **Icons** in popups: `app_icon` strings are resolved as freedesktop icon names (via hyprtoolkit's `systemIcons()` icon-theme lookup), with a fall-through to absolute paths and `file://` URIs. Skipped when no icon is provided.
- **Do-not-disturb mode**: `hyprnotice-ctl mode dnd|none|toggle`. Notifications still arrive and appear in `hyprnotice-ctl list`/the inbox picker — only the popup is suppressed. Same D-Bus methods on `org.hyprnotice.Inbox` (`SetMode`, `GetMode`).
- **Urgency-based styling**: critical (urgency=2) gets a 2px red border + non-expiring; normal (urgency=1) gets a subtle outline; low (urgency=0) gets none. Colors from the lmtt palette (`error`, `outline_variant`).
- **Per-app rules** in `hyprnotice.conf`: stack `rule { app = …; timeout = …; skip_popup = … }` blocks for noisy/important apps. First substring match wins.
- **Persistent inbox** across daemon restart: `CNotificationStore` writes to `$XDG_RUNTIME_DIR/hyprnotice/store.json` every 10s + on graceful shutdown, replays the inbox on next start. Survives `systemctl restart` but not a full reboot (XDG_RUNTIME_DIR is wiped at logout — the right default for "old notifications I missed should disappear when I log back in").
- **`image-data` hint rendering**: raw ARGB/RGB pixel bytes from Discord/Telegram/etc. are encoded to PNG in-process via vendored [stb_image_write](https://github.com/nothings/stb) (single-header, public domain — no new system deps) and handed to hyprtoolkit's `CImageBuilder::data()`. Falls back to `app_icon` if encoding fails.
- **`install.sh`** — symlinks binaries into `/usr/local/bin`, drops a systemd user unit, disables predecessor mako D-Bus activation
- sdbus-c++ event loop integrated into hyprtoolkit's via `addFd`

What does NOT work yet:
- No body-click → default action (hyprtoolkit's IElement doesn't expose click handlers on Rectangle/Layout; "default" actions still appear as a button so the action can be invoked from the popup, and via `hyprnotice-ctl invoke <id>` from the inbox). Would require an upstream hyprtoolkit feature.
- No inline replies (no text-input element in hyprtoolkit yet). Reply-style notifications still render their reply *button* normally — clicking it fires `ActionInvoked` so the source app can pop its own reply UI.
- No sound playback (the spec's `sound-name` / `sound-file` hints are ignored). Most modern apps play their own notification sound; adding this would require linking libcanberra or shelling out to `paplay`. Easy to add when there's a real need.

## Roadmap

| Milestone | Scope |
|---|---|
| **v0.1** ✓ | D-Bus skeleton, store, build/lint/test scaffolding |
| **v0.2** ✓ | Popup rendering via hyprtoolkit layer-shell window. Auto-close demotes to inbox. |
| **v0.3** ✓ | Action buttons in popups. `hyprnotice-ctl` CLI for inbox listing/dismissal/action invocation. notify-send fix. |
| **v0.4** ✓ | lmtt theming via direct lmtt-colors.css read + SIGHUP reload. Install script + systemd unit. mako migration. |
| **v0.5** ✓ | hyprlang config for popup geometry/timing. "default" action rendered as button. |
| **v0.6** ✓ | Icons (path + freedesktop icon-theme lookup). DND mode. |
| **v0.7** ✓ | Urgency-based styling. Per-app rules (skip_popup, timeout overrides). |
| **v1.0** ✓ | Persistent inbox across daemon restart. image-data raw→PNG encoding via vendored stb_image_write. |
| **future** | Body-click default action (hyprtoolkit upstream). Inline replies (text-input element upstream). Sound (libcanberra). Grouping. |

## Building

```sh
cmake --no-warn-unused-cli -DCMAKE_BUILD_TYPE:STRING=Release \
      -DCMAKE_INSTALL_PREFIX:PATH=/usr -S . -B ./build
cmake --build ./build --config Release --target all -j$(nproc)
```

Dependencies (Fedora):

```sh
sudo dnf install hyprutils-devel hyprlang-devel sdbus-cpp-devel cmake gcc-c++ pkgconf-pkg-config
```

## Configuring

Drop a config at `~/.config/hypr/hyprnotice.conf`. Every value has a baked-in default; the file is optional. See [`assets/example.conf`](assets/example.conf) for the full list of keys.

```
popup {
    anchor = top right
    width  = 360
    default_timeout = 5000
}
```

Reload with `pkill -HUP hyprnotice` (or just switch themes — lmtt's hyprnotice module sends the signal after rendering colors).

## Installing

```sh
cmake -B build && cmake --build build -j$(nproc)
./install.sh   # symlinks into /usr/local/bin, drops systemd unit, disables mako
```

## Running

The installer enables `hyprnotice.service` on `graphical-session.target` so the daemon comes up at login. Manual start:

```sh
systemctl --user start hyprnotice.service
journalctl --user -u hyprnotice.service -f
```

`hyprnotice` claims `org.freedesktop.Notifications` on the session bus, so any other notification daemon (mako, dunst, swaync) must be stopped first. The installer disables mako's D-Bus auto-activation; for swaync use `systemctl --user disable --now swaync.service`.

Test from another terminal:

```sh
notify-send "hyprnotice test" "popup at top-right"
hyprnotice-ctl list
hyprnotice-ctl dismiss-all
```

Send a notification with action buttons:

```sh
gdbus call --session \
  --dest org.freedesktop.Notifications \
  --object-path /org/freedesktop/Notifications \
  --method org.freedesktop.Notifications.Notify \
  '"app"' 'uint32 0' '""' '"summary"' '"body"' \
  '["reply", "Reply", "mark-read", "Mark Read"]' \
  '@a{sv} {}' 'int32 0'
```

Persistent (`expire_timeout=0`) so the popup stays until you click a button or
dismiss via `hyprnotice-ctl dismiss <id>`.
