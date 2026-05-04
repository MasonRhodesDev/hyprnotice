#include "Config.hpp"

#include <algorithm>
#include <cctype>

#include <hyprutils/path/Path.hpp>

#include "../helpers/Log.hpp"
#include "../ui/LayerShell.hpp"

namespace HN {

    CConfigManager g_config;

    namespace {
        std::string findPath() {
            const auto p = Hyprutils::Path::findConfig("hyprnotice");
            return p.first.value_or("");
        }

        // Parse anchor strings like "top right" / "top-right" / "bottom_left"
        // into the wlr-layer-shell anchor bitmask. Empty string -> default
        // (top right). Unknown tokens are ignored with a warning.
        uint32_t parseAnchor(const std::string& s) {
            if (s.empty())
                return LayerShell::ANCHOR_TOP | LayerShell::ANCHOR_RIGHT;
            uint32_t out  = 0;
            std::string t = s;
            for (auto& c : t) c = std::tolower(static_cast<unsigned char>(c));
            std::replace(t.begin(), t.end(), '-', ' ');
            std::replace(t.begin(), t.end(), '_', ' ');

            size_t pos = 0;
            while (pos < t.size()) {
                while (pos < t.size() && std::isspace(static_cast<unsigned char>(t[pos]))) ++pos;
                size_t end = pos;
                while (end < t.size() && !std::isspace(static_cast<unsigned char>(t[end]))) ++end;
                const std::string tok = t.substr(pos, end - pos);
                pos = end;
                if (tok.empty())                continue;
                else if (tok == "top")          out |= LayerShell::ANCHOR_TOP;
                else if (tok == "bottom")       out |= LayerShell::ANCHOR_BOTTOM;
                else if (tok == "left")         out |= LayerShell::ANCHOR_LEFT;
                else if (tok == "right")        out |= LayerShell::ANCHOR_RIGHT;
                else
                    Debug::log(Debug::WARN, "config: unknown anchor token \"{}\"", tok);
            }
            return out ? out : (LayerShell::ANCHOR_TOP | LayerShell::ANCHOR_RIGHT);
        }

        uint32_t parseLayer(const std::string& s) {
            if (s == "background") return LayerShell::LAYER_BACKGROUND;
            if (s == "bottom")     return LayerShell::LAYER_BOTTOM;
            if (s == "top")        return LayerShell::LAYER_TOP;
            if (s == "overlay" || s.empty()) return LayerShell::LAYER_OVERLAY;
            Debug::log(Debug::WARN, "config: unknown layer \"{}\"; using overlay", s);
            return LayerShell::LAYER_OVERLAY;
        }
    }

    CConfigManager::CConfigManager()
        : m_config(findPath().c_str(),
                   Hyprlang::SConfigOptions{.throwAllErrors = false, .allowMissingConfig = true}),
          m_path(findPath()) {
        registerSchema();
    }

    void CConfigManager::registerSchema() {
        m_config.addConfigValue("popup:anchor",          Hyprlang::STRING{"top right"});
        m_config.addConfigValue("popup:layer",           Hyprlang::STRING{"overlay"});
        m_config.addConfigValue("popup:margin_top",      Hyprlang::INT{12});
        m_config.addConfigValue("popup:margin_left",     Hyprlang::INT{12});
        m_config.addConfigValue("popup:width",           Hyprlang::INT{360});
        m_config.addConfigValue("popup:height",          Hyprlang::INT{110});
        m_config.addConfigValue("popup:default_timeout", Hyprlang::INT{5000});
        m_config.addConfigValue("popup:max_visible",     Hyprlang::INT{5});

        m_config.addConfigValue("theme:colors_path",     Hyprlang::STRING{""});

        // `rule { app = slack; timeout = 0 }` blocks. Anonymous-keyed so the
        // user can stack multiple of them.
        m_config.addSpecialCategory("rule",
            Hyprlang::SSpecialCategoryOptions{.key = nullptr, .anonymousKeyBased = true});
        m_config.addSpecialConfigValue("rule", "app",        Hyprlang::STRING{""});
        m_config.addSpecialConfigValue("rule", "timeout",    Hyprlang::INT{-1});
        m_config.addSpecialConfigValue("rule", "skip_popup", Hyprlang::INT{0});

        m_config.commence();
    }

    bool CConfigManager::load() {
        // Re-resolve the path on every load so `pkill -HUP` after creating
        // the config file picks it up without a daemon restart.
        m_path = findPath();
        if (m_path.empty()) {
            Debug::log(Debug::INFO, "config: no file found at standard paths; using defaults");
            cacheValues();
            return true;
        }
        Debug::log(Debug::INFO, "config: parsing {}", m_path);
        const auto result = m_config.parse();
        if (result.error)
            Debug::log(Debug::WARN, "config: parse errors: {}", result.getError());
        cacheValues();
        return !result.error;
    }

    void CConfigManager::cacheValues() {
        const auto getInt = [&](const char* key) {
            return std::any_cast<Hyprlang::INT>(m_config.getConfigValue(key));
        };
        const auto getStr = [&](const char* key) -> std::string {
            const auto v = std::any_cast<Hyprlang::STRING>(m_config.getConfigValue(key));
            return v ? std::string{v} : std::string{};
        };

        m_current.anchor          = parseAnchor(getStr("popup:anchor"));
        m_current.layer           = parseLayer(getStr("popup:layer"));
        m_current.margin_top      = static_cast<int32_t>(getInt("popup:margin_top"));
        m_current.margin_left     = static_cast<int32_t>(getInt("popup:margin_left"));
        m_current.width           = static_cast<int32_t>(getInt("popup:width"));
        m_current.height          = static_cast<int32_t>(getInt("popup:height"));
        m_current.default_timeout = static_cast<int32_t>(getInt("popup:default_timeout"));
        m_current.max_visible     = static_cast<int32_t>(getInt("popup:max_visible"));
        m_current.colors_path     = getStr("theme:colors_path");

        m_current.rules.clear();
        const auto keys = m_config.listKeysForSpecialCategory("rule");
        for (const auto& key : keys) {
            const auto getRuleStr = [&](const char* k) -> std::string {
                const auto v = std::any_cast<Hyprlang::STRING>(
                    m_config.getSpecialConfigValue("rule", k, key.c_str()));
                return v ? std::string{v} : std::string{};
            };
            const auto getRuleInt = [&](const char* k) {
                return std::any_cast<Hyprlang::INT>(
                    m_config.getSpecialConfigValue("rule", k, key.c_str()));
            };

            SAppRule r;
            r.app = getRuleStr("app");
            if (r.app.empty())
                continue;
            const auto t = getRuleInt("timeout");
            if (t != -1)
                r.timeout = static_cast<int32_t>(t);
            const auto s = getRuleInt("skip_popup");
            if (s != 0)
                r.skipPopup = (s != 0);
            m_current.rules.push_back(std::move(r));
        }
        Debug::log(Debug::INFO, "config: cached {} rule(s)", m_current.rules.size());
    }

    std::optional<SAppRule> CConfigManager::matchRule(const std::string& appName) const {
        if (appName.empty())
            return std::nullopt;
        // Case-insensitive substring match. Returns the first hit (definition
        // order) so users can put a more specific rule above a generic one.
        std::string a;
        a.reserve(appName.size());
        for (auto c : appName) a.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        for (const auto& r : m_current.rules) {
            std::string b;
            b.reserve(r.app.size());
            for (auto c : r.app) b.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            if (a.find(b) != std::string::npos)
                return r;
        }
        return std::nullopt;
    }

}
