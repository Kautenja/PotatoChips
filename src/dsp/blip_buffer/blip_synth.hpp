// Band-limited waveform generation.
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

#ifndef BLIP_BUFFER_BLIP_SYNTH_HPP
#define BLIP_BUFFER_BLIP_SYNTH_HPP

#include "blip_buffer.hpp"
#include <type_traits>

/// Quality level. Higher levels are slower, and worse in a few cases.
/// Use "Good" as a starting point.
enum class BLIPQuality {
    Low    = 1,
    Medium = 2,
    Good   = 3,
    High   = 4
};

/// BLIPSynth is a transition waveform synthesizer which adds band-limited
/// offsets (transitions) into a BLIPBuffer. For a simpler interface, use
/// Blip_Wave (below).
///
/// Range specifies the greatest expected offset that will occur. For a
/// waveform that goes between +amp and -amp, range should be amp * 2 (half
/// that if it only goes between +amp and 0). When range is large, a higher
/// accuracy scheme is used; to force this even when range is small, pass
/// the negative of range (i.e. -range).
template<BLIPQuality quality, int range>
class BLIPSynth {
    static_assert(-32768 <= range && range <= 32767);
    enum {
        abs_range = (range < 0) ? -range : range,
        fine_mode = (range > 512 || range < 0),
        width = static_cast<int>(quality) * 4,
        res = 1 << blip_res_bits_,
        impulse_size = width / 2 * (fine_mode + 1),
        base_impulses_size = width / 2 * (res / 2 + 1),
        fine_bits = (fine_mode ? (abs_range <= 64 ? 2 : abs_range <= 128 ? 3 :
            abs_range <= 256 ? 4 : abs_range <= 512 ? 5 : abs_range <= 1024 ? 6 :
            abs_range <= 2048 ? 7 : 8) : 0)
    };
    blip_pair_t_  impulses[impulse_size * res * 2 + base_impulses_size];
    BLIPImpulse impulse;

 public:
    /// Initialize a new BLIP synth.
    BLIPSynth() { impulse.init(impulses, width, res, fine_bits); }

    /// Configure low-pass filter (see notes.txt).
    /// Not optimized for real-time control
    inline void treble_eq(const blip_eq_t& eq) { impulse.treble_eq(eq); }

    /// Set volume of a transition at amplitude 'range' by setting volume_unit
    /// to v / range
    inline void volume(double v) { impulse.volume_unit(v * (1.0 / abs_range)); }

    /// Set base volume unit of transitions, where 1.0 is a full swing between
    /// the positive and negative extremes. Not optimized for real-time control.
    inline void volume_unit(double unit) { impulse.volume_unit(unit); }

    /// Return buffer used for output when none is specified for a given call.
    inline BLIPBuffer* output() const { return impulse.buf; }

    inline void output(BLIPBuffer* b) { impulse.buf = b; }

    /// Add an amplitude offset (transition) with a magnitude of
    /// delta * volume_unit into the specified buffer (default buffer if none
    /// specified) at the specified source time. Delta can be positive or
    /// negative. To increase performance by inlining code at the call site,
    /// use offset_inline().
    inline void offset(blip_time_t time, int delta, BLIPBuffer* buf) const {
        offset_resampled(time * buf->factor_ + buf->offset_, delta, buf);
    }

    inline void offset(blip_time_t t, int delta) const {
        offset(t, delta, impulse.buf);
    }

    void offset_resampled(BLIPBuffer::resampled_time_t time, int delta, BLIPBuffer* blip_buf) const {
        typedef blip_pair_t_ pair_t;

        unsigned sample_index = (time >> BLIPBuffer::BLIP_BUFFER_ACCURACY) & ~1;
        assert(("BLIPSynth/Blip_wave: Went past end of buffer", sample_index < blip_buf->buffer_size_));
        enum { const_offset = BLIPBuffer::widest_impulse_ / 2 - width / 2 };
        pair_t* buf = (pair_t*) &blip_buf->buffer_[const_offset + sample_index];

        enum { shift = BLIPBuffer::BLIP_BUFFER_ACCURACY - blip_res_bits_ };
        enum { mask = res * 2 - 1 };
        const pair_t* imp = &impulses[((time >> shift) & mask) * impulse_size];

        pair_t deltaOffset = impulse.offset * delta;

        if (!fine_bits) {
            // normal mode
            for (int n = width / 4; n; --n) {
                pair_t t0 = buf[0] - deltaOffset;
                pair_t t1 = buf[1] - deltaOffset;

                t0 += imp[0] * delta;
                t1 += imp[1] * delta;
                imp += 2;

                buf[0] = t0;
                buf[1] = t1;
                buf += 2;
            }
        } else {
            // fine mode
            enum { sub_range = 1 << fine_bits };
            delta += sub_range / 2;
            int delta2 = (delta & (sub_range - 1)) - sub_range / 2;
            delta >>= fine_bits;

            for (int n = width / 4; n; --n) {
                pair_t t0 = buf[0] - deltaOffset;
                pair_t t1 = buf[1] - deltaOffset;

                t0 += imp[0] * delta2;
                t0 += imp[1] * delta;

                t1 += imp[2] * delta2;
                t1 += imp[3] * delta;

                imp += 4;

                buf[0] = t0;
                buf[1] = t1;
                buf += 2;
            }
        }
    }

    inline void offset_resampled(BLIPBuffer::resampled_time_t t, int o) const {
        offset_resampled(t, o, impulse.buf);
    }

    inline void offset_inline(blip_time_t time, int delta, BLIPBuffer* buf) const {
        offset_resampled(time * buf->factor_ + buf->offset_, delta, buf);
    }

    inline void offset_inline(blip_time_t time, int delta) const {
        offset_inline(time, delta, impulse.buf);
    }
};

/// Blip_Wave is a synthesizer for adding a *single* waveform to a BLIPBuffer.
/// A wave is built from a series of delays and new amplitudes. This provides a
/// simpler interface than BLIPSynth, nothing more.
template<BLIPQuality quality, int range>
class Blip_Wave {
    BLIPSynth<quality, range> synth;
    blip_time_t time_;
    int last_amp;

 public:
    /// Start wave at time 0 and amplitude 0
    Blip_Wave() : time_(0), last_amp(0) { }

    /// See BLIPSynth for description
    inline void volume(double v) { synth.volume(v); }

    inline void volume_unit(double v) { synth.volume_unit(v); }

    inline void treble_eq(const blip_eq_t& eq) { synth.treble_eq(eq); }

    inline BLIPBuffer* output() const { return synth.output(); }

    inline void output(BLIPBuffer* b) {
        synth.output(b);
        if (!b) time_ = last_amp = 0;
    }

    /// Current time in frame
    inline blip_time_t time() const { return time_; }

    inline void time(blip_time_t t) { time_ = t; }

    /// Current amplitude of wave
    inline int amplitude() const { return last_amp; }

    inline void amplitude(int amp) {
        int delta = amp - last_amp;
        last_amp = amp;
        synth.offset_inline(time_, delta);
    }

    /// Move forward by 't' time units
    inline void delay(blip_time_t t) { time_ += t; }

    /// End time frame of specified duration. Localize time to new frame.
    inline void end_frame(blip_time_t duration) {
        assert(("Blip_Wave::end_frame(): Wave hadn't yet been run for entire frame", duration <= time_));
        time_ -= duration;
    }
};

#endif  // BLIP_BUFFER_BLIP_SYNTH_HPP
