// Param quantities for Yamaha YM2612 parameters.
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

#include "rack.hpp"

#ifndef ENGINE_YM2612_PARAMS_HPP_
#define ENGINE_YM2612_PARAMS_HPP_

/// @brief A parameter quantity for the YM2612 LFO.
struct LFOQuantity : rack::ParamQuantity {
    /// @brief Return the value as a formatted string.
    inline std::string getDisplayValueString() override {
        const char* labels[8] = {
            "3.98",
            "5.56",
            "6.02",
            "6.37",
            "6.88",
            "9.63",
            "48.1",
            "72.2"
        };
        const int index = getValue();
        if (index < 0 || index > 7) return "?";
        return labels[index];
    }

    // /// @brief Return the minimal allowed value.
    // inline float getMinValue() override { return 0.f; }

    // /// @brief Return the maximal allowed value.
    // inline float getMaxValue() override { return 7.f; }

    // /// @brief Return the default value.
    // inline float getDefaultValue() override { return 0.f; }

    /// @brief Return the parameter description.
    inline std::string getLabel() override { return "LFO frequency"; }

    /// @brief Return the unit description.
    inline std::string getUnit() override { return " Hz"; }
};

/// @brief A parameter quantity for the YM2612 Multiplier.
struct MultiplierQuantity : rack::ParamQuantity {
    /// @brief Return the value as a formatted string.
    inline std::string getDisplayValueString() override {
        const int value = getValue();
        // the 0th index multiplier is 1/2, not 0
        if (value == 0) return "1/2";
        // the remaining multipliers are integer values
        return std::to_string(value);
    }

    // /// @brief Return the minimal allowed value.
    // inline float getMinValue() override { return 0.f; }

    // /// @brief Return the maximal allowed value.
    // inline float getMaxValue() override { return 15.f; }

    // /// @brief Return the default value.
    // inline float getDefaultValue() override { return 1.f; }

    /// @brief Return the parameter description.
    inline std::string getLabel() override { return "Multiplier"; }

    /// @brief Return the unit description.
    inline std::string getUnit() override { return "x"; }
};

/// @brief A parameter quantity for the YM2612 amplitude modulation sensitivity.
struct AMSQuantity : rack::ParamQuantity {
    /// @brief Return the value as a formatted string.
    inline std::string getDisplayValueString() override {
        // sensitivity labels (in dB) from the Genesis programming manual
        const char* labels[4] = {
            "0.00",
            "1.40",
            "5.90",
            "11.8"
        };
        const unsigned index = getValue();
        if (index > 3) return "?";
        return labels[index];
    }

    // /// @brief Return the minimal allowed value.
    // inline float getMinValue() override { return 0.f; }

    // /// @brief Return the maximal allowed value.
    // inline float getMaxValue() override { return 4.f; }

    // /// @brief Return the default value.
    // inline float getDefaultValue() override { return 0.f; }

    /// @brief Return the parameter description.
    inline std::string getLabel() override {
        return "LFO amplitude modulation sensitivity";
    }

    /// @brief Return the unit description.
    inline std::string getUnit() override {
        return "dB";
    }
};

/// @brief A parameter quantity for the YM2612 frequency modulation sensitivity.
struct FMSQuantity : rack::ParamQuantity {
    /// @brief Return the value as a formatted string.
    inline std::string getDisplayValueString() override {
        // FMS labels (% of a halftone) from the Genesis programming manual
        const char* labels[8] = {
            "0.00",
            "3.40",
            "6.70",
            "10.0",
            "14.0",
            "20.0",
            "40.0",
            "80.0"
        };
        const unsigned index = getValue();
        if (index > 7) return "?";
        return labels[index];
    }

    // /// @brief Return the minimal allowed value.
    // inline float getMinValue() override { return 0.f; }

    // /// @brief Return the maximal allowed value.
    // inline float getMaxValue() override { return 7.f; }

    // /// @brief Return the default value.
    // inline float getDefaultValue() override { return 0.f; }

    /// @brief Return the parameter description.
    inline std::string getLabel() override {
        return "LFO frequency modulation sensitivity";
    }

    /// @brief Return the unit description.
    inline std::string getUnit() override { return "\% of a halftone"; }
};

#endif  // ENGINE_YM2612_PARAMS_HPP_
