#pragma once

#include <chrono>

#include <hyprtoolkit/core/Backend.hpp>
#include <hyprtoolkit/window/Window.hpp>
#include <hyprutils/memory/Atomic.hpp>

#include "../core/Notification.hpp"
#include "../helpers/Memory.hpp"

namespace HN {

    class CNotificationStore;
    class CNotificationsService;

    // One layer-shell popup window per VISIBLE notification. The window
    // auto-closes after the notification's expireTimeoutMs (or a daemon
    // default), at which point the store is told to *demote* the
    // notification to INBOX — not close it. Demotion preserves the
    // sender's action handlers (the freedesktop NotificationClosed signal
    // is NOT emitted) so users can act on the notification later from the
    // inbox picker.
    class CPopupWindow {
      public:
        CPopupWindow(SP<Hyprtoolkit::IBackend> backend,
                     SP<SNotification>         notification,
                     CNotificationStore&       store,
                     CNotificationsService&    notif);
        ~CPopupWindow();

        uint32_t id() const { return m_id; }

        // Re-render content for an in-place replace (Notify with replaces_id).
        void rebuild(SP<SNotification> updated);

        // Tear down the window now (without demoting).
        void close();

      private:
        void                                buildContent();
        void                                scheduleAutoClose(int32_t ms);

        SP<Hyprtoolkit::IBackend>           m_backend;
        SP<SNotification>                   m_notif;
        CNotificationStore&                 m_store;
        CNotificationsService&              m_notifService;
        uint32_t                            m_id = 0;

        SP<Hyprtoolkit::IWindow>            m_window;
        Hyprutils::Memory::CAtomicSharedPointer<Hyprtoolkit::CTimer> m_autoCloseTimer;
    };

}
