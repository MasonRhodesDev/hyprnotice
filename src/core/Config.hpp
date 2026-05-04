#pragma once

#include <cstdint>
#include <string>

#include <hyprlang.hpp>

namespace HN {

    // hyprlang-backed runtime config. Values cached in Config struct after
    // load() so popup builds don't pay parser overhead per frame. Config
    // file lives at $XDG_CONFIG_HOME/hypr/hyprnotice.conf (with the standard
    // ~/.config/hypr/, /etc/xdg/hypr/, etc. fallback chain via
    // Hyprutils::Path::findConfig).
    //
    // A missing config file is fine — every value has a baked-in default.
    struct SConfig {
        // Layer-shell placement. Values mirror zwlr_layer_surface_v1::anchor
        // bitmask literals in src/ui/LayerShell.hpp; the parser converts
        // human strings ("top right" etc.) into the bitmask.
        uint32_t anchor       = 1 | 8;       // ANCHOR_TOP | ANCHOR_RIGHT
        uint32_t layer        = 3;            // LAYER_OVERLAY
        int32_t  margin_top   = 12;
        int32_t  margin_left  = 12;

        // Popup geometry & timing.
        int32_t  width            = 360;
        int32_t  height           = 110;     // initial; auto-grows for body
        int32_t  default_timeout  = 5000;    // ms; 0 = persistent
        int32_t  max_visible      = 5;       // popup queue cap (TODO: enforce)

        // Theming.
        std::string colors_path = "";        // empty = use CTheme::defaultPath()
    };

    class CConfigManager {
      public:
        // Resolves the config path via Hyprutils::Path::findConfig("hyprnotice")
        // and constructs a Hyprlang::CConfig with allowMissingConfig=true so
        // the daemon starts cleanly even before a user has written a config.
        CConfigManager();
        bool load();

        // Read-only snapshot. Lambdas in popup builds capture by const-ref
        // to avoid stale copies after a reload.
        const SConfig& current() const { return m_current; }

      private:
        Hyprlang::CConfig m_config;
        SConfig           m_current;
        std::string       m_path;

        void registerSchema();
        void cacheValues();
    };

    extern CConfigManager g_config;

}
