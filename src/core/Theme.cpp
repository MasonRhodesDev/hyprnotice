#include "Theme.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>

#include "../helpers/Log.hpp"

namespace HN {

    CTheme g_theme;

    CTheme::CTheme() = default;

    std::filesystem::path CTheme::defaultPath() {
        if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg)
            return std::filesystem::path{xdg} / "matugen" / "lmtt-colors.css";
        if (const char* home = std::getenv("HOME"); home && *home)
            return std::filesystem::path{home} / ".config" / "matugen" / "lmtt-colors.css";
        return {};
    }

    namespace {
        // Parse "#rgb", "#rrggbb", or "#rrggbbaa" hex into ARGB32 used by CHyprColor.
        std::optional<uint64_t> parseHex(std::string_view s) {
            if (s.empty() || s[0] != '#')
                return std::nullopt;
            s.remove_prefix(1);

            const auto fromHex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return -1;
            };

            uint32_t r = 0, g = 0, b = 0, a = 0xFF;
            if (s.size() == 3) {
                int rh = fromHex(s[0]), gh = fromHex(s[1]), bh = fromHex(s[2]);
                if (rh < 0 || gh < 0 || bh < 0)
                    return std::nullopt;
                r = (rh << 4) | rh;
                g = (gh << 4) | gh;
                b = (bh << 4) | bh;
            } else if (s.size() == 6 || s.size() == 8) {
                auto h = [&](size_t i) { return fromHex(s[i]); };
                if (h(0) < 0 || h(1) < 0 || h(2) < 0 || h(3) < 0 || h(4) < 0 || h(5) < 0)
                    return std::nullopt;
                r = (h(0) << 4) | h(1);
                g = (h(2) << 4) | h(3);
                b = (h(4) << 4) | h(5);
                if (s.size() == 8) {
                    if (h(6) < 0 || h(7) < 0)
                        return std::nullopt;
                    a = (h(6) << 4) | h(7);
                }
            } else {
                return std::nullopt;
            }
            return (static_cast<uint64_t>(a) << 24) | (r << 16) | (g << 8) | b;
        }
    }

    bool CTheme::reload() {
        const auto path = defaultPath();
        if (path.empty()) {
            Debug::log(Debug::WARN, "theme: no path resolved (HOME/XDG_CONFIG_HOME unset)");
            return false;
        }

        std::ifstream f{path};
        if (!f) {
            Debug::log(Debug::WARN, "theme: cannot read {}", path.string());
            return false;
        }

        // @define-color <name> <#hex>;  per matugen's lmtt template.
        const std::regex re{R"(@define-color\s+([A-Za-z_][A-Za-z0-9_]*)\s+(#[0-9A-Fa-f]+)\s*;)"};

        std::unordered_map<std::string, Hyprtoolkit::CHyprColor> next;
        std::string                                              line;
        while (std::getline(f, line)) {
            std::smatch m;
            if (!std::regex_search(line, m, re))
                continue;
            const auto name = m[1].str();
            const auto hex  = m[2].str();
            if (auto argb = parseHex(hex))
                next.emplace(name, Hyprtoolkit::CHyprColor{*argb});
        }

        Debug::log(Debug::INFO, "theme: loaded {} colors from {}", next.size(), path.string());
        m_colors = std::move(next);
        return true;
    }

    Hyprtoolkit::CHyprColor CTheme::get(const std::string& name,
                                         const Hyprtoolkit::CHyprColor& fallback) const {
        auto it = m_colors.find(name);
        return it != m_colors.end() ? it->second : fallback;
    }

}
