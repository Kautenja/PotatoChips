// Native Rack panel theme selection shared by sound modules and blanks.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef RACK_EXTENSIONS_PANEL_HPP_
#define RACK_EXTENSIONS_PANEL_HPP_

#include <rack.hpp>
#include <string>

/// @brief Load a panel and its -dark.svg companion through Rack's SVG cache.
/// @param plugin owning plugin
/// @param light_path existing resource path ending in .svg
/// @returns a native panel following Rack's global preference, including previews
/// @details Call only during widget construction on the UI thread. Both assets
/// remain immutable; Rack switches cached backgrounds without module state.
inline rack::app::ThemedSvgPanel* createThemedPanel(
    rack::plugin::Plugin* plugin, const std::string& light_path) {
    const std::string dark_path = light_path.substr(0, light_path.size() - 4)
        + "-dark.svg";
    return rack::createPanel(rack::asset::plugin(plugin, light_path),
        rack::asset::plugin(plugin, dark_path));
}

#endif  // RACK_EXTENSIONS_PANEL_HPP_
