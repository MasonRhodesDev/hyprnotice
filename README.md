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

## Status: v0.2.0 — popups render

What works:
- D-Bus server registers `org.freedesktop.Notifications` on the user session bus
- Implements `Notify`, `CloseNotification`, `GetCapabilities`, `GetServerInformation`
- Notifications stored in `CNotificationStore`; emits `NotificationClosed` on close
- **Layer-shell popups rendered via hyprtoolkit** (top-right, OVERLAY layer, no kbd focus)
- Popup auto-closes via configurable timeout (default 5s, `expire_timeout=0` keeps it persistent)
- **Auto-close demotes to INBOX, doesn't emit NotificationClosed** — sender's action handlers stay alive for the inbox UI to invoke later
- sdbus-c++ event loop integrated into hyprtoolkit's via `addFd`
- `--version`, `--help`, `--verbose`, `--quiet` flags

What does NOT work yet:
- No icon rendering (popups are text-only summary + body)
- No action buttons in popups
- No inbox picker / `hyprnotice-ctl` CLI
- No hyprlang config (layout, default-timeout, anchor, per-app rules)
- No lmtt theming integration (uses hyprtoolkit's default palette)
- No `ActionInvoked` wiring beyond the signal definition

## Roadmap

| Milestone | Scope |
|---|---|
| **v0.1** ✓ | D-Bus skeleton, store, build/lint/test scaffolding |
| **v0.2** ✓ | Popup rendering via hyprtoolkit layer-shell window. Auto-close demotes to inbox. |
| **v0.3** | Action buttons in popups. Icons. `hyprnotice-ctl` CLI for inbox listing/dismissal/action invocation. |
| **v0.4** | `hyprlang` config for layout/timeouts/per-app rules. lmtt module + matugen template. |
| **v0.5** | Persistent history (write to `$XDG_RUNTIME_DIR` so survives daemon restart but not reboot). DND mode. Per-monitor placement. |
| **v1.0** | Feature-parity with swaync inbox: replies, grouping, sound, urgency-based styling. |

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

## Running

The daemon claims `org.freedesktop.Notifications` on the session bus, so any other notification daemon (mako, dunst, swaync) **must be stopped first**:

```sh
systemctl --user stop mako.service swaync.service 2>/dev/null
./build/hyprnotice --verbose
```

Test from another terminal:

```sh
notify-send "hyprnotice test" "if this prints to stderr above, the D-Bus server is alive"
```
