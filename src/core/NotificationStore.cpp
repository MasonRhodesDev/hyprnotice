#include "NotificationStore.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

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

    void CNotificationStore::setDnd(bool v) {
        if (m_dnd == v)
            return;
        m_dnd = v;
        Debug::log(Debug::INFO, "store: dnd={}", v ? "on" : "off");
        m_events.modeChanged.emit(v);
    }

    namespace {
        std::filesystem::path persistencePath() {
            const char* runtime = std::getenv("XDG_RUNTIME_DIR");
            if (!runtime || !*runtime) runtime = std::getenv("TMPDIR");
            if (!runtime || !*runtime) runtime = "/tmp";
            return std::filesystem::path{runtime} / "hyprnotice" / "store.json";
        }

        // Tiny ad-hoc JSON escaper. The full glaze dependency would be
        // overkill for the four scalar types we serialise here.
        std::string esc(std::string_view s) {
            std::string out;
            out.reserve(s.size() + 2);
            out.push_back('"');
            for (char c : s) {
                switch (c) {
                    case '"':  out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\n': out += "\\n";  break;
                    case '\r': out += "\\r";  break;
                    case '\t': out += "\\t";  break;
                    default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                            char buf[8];
                            std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<int>(c));
                            out += buf;
                        } else {
                            out.push_back(c);
                        }
                }
            }
            out.push_back('"');
            return out;
        }

        // Minimal JSON parser sufficient for the shape we emit. Returns the
        // string starting at `pos`, advancing `pos` past the closing quote.
        std::string parseString(std::string_view s, size_t& pos) {
            std::string out;
            ++pos; // opening quote
            while (pos < s.size() && s[pos] != '"') {
                if (s[pos] == '\\' && pos + 1 < s.size()) {
                    switch (s[pos + 1]) {
                        case '"':  out.push_back('"');  break;
                        case '\\': out.push_back('\\'); break;
                        case 'n':  out.push_back('\n'); break;
                        case 'r':  out.push_back('\r'); break;
                        case 't':  out.push_back('\t'); break;
                        case 'u':
                            if (pos + 5 < s.size()) {
                                const auto code = std::stoi(std::string{s.substr(pos + 2, 4)}, nullptr, 16);
                                if (code < 0x80)
                                    out.push_back(static_cast<char>(code));
                                pos += 4;
                            }
                            break;
                        default:   out.push_back(s[pos + 1]); break;
                    }
                    pos += 2;
                } else {
                    out.push_back(s[pos++]);
                }
            }
            if (pos < s.size()) ++pos; // closing quote
            return out;
        }

        int64_t parseInt(std::string_view s, size_t& pos) {
            size_t end = pos;
            if (end < s.size() && (s[end] == '-' || s[end] == '+')) ++end;
            while (end < s.size() && std::isdigit(static_cast<unsigned char>(s[end]))) ++end;
            const auto v = std::stoll(std::string{s.substr(pos, end - pos)});
            pos = end;
            return v;
        }

        void skipWS(std::string_view s, size_t& pos) {
            while (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) ++pos;
        }
    }

    void CNotificationStore::saveToDisk() const {
        const auto path = persistencePath();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        std::ostringstream out;
        out << "{\"next_id\":" << m_nextId << ",\"items\":[";
        bool first = true;
        for (const auto& [id, n] : m_byId) {
            if (n->state == SNotification::eState::CLOSED)
                continue;
            if (!first) out << ',';
            first = false;
            out << "{"
                << "\"id\":"      << n->id      << ','
                << "\"app\":"     << esc(n->appName)  << ','
                << "\"icon\":"    << esc(n->appIcon)  << ','
                << "\"summary\":" << esc(n->summary)  << ','
                << "\"body\":"    << esc(n->body)     << ','
                << "\"timeout\":" << n->expireTimeoutMs << ','
                << "\"urgency\":" << static_cast<int>(n->urgency) << ','
                << "\"state\":"   << static_cast<int>(n->state) << ','
                << "\"actions\":[";
            for (size_t i = 0; i < n->actions.size(); ++i) {
                if (i) out << ',';
                out << esc(n->actions[i]);
            }
            out << "]}";
        }
        out << "]}";

        // Atomic-ish write: tmp + rename so a half-flushed file doesn't
        // corrupt the store on next boot.
        const auto tmp = path.string() + ".tmp";
        {
            std::ofstream f{tmp, std::ios::trunc};
            f << out.str();
        }
        std::filesystem::rename(tmp, path, ec);
        if (ec) {
            Debug::log(Debug::WARN, "store: persist rename failed: {}", ec.message());
            return;
        }
        Debug::log(Debug::TRACE, "store: persisted {} notifications to {}",
                   m_byId.size(), path.string());
    }

    void CNotificationStore::loadFromDisk() {
        const auto path = persistencePath();
        std::ifstream f{path};
        if (!f) {
            Debug::log(Debug::TRACE, "store: no persistence file at {}", path.string());
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        const auto text = ss.str();
        if (text.empty())
            return;

        // Hand-rolled parser — narrow shape, predictable input.
        size_t pos = 0;
        skipWS(text, pos);
        if (pos >= text.size() || text[pos] != '{') {
            Debug::log(Debug::WARN, "store: persistence file isn't a JSON object");
            return;
        }
        ++pos;

        uint32_t nextId = 1;
        std::vector<SP<SNotification>> items;

        while (pos < text.size()) {
            skipWS(text, pos);
            if (text[pos] == '}') { ++pos; break; }
            if (text[pos] == ',') { ++pos; continue; }
            if (text[pos] != '"') { ++pos; continue; }

            const auto key = parseString(text, pos);
            skipWS(text, pos);
            if (pos < text.size() && text[pos] == ':') ++pos;
            skipWS(text, pos);

            if (key == "next_id") {
                nextId = static_cast<uint32_t>(parseInt(text, pos));
            } else if (key == "items" && pos < text.size() && text[pos] == '[') {
                ++pos;
                while (pos < text.size()) {
                    skipWS(text, pos);
                    if (text[pos] == ']') { ++pos; break; }
                    if (text[pos] == ',') { ++pos; continue; }
                    if (text[pos] != '{') { ++pos; continue; }
                    ++pos;
                    auto n = makeShared<SNotification>();
                    n->received = std::chrono::steady_clock::now();
                    while (pos < text.size()) {
                        skipWS(text, pos);
                        if (text[pos] == '}') { ++pos; break; }
                        if (text[pos] == ',') { ++pos; continue; }
                        if (text[pos] != '"') { ++pos; continue; }
                        const auto k2 = parseString(text, pos);
                        skipWS(text, pos);
                        if (pos < text.size() && text[pos] == ':') ++pos;
                        skipWS(text, pos);
                        if (k2 == "id")          n->id        = static_cast<uint32_t>(parseInt(text, pos));
                        else if (k2 == "app")    n->appName   = parseString(text, pos);
                        else if (k2 == "icon")   n->appIcon   = parseString(text, pos);
                        else if (k2 == "summary") n->summary  = parseString(text, pos);
                        else if (k2 == "body")   n->body      = parseString(text, pos);
                        else if (k2 == "timeout") n->expireTimeoutMs = static_cast<int32_t>(parseInt(text, pos));
                        else if (k2 == "urgency") n->urgency  = static_cast<eUrgency>(parseInt(text, pos));
                        else if (k2 == "state")   n->state    = static_cast<SNotification::eState>(parseInt(text, pos));
                        else if (k2 == "actions" && pos < text.size() && text[pos] == '[') {
                            ++pos;
                            while (pos < text.size()) {
                                skipWS(text, pos);
                                if (text[pos] == ']') { ++pos; break; }
                                if (text[pos] == ',') { ++pos; continue; }
                                if (text[pos] == '"')
                                    n->actions.push_back(parseString(text, pos));
                                else
                                    ++pos;
                            }
                        }
                    }
                    if (n->id != 0) items.push_back(n);
                }
            }
        }

        m_nextId = std::max<uint32_t>(nextId, 1);
        for (auto& n : items) {
            // Replays come back as INBOX so we don't re-pop popups for old
            // entries on daemon restart.
            n->state = SNotification::eState::INBOX;
            m_byId.emplace(n->id, n);
        }
        Debug::log(Debug::INFO, "store: loaded {} notifications from {}",
                   items.size(), path.string());
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
