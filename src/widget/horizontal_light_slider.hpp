// Horizontal FIR slider using Rack's standard parameter and light widgets.
// Copyright 2026 Christian Kauten
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef WIDGET_HORIZONTAL_LIGHT_SLIDER_HPP_
#define WIDGET_HORIZONTAL_LIGHT_SLIDER_HPP_

#include "../plugin.hpp"

/// A horizontal track with a solid handle that remains visible without light.
struct HorizontalSlider : rack::app::SvgSlider {
    HorizontalSlider() {
        setBackgroundSvg(rack::Svg::load(rack::asset::plugin(
            plugin_instance, "res/HorizontalSlider.svg")));
        setHandleSvg(rack::Svg::load(rack::asset::plugin(
            plugin_instance, "res/HorizontalSliderHandle.svg")));
        setHandlePos(rack::math::Vec(0, 0), rack::math::Vec(64, 0));
        horizontal = true;
    }
};

/// Rack moves this RGB indicator with the handle and owns drag/history behavior.
using HorizontalLightSlider = rack::componentlibrary::LightSlider<
    HorizontalSlider,
    rack::componentlibrary::SmallLight<rack::componentlibrary::RedGreenBlueLight>>;

#endif  // WIDGET_HORIZONTAL_LIGHT_SLIDER_HPP_
