// Extensions to the VCV Rack helper functions.
// Copyright 2020 Christian Kauten
//
// Author: Christian Kauten (kautenja@auburn.edu)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef KAUTENJA_RACK_HELPERS_HPP
#define KAUTENJA_RACK_HELPERS_HPP

#include "rack.hpp"

/// @brief Create a parameter that snaps to integer values.
///
/// @tparam P the type of the parameter to initialize
/// @tparam Args the type of arguments to pass to the `createParam` function
/// @param args the arguments to pass to the `createParam` function
/// @returns a pointer to the freshly allocated parameter
///
template<typename P, typename... Args>
inline rack::ParamWidget* createSnapParam(Args... args) {
    auto param = rack::createParam<P>(args...);
    param->snap = true;
    return param;
}

/// @brief Set the given 3-color VU meter light based on given VU meter.
///
/// @param vuMeter the VU meter to get the data from
/// @param light the light to update from the VU meter data
///
inline void setVULight3(rack::dsp::VuMeter2& vuMeter, rack::engine::Light* light) {
    // get the global brightness scale from -12 to 3
    auto brightness = vuMeter.getBrightness(-12, 3);
    // set the red light based on total brightness and
    // brightness from 0dB to 3dB
    (light + 0)->setBrightness(brightness * vuMeter.getBrightness(0, 3));
    // set the red light based on inverted total brightness and
    // brightness from -12dB to 0dB
    (light + 1)->setBrightness((1 - brightness) * vuMeter.getBrightness(-12, 0));
    // set the blue light to off
    (light + 2)->setBrightness(0);
}

/// @brief Return the normal voltage for a port in a normalling chain.
///
/// @param input a pointer to the first input in the normal chain
/// @param offset the index of the port in the normal chain
/// @param channel the polyphony channel to get the voltage of
/// @param voltage the normal voltage to use for the first port in the chain
/// @returns the voltage based on the normal chain from input to offset
///
template<typename T>
inline float normalChain(
    T* input,
    unsigned offset,
    unsigned channel,
    float voltage = 0.f
) {
    // if the offset is greater than 0, return the voltage of the previous input,
    // otherwise use the default voltage for the normal chain
    const auto normal = offset ? input[offset - 1].getVoltage(channel) : voltage;
    // get the output voltage based on the normalled input and channel
    const auto output = input[offset].getNormalVoltage(normal, channel);
    // reset the voltage on this port for other ports in the normalling chain
    input[offset].setVoltage(output, channel);
    return output;
}

#endif  // KAUTENJA_RACK_HELPERS_HPP
