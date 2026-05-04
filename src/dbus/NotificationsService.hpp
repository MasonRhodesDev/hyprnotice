#pragma once

#include <memory>

#include <sdbus-c++/sdbus-c++.h>

#include "../core/NotificationStore.hpp"
#include "../helpers/Memory.hpp"

namespace HN {

    // Implements the org.freedesktop.Notifications interface on the well-known
    // bus name org.freedesktop.Notifications, object path /org/freedesktop/Notifications.
    //
    // Spec reference:
    //   https://specifications.freedesktop.org/notification-spec/latest/
    //
    // We model the bus via the v2 sdbus-c++ low-level API (vtable + signals)
    // because the high-level proxy generator is overkill for a five-method
    // service and we want explicit control over `NotificationClosed` /
    // `ActionInvoked` signal emission timing.
    class CNotificationsService {
      public:
        explicit CNotificationsService(sdbus::IConnection& bus, CNotificationStore& store);
        ~CNotificationsService();

        // Emit ActionInvoked(id, action_key) on the bus. Called by the UI
        // layer when a user clicks an action button, and by the inbox CLI
        // (via CInboxService::Invoke).
        void emitActionInvoked(uint32_t id, const std::string& actionKey);

        // Accessor for inbox/popup layers to look up sender info etc.
        CNotificationStore& store() { return m_store; }

      private:
        // ===== D-Bus method handlers =====
        // Notify(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout) -> u
        uint32_t onNotify(const std::string& appName, uint32_t replacesId,
                          const std::string& appIcon, const std::string& summary,
                          const std::string& body, const std::vector<std::string>& actions,
                          const std::map<std::string, sdbus::Variant>& hints,
                          int32_t expireTimeoutMs, sdbus::ObjectPath /*sender*/);

        // CloseNotification(id) -> void
        void     onCloseNotification(uint32_t id);

        // GetCapabilities() -> as
        std::vector<std::string> onGetCapabilities();

        // GetServerInformation() -> ssss (four separate strings, NOT a struct).
        // The spec lists name/vendor/version/spec_version as four return
        // values; libnotify parses them with `g_variant_get(reply, "(ssss)", ...)`
        // which is the auto-tuple wrapping D-Bus applies to *every* method
        // reply. If we return sdbus::Struct we'd wrap a struct *inside* that
        // tuple → "((ssss))" on the wire → libnotify sees null fields.
        std::tuple<std::string, std::string, std::string, std::string> onGetServerInformation();

        // ===== signal wiring =====
        sdbus::IConnection&                m_bus;
        CNotificationStore&                m_store;
        std::unique_ptr<sdbus::IObject>    m_obj;        // sdbus owns these by std::unique_ptr

        // Cleared on dtor; handles `closed` from the store and emits
        // NotificationClosed on D-Bus.
        Hyprutils::Signal::CHyprSignalListener m_closedListener;
    };

}
