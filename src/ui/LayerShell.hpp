#pragma once

#include <cstdint>

// hyprtoolkit's CWindowBuilder takes raw uint32_t for layer-shell parameters
// rather than wrapping the wlr-layer-shell-unstable-v1 enums itself. These
// constants mirror the protocol — values are stable and load-bearing across
// all wlroots-based compositors (Hyprland, sway, etc.).
namespace HN::LayerShell {

    // zwlr_layer_shell_v1::layer
    constexpr uint32_t LAYER_BACKGROUND = 0;
    constexpr uint32_t LAYER_BOTTOM     = 1;
    constexpr uint32_t LAYER_TOP        = 2;
    constexpr uint32_t LAYER_OVERLAY    = 3;

    // zwlr_layer_surface_v1::anchor (bitmask)
    constexpr uint32_t ANCHOR_TOP    = 1;
    constexpr uint32_t ANCHOR_BOTTOM = 2;
    constexpr uint32_t ANCHOR_LEFT   = 4;
    constexpr uint32_t ANCHOR_RIGHT  = 8;

    // zwlr_layer_surface_v1::keyboard_interactivity
    constexpr uint32_t KB_NONE      = 0;
    constexpr uint32_t KB_EXCLUSIVE = 1;
    constexpr uint32_t KB_ON_DEMAND = 2;

}
