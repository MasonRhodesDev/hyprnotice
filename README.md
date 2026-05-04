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

## Status: v0.1.0 — D-Bus skeleton

What works:
- D-Bus server registers `org.freedesktop.Notifications` on the user session bus
- Implements `Notify`, `CloseNotification`, `GetCapabilities`, `GetServerInformation`
- Notifications stored in `CNotificationStore`; emits `NotificationClosed` on close
- `--version`, `--help`, `--verbose`, `--quiet` flags

What does NOT work yet:
- No popup rendering (you can `notify-send` and inspect via `dbus-monitor`, but nothing shows on screen)
- No inbox picker / CLI client
- No hyprlang config
- No lmtt theming integration
- No `ActionInvoked` wiring (the signal exists; nothing emits it yet)

## Roadmap

| Milestone | Scope |
|---|---|
| **v0.1** ✓ | D-Bus skeleton, store, build/lint/test scaffolding |
| **v0.2** | Popup rendering via hyprtoolkit layer-shell window. Per-monitor placement. Urgency-based styling. |
| **v0.3** | `hyprnotice-ctl` CLI for inbox listing/dismissal. Wire `ActionInvoked` from CLI invocation. |
| **v0.4** | `hyprlang` config for layout/timeouts/per-app rules. lmtt module + matugen template. |
| **v0.5** | Persistent history (write to `$XDG_RUNTIME_DIR` so survives daemon restart but not reboot). DND mode. |
| **v1.0** | Feature-parity with swaync inbox: action buttons, replies, grouping, sound. |

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
