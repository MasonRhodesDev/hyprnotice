#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace HN {

    // Mirror of the org.freedesktop.Notifications Notify() arguments plus
    // server-side bookkeeping fields. Stored in CNotificationStore so that the
    // sender's D-Bus connection stays "live" — we don't emit NotificationClosed
    // when a notification merely scrolls off-screen, only when the user
    // explicitly dismisses it. This is what allows actions (Slack's "Reply",
    // browser deep links, etc.) to keep working from the inbox.
    struct SNotification {
        uint32_t id        = 0;       // server-assigned, monotonic from 1
        uint32_t replaceId = 0;       // 0 = new; nonzero = replaces an existing id
        std::string appName;
        std::string appIcon;          // freedesktop icon name or path
        std::string summary;
        std::string body;
        std::vector<std::string> actions;     // [key, label, key, label, ...]
        int32_t expireTimeoutMs = -1;         // -1 = server default; 0 = persistent
        std::string sender;                   // D-Bus unique name of the sending app
        std::chrono::steady_clock::time_point received;

        // Inbox state.
        enum class eState : uint8_t {
            VISIBLE,         // currently rendered as a popup
            INBOX,           // moved off-screen, still alive (action handlers retained)
            CLOSED,          // dismissed by user; awaiting NotificationClosed emit
        };
        eState state = eState::VISIBLE;

        // Reasons for NotificationClosed signal (per spec).
        enum class eCloseReason : uint32_t {
            EXPIRED       = 1,   // we don't currently use this — TTL expiry keeps
                                 // notifications in inbox instead of closing them
            DISMISSED     = 2,   // user dismissed
            CLOSED_BY_API = 3,   // CloseNotification() called via D-Bus
            UNDEFINED     = 4,
        };
    };

}
