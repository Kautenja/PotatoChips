// Triggers for detecting boolean events in time-domain signals.
// Copyright 2020 Christian Kauten
// Copyright 2006 Shay Green
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

#ifndef DSP_TRIGGERS_
#define DSP_TRIGGERS_

/// @brief A trigger that detects when a boolean changes from false to true.
struct BooleanTrigger {
 private:
    /// the current state of the trigger
    bool state = false;

 public:
    /// @brief Return the state of the boolean trigger.
    inline bool isHigh() const { return state; }

    /// @brief Reset the trigger to its default state
    inline void reset() { state = false; }

    /// @brief Process a step of the boolean signal.
    ///
    /// @param state a sample of a boolean signal
    /// @returns true if the state changes from false to true
    ///
    inline bool process(bool signal) {
        const bool triggered = signal && !state;
        state = signal;
        return triggered;
    }
};

/// @brief A trigger that detects a threshold value.
struct ThresholdTrigger {
 private:
    /// the current value of the trigger's signal
    bool state = false;

 public:
    /// @brief Return the state of the boolean trigger.
    inline float isHigh() const { return state; }

    /// @brief Reset the trigger to its default state
    inline void reset() { state = false; }

    /// @brief Process a step of the signal.
    ///
    /// @param state a sample of an arbitrary signal
    /// @returns true if the trigger is above 1.0 and false if it is below 0.0.
    ///
    inline bool process(float signal) {
        if (state) {  // HIGH to LOW
            if (signal <= 0.f) state = false;
        } else if (signal >= 1.f) {  // LOW to HIGH
            state = true;
            return true;
        }
        return false;
    }
};

/// @brief A trigger that detects a threshold value held for a period of time.
struct HeldThresholdTrigger {
 private:
    /// the number of milliseconds to wait for detecting a hold
    static constexpr int HOLD_TIME = 100;

 public:
    /// @brief Reset the trigger to its default state
    inline void reset() { }

    /// @brief Process a step of the signal.
    ///
    /// @param signal the input signal to process
    /// @param sample_time the amount of time between samples, i.e.,
    /// \f$T_s = \frac{1}{f_s}\f$
    ///
    inline void process(float signal, float sample_time) { }

    inline bool didTrigger() {
        return false;
    }

    inline bool isHolding() {
        return false;
    }
};

#endif  // DSP_TRIGGERS_
