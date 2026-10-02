// A trigger that divides another trigger signal by an integer factor.
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

#ifndef DSP_TRIGGER_DIVIDER_HPP
#define DSP_TRIGGER_DIVIDER_HPP

#include <algorithm>
#include <cstdint>

/// @brief A collection of structures for detecting trigger events.
namespace Trigger {

/// @brief A trigger that detects integer divisions in other triggers.
struct Divider {
 private:
    /// the current sample of the divider
    uint32_t clock = 0;

    /// the integer division of the divider
    uint32_t division = 1;

 public:
    /// @brief Reset the internal clock to 0.
    /// @details
    /// The `division` parameter will not be effected.
    void reset() { clock = 0; }

    /// @brief Set the clock division to a new value.
    ///
    /// @param division_ the integer division of the source clock
    ///
    void setDivision(uint32_t division_) {
        division = std::max(division_, static_cast<uint32_t>(1));
    }

    /// @brief Return the integer division.
    ///
    /// @returns the number of sample divisions between triggers
    ///
    inline uint32_t getDivision() const { return division; }

    /// @brief Return the value of the internal clock.
    ///
    /// @returns the current value of the internal clock
    /// \f$\in [0, \f$`getDivision()`\f$)\f$
    ///
    uint32_t getClock() const { return clock; }

    /// @brief Return the phase of the clock divider.
    ///
    /// @returns the phase of the integer division cycle \f$\in [0.0, 1.0]\f$
    ///
    float getPhase() const {
        return static_cast<float>(clock) / static_cast<float>(division);
    }

    /// @brief Get the gate signal from the divider.
    ///
    /// @param pulse_width the width of the gate signal \f$\in [0.01, 0.99]\f$.
    /// @returns true if the gate is high, false otherwise
    ///
    bool getGate(float pulse_width = 0.5f) const {
        return getPhase() < std::max(std::min(pulse_width, 0.99f), 0.01f);
    }

    /// @brief Process a tick from the source clock.
    ///
    /// @returns true if the trigger divider is firing, false otherwise
    ///
    bool process() {
        // the trigger fires at 0 to hit down-beats
        const bool trigger = clock == 0;
        // the clock is incremented and wrapped around the division
        clock = (clock + 1) % division;
        return trigger;
    }
};

}  // namespace Trigger

#endif  // DSP_TRIGGER_DIVIDER_HPP
