#include "InboxService.hpp"

#include "NotificationsService.hpp"
#include "../helpers/Log.hpp"

namespace HN {

    static constexpr const char* kObjectPath = "/org/hyprnotice/Inbox";
    static constexpr const char* kInterface  = "org.hyprnotice.Inbox";

    CInboxService::CInboxService(sdbus::IConnection& bus, CNotificationStore& store, CNotificationsService& notif)
        : m_bus(bus), m_store(store), m_notif(notif) {

        m_obj = sdbus::createObject(m_bus, sdbus::ObjectPath{kObjectPath});

        m_obj->addVTable(
            sdbus::registerMethod("List").implementedAs(
                [this] { return onList(); }),
            sdbus::registerMethod("Invoke").implementedAs(
                [this](uint32_t id, const std::string& key) { onInvoke(id, key); }),
            sdbus::registerMethod("Dismiss").implementedAs(
                [this](uint32_t id) { onDismiss(id); }),
            sdbus::registerMethod("DismissAll").implementedAs(
                [this] { onDismissAll(); }),
            sdbus::registerSignal("Changed").withParameters<>()
        ).forInterface(sdbus::InterfaceName{kInterface});

        // Bridge store events → Changed signal so a watching CLI can re-list
        // without polling.
        const auto emitChanged = [this] {
            m_obj->emitSignal("Changed").onInterface(sdbus::InterfaceName{kInterface});
        };
        m_addedListener   = m_store.m_events.added.listen([emitChanged](SP<SNotification>) { emitChanged(); });
        m_updatedListener = m_store.m_events.updated.listen([emitChanged](SP<SNotification>) { emitChanged(); });
        m_closedListener  = m_store.m_events.closed.listen(
            [emitChanged](uint32_t, SNotification::eCloseReason) { emitChanged(); });
    }

    CInboxService::~CInboxService() = default;

    std::vector<sdbus::Struct<uint32_t, std::string, std::string, std::string, std::string>>
    CInboxService::onList() {
        std::vector<sdbus::Struct<uint32_t, std::string, std::string, std::string, std::string>> out;
        for (const auto& n : m_store.snapshot()) {
            const char* state = (n->state == SNotification::eState::INBOX) ? "inbox" : "visible";
            out.emplace_back(sdbus::Struct<uint32_t, std::string, std::string, std::string, std::string>{
                n->id, n->appName, n->summary, n->body, std::string{state}});
        }
        return out;
    }

    void CInboxService::onInvoke(uint32_t id, const std::string& actionKey) {
        if (!m_store.get(id)) {
            Debug::log(Debug::WARN, "inbox: invoke({}) — unknown id", id);
            return;
        }
        Debug::log(Debug::INFO, "inbox: invoke id={} action=\"{}\"", id, actionKey);
        // Tell the source app to act. After this returns, the sender will
        // typically close the notification itself, but we also dismiss to
        // ensure the inbox stays in sync.
        m_notif.emitActionInvoked(id, actionKey);
        m_store.close(id, SNotification::eCloseReason::DISMISSED);
    }

    void CInboxService::onDismiss(uint32_t id) {
        Debug::log(Debug::INFO, "inbox: dismiss id={}", id);
        m_store.close(id, SNotification::eCloseReason::DISMISSED);
    }

    void CInboxService::onDismissAll() {
        const auto snap = m_store.snapshot();
        Debug::log(Debug::INFO, "inbox: dismiss-all ({} entries)", snap.size());
        for (const auto& n : snap)
            m_store.close(n->id, SNotification::eCloseReason::DISMISSED);
    }

}
