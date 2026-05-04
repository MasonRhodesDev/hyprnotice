#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

#include <hyprtoolkit/palette/Color.hpp>

namespace HN {

    // Parses ~/.config/matugen/lmtt-colors.css into a color map. The lmtt
    // workflow regenerates that file on every theme switch (and a custom lmtt
    // module SIGHUPs hyprnotice afterward). Color names match the matugen
    // template variable names: surface, on_surface, primary, error, etc.
    //
    // Looks up by name with a fallback so the daemon stays usable even when
    // the file is missing or doesn't define a particular variable.
    class CTheme {
      public:
        CTheme();

        // Reload from disk. Safe to call from a signal handler context
        // (just sets a flag; the dispatcher re-applies on its next idle).
        // Returns true if the file was successfully parsed.
        bool reload();

        // Look up a color by name (e.g. "surface", "on_surface"). If absent,
        // returns the provided fallback.
        Hyprtoolkit::CHyprColor get(const std::string& name,
                                    const Hyprtoolkit::CHyprColor& fallback) const;

        // Path to the file; lmtt's matugen output. Configurable later via
        // hyprlang.
        static std::filesystem::path defaultPath();

      private:
        std::unordered_map<std::string, Hyprtoolkit::CHyprColor> m_colors;
    };

    // Process-global instance, populated at daemon startup.
    extern CTheme g_theme;

}
