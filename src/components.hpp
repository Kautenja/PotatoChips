// Components for the plugin.
// Copyright 2020 Christian Kauten
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

#include <string>
#include "rack.hpp"

#ifndef COMPONENTS_HPP_
#define COMPONENTS_HPP_

/// @brief Create a parameter that snaps to integer values.
///
/// @tparam P the type of the parameter to initialize
/// @tparam Args the type of arguments to pass to the `createParam` function
/// @tparam args the arguments to pass to the `createParam` function
/// @returns a pointer to the freshly allocated parameter
///
template<typename P, typename... Args>
inline rack::ParamWidget* createSnapParam(Args... args) {
    auto param = rack::createParam<P>(args...);
    param->snap = true;
    return param;
}

/// @brief A parameter quantity for a boolean switch.
struct BooleanParamQuantity : rack::ParamQuantity {
    /// @brief Return the value as a formatted string.
    inline std::string getDisplayValueString() override {
        if (getValue()) return "On";
        return "Off";
    }
};

/// @brief A parameter quantity for a trigger button.
struct TriggerParamQuantity : rack::ParamQuantity {
    /// @brief Return the parameter description instead of the value text.
    inline std::string getDisplayValueString() override {
        return rack::ParamQuantity::getLabel();
    }

    /// @brief Return the parameter description (disabled for this trigger).
    inline std::string getLabel() override { return ""; }
};

#endif  // COMPONENTS_HPP_
