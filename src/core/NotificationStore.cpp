#include "NotificationStore.hpp"

#include "../helpers/Log.hpp"

namespace HN {

    uint32_t CNotificationStore::accept(SNotification&& n) {
        if (n.replaceId != 0 && m_byId.contains(n.replaceId)) {
            auto&    slot = m_byId[n.replaceId];
            const auto id = slot->id;
            const auto sender = slot->sender;          // sender doesn't change on replace
            *slot              = std::move(n);
            slot->id           = id;
            slot->sender       = sender;
            m_events.updated.emit(slot);
            Debug::log(Debug::LOG, "store: replaced id={}", id);
            return id;
        }

        n.id = m_nextId++;
        if (n.id == 0)         // wrap-around guard; id 0 is reserved
            n.id = m_nextId++;
        auto sp = makeShared<SNotification>(std::move(n));
        m_byId.emplace(sp->id, sp);
        m_events.added.emit(sp);
        Debug::log(Debug::LOG, "store: accepted id={} app=\"{}\" summary=\"{}\"",
                   sp->id, sp->appName, sp->summary);
        return sp->id;
    }

    void CNotificationStore::close(uint32_t id, SNotification::eCloseReason reason) {
        auto it = m_byId.find(id);
        if (it == m_byId.end()) {
            Debug::log(Debug::TRACE, "store: close({}) — unknown id", id);
            return;
        }
        it->second->state = SNotification::eState::CLOSED;
        m_events.closed.emit(id, reason);
        m_byId.erase(it);
        Debug::log(Debug::LOG, "store: closed id={} reason={}", id, static_cast<uint32_t>(reason));
    }

    void CNotificationStore::demote(uint32_t id) {
        auto it = m_byId.find(id);
        if (it == m_byId.end())
            return;
        if (it->second->state == SNotification::eState::VISIBLE) {
            it->second->state = SNotification::eState::INBOX;
            m_events.updated.emit(it->second);
        }
    }

    SP<SNotification> CNotificationStore::get(uint32_t id) const {
        auto it = m_byId.find(id);
        return it != m_byId.end() ? it->second : SP<SNotification>{};
    }

    std::vector<SP<SNotification>> CNotificationStore::snapshot() const {
        std::vector<SP<SNotification>> out;
        out.reserve(m_byId.size());
        for (const auto& [_, n] : m_byId)
            out.push_back(n);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            return a->id > b->id;       // newest first
        });
        return out;
    }

}
