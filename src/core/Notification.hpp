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
    // Urgency mirrors the freedesktop "urgency" hint (byte: 0 low, 1 normal, 2 critical).
    enum class eUrgency : uint8_t { LOW = 0, NORMAL = 1, CRITICAL = 2 };

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

        eUrgency urgency = eUrgency::NORMAL;

        // Raw image bytes from the freedesktop "image-data" / legacy
        // "icon_data" hint. Format: ARGB / RGBA / RGB / etc., described by
        // the (width, height, rowstride, hasAlpha, bitsPerSample, channels)
        // tuple. Empty data means "use appIcon instead".
        struct SImageData {
            int32_t              width         = 0;
            int32_t              height        = 0;
            int32_t              rowstride     = 0;
            bool                 hasAlpha      = false;
            int32_t              bitsPerSample = 0;
            int32_t              channels      = 0;
            std::vector<uint8_t> data;
            bool                 empty() const { return data.empty(); }
        };
        SImageData imageData;

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
