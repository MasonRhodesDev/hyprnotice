#include "NotificationsService.hpp"

#include "../helpers/Log.hpp"

namespace HN {

    static constexpr const char* kBusName    = "org.freedesktop.Notifications";
    static constexpr const char* kObjectPath = "/org/freedesktop/Notifications";
    static constexpr const char* kInterface  = "org.freedesktop.Notifications";

    CNotificationsService::CNotificationsService(sdbus::IConnection& bus, CNotificationStore& store)
        : m_bus(bus), m_store(store) {
        m_obj = sdbus::createObject(m_bus, sdbus::ObjectPath{kObjectPath});

        m_obj->addVTable(
            sdbus::registerMethod("Notify").implementedAs(
                [this](const std::string& app, uint32_t rid,
                       const std::string& icon, const std::string& sum,
                       const std::string& body,
                       const std::vector<std::string>& acts,
                       const std::map<std::string, sdbus::Variant>& hints,
                       int32_t timeout) -> uint32_t {
                    return onNotify(app, rid, icon, sum, body, acts, hints, timeout, sdbus::ObjectPath{});
                }),
            sdbus::registerMethod("CloseNotification").implementedAs(
                [this](uint32_t id) { onCloseNotification(id); }),
            sdbus::registerMethod("GetCapabilities").implementedAs(
                [this] { return onGetCapabilities(); }),
            sdbus::registerMethod("GetServerInformation").implementedAs(
                [this] { return onGetServerInformation(); }),
            sdbus::registerSignal("NotificationClosed")
                .withParameters<uint32_t, uint32_t>("id", "reason"),
            sdbus::registerSignal("ActionInvoked")
                .withParameters<uint32_t, std::string>("id", "action_key")
        ).forInterface(sdbus::InterfaceName{kInterface});

        // Bridge store's `closed` to the D-Bus signal.
        m_closedListener = m_store.m_events.closed.listen(
            [this](uint32_t id, SNotification::eCloseReason reason) {
                m_obj->emitSignal("NotificationClosed")
                    .onInterface(sdbus::InterfaceName{kInterface})
                    .withArguments(id, static_cast<uint32_t>(reason));
            });

        m_bus.requestName(sdbus::ServiceName{kBusName});
        Debug::log(Debug::INFO, "D-Bus: claimed name {}", kBusName);
    }

    CNotificationsService::~CNotificationsService() {
        try {
            m_bus.releaseName(sdbus::ServiceName{kBusName});
        } catch (...) {
            // best-effort; bus may already be torn down
        }
    }

    void CNotificationsService::emitActionInvoked(uint32_t id, const std::string& key) {
        m_obj->emitSignal("ActionInvoked")
            .onInterface(sdbus::InterfaceName{kInterface})
            .withArguments(id, key);
    }

    uint32_t CNotificationsService::onNotify(const std::string& appName, uint32_t replacesId,
                                             const std::string& appIcon, const std::string& summary,
                                             const std::string& body,
                                             const std::vector<std::string>& actions,
                                             const std::map<std::string, sdbus::Variant>& /*hints*/,
                                             int32_t expireTimeoutMs, sdbus::ObjectPath /*sender*/) {
        SNotification n{
            .replaceId       = replacesId,
            .appName         = appName,
            .appIcon         = appIcon,
            .summary         = summary,
            .body            = body,
            .actions         = actions,
            .expireTimeoutMs = expireTimeoutMs,
            .received        = std::chrono::steady_clock::now(),
        };
        return m_store.accept(std::move(n));
    }

    void CNotificationsService::onCloseNotification(uint32_t id) {
        m_store.close(id, SNotification::eCloseReason::CLOSED_BY_API);
    }

    std::vector<std::string> CNotificationsService::onGetCapabilities() {
        // Minimal honest set for v0.1. Add as features land:
        //   "actions", "action-icons", "body", "body-hyperlinks", "body-images",
        //   "body-markup", "icon-multi", "icon-static", "persistence", "sound".
        return {"body", "persistence"};
    }

    sdbus::Struct<std::string, std::string, std::string, std::string>
    CNotificationsService::onGetServerInformation() {
        return sdbus::Struct{std::string{"hyprnotice"},
                             std::string{"hyprwm-community"},
                             std::string{HYPRNOTICE_VERSION},
                             std::string{"1.2"}};
    }

}
