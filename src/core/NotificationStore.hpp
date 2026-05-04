#pragma once

#include <cstdint>
#include <unordered_map>

#include <hyprutils/signal/Signal.hpp>

#include "Notification.hpp"
#include "../helpers/Memory.hpp"

namespace HN {

    // The single in-memory store of every notification this daemon has accepted
    // and not yet closed. Two design properties matter:
    //
    //   1. Notifications stay in this map until the user explicitly closes
    //      them — even after they leave the on-screen popup queue. This is
    //      what keeps action handlers alive on the sending app's side.
    //   2. IDs are monotonically increasing from 1 (the spec says id 0 is
    //      reserved). On replaceId != 0, we mutate the existing slot in place
    //      so that the sender keeps its handle.
    class CNotificationStore {
      public:
        // Persistence: write/read JSON snapshot to $XDG_RUNTIME_DIR/hyprnotice/store.json
        // (or $TMPDIR fallback). Called on graceful shutdown and at startup
        // so notifications survive a `systemctl restart hyprnotice`. We
        // intentionally use XDG_RUNTIME_DIR rather than XDG_DATA_HOME so the
        // store does NOT survive a full reboot — old notifications losing
        // relevance after a logout is the right default.
        void saveToDisk() const;
        void loadFromDisk();

        // Insert or replace. Returns the assigned id.
        uint32_t accept(SNotification&& n);

        // Mark closed, fire `closed` signal, drop from map.
        // reason is propagated to the NotificationClosed D-Bus signal.
        void close(uint32_t id, SNotification::eCloseReason reason);

        // Move VISIBLE → INBOX without closing.
        void demote(uint32_t id);

        SP<SNotification> get(uint32_t id) const;

        // Snapshot of the inbox, sorted newest-first. Caller-owned copies so
        // iteration is safe across UI updates.
        std::vector<SP<SNotification>> snapshot() const;

        // Do-not-disturb mode. When true, accept() still records the
        // notification (it shows up in the inbox CLI as usual) but the popup
        // manager skips the on-screen popup. Toggled via the inbox D-Bus
        // SetMode method or `hyprnotice-ctl mode`.
        bool dnd() const { return m_dnd; }
        void setDnd(bool v);

        struct {
            // (id, reason) — emit NotificationClosed on D-Bus from the server.
            Hyprutils::Signal::CSignalT<uint32_t, SNotification::eCloseReason> closed;

            // (notification) — UI subscribers redraw the popup queue & inbox.
            Hyprutils::Signal::CSignalT<SP<SNotification>>                     added;
            Hyprutils::Signal::CSignalT<SP<SNotification>>                     updated;

            // (newDnd) — emitted when DND state flips. Inbox CLI watchers
            // surface this via the existing Changed signal.
            Hyprutils::Signal::CSignalT<bool>                                  modeChanged;
        } m_events;

      private:
        uint32_t                                          m_nextId = 1;
        std::unordered_map<uint32_t, SP<SNotification>>   m_byId;
        bool                                              m_dnd    = false;
    };

}
