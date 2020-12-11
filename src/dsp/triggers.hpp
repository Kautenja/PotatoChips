// Triggers for detecting boolean events in time-domain signals.
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

#ifndef DSP_TRIGGERS_
#define DSP_TRIGGERS_

#include "exceptions.hpp"

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
    /// @brief Return the state of the boolean trigger. The state will go true
    /// after processing an input signal of \f$1.0\f$, and will stay high until
    /// the signal reaches \f$0.0\f$.
    ///
    inline float isHigh() const { return state; }

    /// @brief Reset the trigger to its default state
    inline void reset() { state = false; }

    /// @brief Process a step of the signal.
    ///
    /// @param state a sample of an arbitrary signal
    /// @returns true if the trigger goes above \f$1.0\f$ and false if it goes
    /// below \f$0.0\f$. The trigger goes high once per cycle and must return
    /// to \f$0.0\f$ before firing again, .i.e, `isHigh` will go true at
    /// \f$1.0\f$ and stay high until the signal reaches \f$0.0\f$.
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
    /// the number of seconds to wait for detecting a hold (\f$100ms\f$)
    static constexpr float HOLD_TIME = 0.100;

    /// the number of samples per second
    float sample_rate;

 public:
    /// @brief Initialize a new held threshold trigger.
    ///
    /// @param sample_rate the number of samples per second, i.e.,
    /// \f$f_s = \frac{1}{T_s}\f$
    ///
    explicit HeldThresholdTrigger(float sample_rate_ = 44100) {
        set_sample_rate(sample_rate_);
    }

    /// @brief Set the sample rate.
    ///
    /// @param sample_rate the number of samples per second, i.e.,
    /// \f$f_s = \frac{1}{T_s}\f$
    ///
    void set_sample_rate(float sample_rate_) {
        if (sample_rate_ <= 0.f)
            throw Exception("sample_rate must be positive");
        sample_rate = sample_rate_;
    }

    /// @brief Return the sample rate, i.e., \f$f_s = \frac{1}{T_s}\f$.
    float get_sample_rate() const { return sample_rate; }

    /// @brief Reset the trigger to the default state.
    /// @details
    /// This does not affect the sample rate of the trigger.
    inline void reset() { }

    /// @brief Process a step of the signal.
    ///
    /// @param signal the input signal to process
    /// @param sample_time the amount of time between samples, i.e.,
    /// \f$T_s = \frac{1}{f_s}\f$
    ///
    inline void process(float signal, float sample_time) {

    }

    inline bool didTrigger() {
        return false;
    }

    inline bool isHolding() {
        return false;
    }
};

#endif  // DSP_TRIGGERS_
