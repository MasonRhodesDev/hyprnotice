#pragma once

#include <memory>

#include <sdbus-c++/sdbus-c++.h>

#include "../core/NotificationStore.hpp"
#include "../helpers/Memory.hpp"

namespace HN {

    class CNotificationsService;

    // Custom interface for the hyprnotice-ctl CLI to query and act on the
    // inbox. The freedesktop Notifications spec only covers the *sender* side
    // (apps publishing notifications); there's no standard for an inbox UI to
    // enumerate pending notifications, so we expose a small org.hyprnotice
    // interface alongside.
    //
    // Bus name: same daemon (org.freedesktop.Notifications)
    // Path:     /org/hyprnotice/Inbox
    // Interface: org.hyprnotice.Inbox
    class CInboxService {
      public:
        CInboxService(sdbus::IConnection& bus, CNotificationStore& store, CNotificationsService& notif);
        ~CInboxService();

      private:
        // org.hyprnotice.Inbox.List() -> a(ussss)
        //   array of (id, app_name, summary, body, state) for every entry
        //   currently in the store. state is one of "visible"|"inbox".
        std::vector<sdbus::Struct<uint32_t, std::string, std::string, std::string, std::string>> onList();

        // Invoke(id, action_key) — emit ActionInvoked on the freedesktop iface
        // (so the source app reacts), then close the notification.
        void onInvoke(uint32_t id, const std::string& actionKey);

        // Dismiss(id) — close with DISMISSED reason.
        void onDismiss(uint32_t id);

        // DismissAll() — close all entries.
        void onDismissAll();

        // SetMode(s) where s is "none" | "dnd" | "toggle".
        void        onSetMode(const std::string& mode);
        // GetMode() -> "none" | "dnd"
        std::string onGetMode();

        sdbus::IConnection&             m_bus;
        CNotificationStore&             m_store;
        CNotificationsService&          m_notif;
        std::unique_ptr<sdbus::IObject> m_obj;

        Hyprutils::Signal::CHyprSignalListener m_addedListener;
        Hyprutils::Signal::CHyprSignalListener m_updatedListener;
        Hyprutils::Signal::CHyprSignalListener m_closedListener;
    };

}
