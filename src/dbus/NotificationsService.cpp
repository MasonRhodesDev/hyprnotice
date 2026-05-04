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

    namespace {
        // Try-extract a Variant value as type T; returns nullopt if either
        // the variant is the wrong type or get<>() throws.
        template <typename T>
        std::optional<T> tryGet(const sdbus::Variant& v) {
            try {
                return v.get<T>();
            } catch (...) {
                return std::nullopt;
            }
        }

        SNotification::SImageData parseImageData(const sdbus::Variant& v) {
            // Spec: hint value is a struct (iiibii ay) — width, height,
            // rowstride, has_alpha, bits_per_sample, channels, data.
            using Tuple = sdbus::Struct<int32_t, int32_t, int32_t, bool, int32_t, int32_t,
                                        std::vector<uint8_t>>;
            auto t = tryGet<Tuple>(v);
            if (!t)
                return {};
            return {
                .width         = std::get<0>(*t),
                .height        = std::get<1>(*t),
                .rowstride     = std::get<2>(*t),
                .hasAlpha      = std::get<3>(*t),
                .bitsPerSample = std::get<4>(*t),
                .channels      = std::get<5>(*t),
                .data          = std::move(std::get<6>(*t)),
            };
        }
    }

    uint32_t CNotificationsService::onNotify(const std::string& appName, uint32_t replacesId,
                                             const std::string& appIcon, const std::string& summary,
                                             const std::string& body,
                                             const std::vector<std::string>& actions,
                                             const std::map<std::string, sdbus::Variant>& hints,
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

        // Hints we honor. Spec lists more (sound, x/y, transient, …); add as
        // they become useful.
        if (auto it = hints.find("urgency"); it != hints.end()) {
            if (auto u = tryGet<uint8_t>(it->second))
                n.urgency = (*u >= 2) ? eUrgency::CRITICAL :
                            (*u == 1) ? eUrgency::NORMAL : eUrgency::LOW;
        }
        // Modern apps use "image-data"; legacy apps "icon_data". Same format.
        if (auto it = hints.find("image-data"); it != hints.end())
            n.imageData = parseImageData(it->second);
        else if (auto it2 = hints.find("icon_data"); it2 != hints.end())
            n.imageData = parseImageData(it2->second);

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

    std::tuple<std::string, std::string, std::string, std::string>
    CNotificationsService::onGetServerInformation() {
        return {"hyprnotice", "hyprwm-community", HYPRNOTICE_VERSION, "1.2"};
    }

}
