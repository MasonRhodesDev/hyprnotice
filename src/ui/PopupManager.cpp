#include "PopupManager.hpp"

#include "../core/NotificationStore.hpp"
#include "../helpers/Log.hpp"

namespace HN {

    CPopupManager::CPopupManager(SP<Hyprtoolkit::IBackend> backend, CNotificationStore& store, CNotificationsService& notif)
        : m_backend(backend), m_store(store), m_notif(notif) {

        m_addedListener   = m_store.m_events.added.listen(
            [this](SP<SNotification> n) { onAdded(n); });
        m_updatedListener = m_store.m_events.updated.listen(
            [this](SP<SNotification> n) { onUpdated(n); });
        m_closedListener  = m_store.m_events.closed.listen(
            [this](uint32_t id, SNotification::eCloseReason r) { onClosed(id, r); });
    }

    CPopupManager::~CPopupManager() = default;

    void CPopupManager::onAdded(SP<SNotification> n) {
        // DND silences popups but keeps the notification in the store so it
        // shows up in `hyprnotice-ctl list` and the inbox picker. We mark the
        // state as INBOX up-front so the CLI doesn't lie about whether the
        // popup is on screen.
        if (m_store.dnd()) {
            Debug::log(Debug::TRACE, "popup-mgr: dnd active — id={} suppressed (inbox)", n->id);
            m_store.demote(n->id);
            return;
        }
        Debug::log(Debug::TRACE, "popup-mgr: spawning popup for id={}", n->id);
        m_popups.emplace(n->id, makeUnique<CPopupWindow>(m_backend, n, m_store, m_notif));
    }

    void CPopupManager::onUpdated(SP<SNotification> n) {
        auto it = m_popups.find(n->id);
        if (it == m_popups.end()) {
            // Was previously demoted to INBOX (no popup) — bring it back
            // visible only if its state requires it.
            if (n->state == SNotification::eState::VISIBLE) {
                m_popups.emplace(n->id, makeUnique<CPopupWindow>(m_backend, n, m_store, m_notif));
            }
            return;
        }

        if (n->state == SNotification::eState::INBOX) {
            // Demoted — close the popup window but keep the daemon-side
            // notification alive in the store.
            it->second->close();
            m_popups.erase(it);
            Debug::log(Debug::TRACE, "popup-mgr: demoted id={} to inbox", n->id);
        } else {
            it->second->rebuild(n);
        }
    }

    void CPopupManager::onClosed(uint32_t id, SNotification::eCloseReason /*reason*/) {
        auto it = m_popups.find(id);
        if (it == m_popups.end())
            return;
        it->second->close();
        m_popups.erase(it);
    }

}
