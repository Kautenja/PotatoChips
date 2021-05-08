// Band-limited sound synthesis buffer (forked from Blip_Buffer 0.4.1).
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

#ifndef DSP_BLIP_BUFFER_HPP_
#define DSP_BLIP_BUFFER_HPP_

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <limits>
#include "exceptions.hpp"

#if defined (__GNUC__) || _MSC_VER >= 1100
    #define BLIP_RESTRICT __restrict
#else
    #define BLIP_RESTRICT
#endif

/// @brief A Band-limited impulse polynomial buffer.
class BLIPBuffer {
 public:
    /// The number of bits in re-sampled ratio fraction. Higher values give a
    /// more accurate ratio but reduce maximum buffer size.
    static constexpr uint32_t ACCURACY = 16;
    /// Number bits in phase offset. Fewer than 6 bits (64 phase offsets)
    /// results in noticeable broadband noise when synthesizing high frequency
    /// square waves. Affects size of BLIPSynthesizer objects since they store
    /// the waveform directly.
    static constexpr uint32_t PHASE_BITS = 6;
    /// the size of the buffer and the largest impulse that it can accommodate
    static constexpr int32_t WIDEST_IMPULSE = 16;
    /// the index of the BLIP sample following the phase bits
    static constexpr int32_t RESOLUTION = 1 << PHASE_BITS;
    /// the dynamic range of the BLIP samples measured as a bit shift offset
    static constexpr uint32_t SAMPLE_BITS = 30;

 protected:
    /// The sample rate to generate samples from the buffer at
    uint32_t sample_rate = 0;
    /// The clock rate of the chip to emulate
    uint32_t clock_rate = 0;
    /// the clock rate factor, i.e., the number of CPU samples per audio sample
    uint32_t factor = 1L << ACCURACY;

    /// the cut-off frequency of the high-pass filter in Hz
    int32_t bass_freq = 16;
    /// the number of shifts to adjust samples to filter out bass according to
    /// the cut-off frequency of the hi-pass filter (`bass_freq`)
    int32_t bass_shift = 0;

    /// the accumulator for integrating samples into
    int32_t accumulator = 0;
    /// the buffer of samples in the BLIP buffer
    int32_t buffer[WIDEST_IMPULSE + 1];

 private:
    /// Disable the copy constructor.
    BLIPBuffer(const BLIPBuffer&);

    /// Disable the assignment operator.
    BLIPBuffer& operator=(const BLIPBuffer&);

 public:
    /// @brief Initialize a new BLIPBuffer.
    BLIPBuffer() { flush(); }

    /// @brief Return the current sample rate.
    ///
    /// @returns the audio sample rate
    ///
    inline uint32_t get_sample_rate() const { return sample_rate; }

    /// @brief Return the number of source time units per second.
    ///
    /// @returns the number of source time units per second
    ///
    inline uint32_t get_clock_rate() const { return clock_rate; }

    /// @brief Return the current factor from the sample rate and clock rate.
    ///
    /// @returns the current factor
    ///
    inline uint32_t get_factor() const { return factor; }

    /// @brief Return the frequency of the  high-pass filter.
    ///
    /// @returns the cut-off frequency of the high-pass filter
    /// @details
    /// Higher values reduce the bass more.
    ///
    inline uint32_t get_bass_freq() const { return bass_freq; }

    /// @brief Return the number of bits to shift for high-pass filtering.
    ///
    /// @returns the number of bits to shift to high-pass the signal
    ///
    inline uint32_t get_bass_shift() const { return bass_shift; }

    /// @brief Return the sample accumulator.
    ///
    /// @returns the sample accumulator
    ///
    inline int32_t get_accumulator() const { return accumulator; }

    /// @brief Return a pointer to the underlying buffer.
    ///
    /// @returns a pointer to the underlying buffer of samples
    ///
    inline int32_t* get_buffer() { return buffer; }

    /// @brief Flush the current contents of the buffer and accumulator.
    void flush() { accumulator = 0; memset(buffer, 0, sizeof(buffer)); }

    /// @brief Set the output sample rate and clock rate.
    ///
    /// @param sample_rate_ the number of samples per second
    /// @param clock_rate_ the number of source clock cycles per second
    ///
    void set_sample_rate(const uint32_t& sample_rate_, const uint32_t& clock_rate_) {
        if (!(sample_rate_ > 0))  // sample rate must be positive
            throw Exception("sample_rate must be greater than 0.");
        if (!(clock_rate_ > 0))  // clock rate must be positive
            throw Exception("clock_rate must be greater than 0.");
        // Calculate the number of clock cycles per sample, quantize by
        // truncation, and re-calculate the clock rate with rounding error
        // accounted for.
        auto quantized_clock_rate = sample_rate_ * (clock_rate_ / sample_rate_);
        // calculate the time factor based on the clock_rate and sample_rate
        float ratio = static_cast<float>(sample_rate_) / quantized_clock_rate;
        int32_t factor_ = floor(ratio * (1L << ACCURACY) + 0.5f);
        if (!(factor_ > 0))  // factor must be positive
            throw Exception("sample_rate : clock_rate ratio is too large.");
        // update the instance variables atomically after error handling
        sample_rate = sample_rate_;
        clock_rate = quantized_clock_rate;
        factor = factor_;
        // reset the bass frequency because sample_rate has changed. This
        // function is atomic and guaranteed to not raise an error.
        set_bass_freq(bass_freq);
        // clear the contents of the buffer / accumulator
        flush();
    }

    /// @brief Set the frequency of the global high-pass filter.
    ///
    /// @param frequency the cut-off frequency of the high-pass filter
    /// @details
    /// Higher frequency values reduce the bass more. Performance of this
    /// function varies by architecture.
    ///
    inline void set_bass_freq(const int32_t& frequency) {
        bass_freq = frequency;
        if (bass_freq > 0) {  // calculate the bass shift from the frequency
            #if defined(_M_IX86)    || \
                defined(_M_IA64)    || \
                defined(__i486__)   || \
                defined(__x86_64__) || \
                defined(__ia64__)   || \
                defined(__i386__)  // CISC (true)
                // extract the highest bit from the registered frequency
                asm(
                    "bsrl %1, %0"
                    : "=r" (bass_shift)
                    : "r" ((bass_freq << 16) / sample_rate)
                );
                bass_shift = 13 - bass_shift;
            #else  // CISC (false)
                // NOTE: above assembly replaces the following C++ while loop
                // for CISC architectures. See:
                // https://stackoverflow.com/questions/671815/what-is-the-fastest-most-efficient-way-to-find-the-highest-set-bit-msb-in-an-i
                // TODO: An assembly RISC equivalent can be worked out. See:
                // https://fgiesen.wordpress.com/2013/10/18/bit-scanning-equivalencies/
                bass_shift = 13;
                int32_t f = (bass_freq << 16) / sample_rate;
                while ((f >>= 1) && --bass_shift) { }
            #endif  // CISC
        } else {  // frequency is 0, set shift to static value
            bass_shift = 31;
        }
    }

    /// @brief Return a scaled floating point output sample from the buffer.
    ///
    /// @returns the sample \f$\in [-1, 1]\f$
    /// @details
    /// The buffer is advanced by the read operation.
    ///
    inline float read_sample() {
        // get the sample from the accumulator (don't clip it though). cast
        // it as a float for later calculation.
        float sample = accumulator >> (SAMPLE_BITS - 16);
        accumulator += *buffer - (accumulator >> (bass_shift));
        // copy remaining samples to beginning and clear old samples
        static constexpr auto count = 1;
        auto remain = count + WIDEST_IMPULSE;
        memmove(buffer, buffer + count, remain * sizeof *buffer);
        memset(buffer + remain, 0, count * sizeof *buffer);
        // scale the sample by the scale factor and the binary code space for
        // the digital signal to produce a floating point value
        return sample / std::numeric_limits<int16_t>::max();
    }
};

/// @brief Low-pass equalization parameters and logic.
/// @tparam T the datatype for computing floating point logic
template<typename T>
class BLIPEqualizer {
 private:
    /// the constant value for Pi
    static constexpr T PI = 3.1415926535897932384626433832795029;
    /// Logarithmic roll-off to treble dB at half sampling rate. Negative
    /// values reduce treble, small positive values (0 to 5.0) increase treble.
    T treble = 0;
    /// the cut-off frequency of the low-pass filter
    uint32_t cutoff_freq = 0;
    /// the roll-off frequency of the low-pass filter
    uint32_t rolloff_freq = 0;
    /// the sample rate the engine is running at
    uint32_t sample_rate = 0;

    /// Generate a sinc function.
    ///
    /// @param out the output buffer to generate sinc values into
    /// @param count the number of samples to generate
    /// @param oversample the amount of oversampling to apply
    /// @param treble Logarithmic roll-off to treble dB at half sampling rate.
    /// Negative values reduce treble, small positive values (0 to 5.0)
    /// increase treble.
    /// @param cutoff the cut-off frequency in [0, 1)
    ///
    static inline void gen_sinc(T* out, size_t count, T oversample, T treble, T cutoff) {
        if (cutoff >= 0.999)
            cutoff = 0.999;
        if (treble < -300)
            treble = -300;
        else if (treble > 5)
            treble = 5;
        static constexpr T maxh = 4096;
        const T rolloff = pow(10, 1 / (20 * maxh) * treble / (1 - cutoff));
        const T pow_a_n = pow(rolloff, maxh - maxh * cutoff);
        const T to_angle = PI / 2 / maxh / oversample;
        for (size_t i = 0; i < count; i++) {
            T angle = (2 * (i - count) + 1) * to_angle;
            T c = rolloff * cos((maxh - 1) * angle) - cos(maxh * angle);
            T cos_nc_angle = cos(maxh * cutoff * angle);
            T cos_nc1_angle = cos((maxh * cutoff - 1) * angle);
            T cos_angle = cos(angle);
            c = c * pow_a_n - rolloff * cos_nc1_angle + cos_nc_angle;
            T d = 1 + rolloff * (rolloff - cos_angle - cos_angle);
            T b = 2 - cos_angle - cos_angle;
            T a = 1 - cos_angle - cos_nc_angle + cos_nc1_angle;
            // a / b + c / d
            out[i] = (a * d + c * b) / (b * d);
        }
    }

 public:
    /// Initialize a new BLIPEqualizer.
    ///
    /// @param treble logarithmic roll-off to treble dB at half sampling rate.
    /// Negative values reduce treble, small positive values (0 to 5.0) increase
    /// treble.
    /// @param cutoff_freq the cut-off frequency of the low-pass filter
    /// @param rolloff_freq the roll-off frequency of the low-pass filter
    /// @param sample_rate the sample rate the engine is running at
    ///
    explicit BLIPEqualizer(T treble,
        uint32_t cutoff_freq = 0,
        uint32_t rolloff_freq = 0,
        uint32_t sample_rate = 44100
    ) :
        treble(treble),
        cutoff_freq(cutoff_freq),
        rolloff_freq(rolloff_freq),
        sample_rate(sample_rate) { }

    /// Generate sinc values into an output buffer with given quantity.
    ///
    /// @param out the output buffer to equalize
    /// @param count the number of samples to generate
    /// @details
    /// for usage within instances of BLIPSynthesizer_
    ///
    inline void operator()(T* out, uint32_t count) const {
        // lower cutoff freq for narrow kernels with their wider transition band
        // (8 points->1.49, 16 points->1.15)
        const T half_rate = sample_rate * 0.5;
        const T oversample = cutoff_freq ?
            half_rate / cutoff_freq :
            BLIPBuffer::RESOLUTION * 2.25 / count + 0.85;
        const T cutoff = rolloff_freq * oversample / half_rate;
        // generate a sinc
        gen_sinc(out, count, BLIPBuffer::RESOLUTION * oversample, treble, cutoff);
        // apply (half of) hamming window
        const T to_fraction = PI / (count - 1);
        for (uint32_t i = count; i > 0; i--)
            out[i] *= 0.54 - 0.46 * cos(i * to_fraction);
    }
};

/// the synthesis quality level. Start with BLIP_QUALITY_GOOD.
enum BLIPQuality {
    BLIP_QUALITY_MEDIUM  = 8,
    BLIP_QUALITY_GOOD = 12,
    BLIP_QUALITY_HIGH = 16
};

/// @brief A digital synthesizer for arbitrary waveforms based on BLIP.
/// @tparam QUALITY the quality of the BLIP algorithm
/// @tparam DYNAMIC_RANGE specifies the greatest expected change in amplitude.
/// Calculate it by finding the difference between the maximum and minimum
/// expected amplitudes (max - min).
///
template<BLIPQuality QUALITY, int32_t DYNAMIC_RANGE>
class BLIPSynthesizer {
 private:
    /// the last set volume level (used to detect changes in volume level)
    double volume_unit = 0;
    /// the impulses in the synthesizers buffer
    int16_t impulses[BLIPBuffer::RESOLUTION * (QUALITY / 2) + 1];
    /// the kernel unit for calculating amplitudes of impulses
    int32_t kernel_unit = 0;
    /// the output buffer that the synthesizer writes samples to
    BLIPBuffer* buffer = 0;
    /// the last amplitude value (DPCM sample) to output from the synthesizer
    int32_t last_amp = 0;
    /// the influence of amplitude deltas based on the volume unit
    int32_t delta_factor = 0;

    /// @brief Return the size of the impulses.
    static inline int32_t impulses_size() {
        return QUALITY * (BLIPBuffer::RESOLUTION / 2) + 1;
    }

    /// @brief Adjust the impulses in the buffer according to the kernel unit.
    void adjust_impulse() {
        // sum pairs for each phase and add error correction to end of 1st half
        static const int32_t SIZE = impulses_size();
        for (int32_t p = BLIPBuffer::RESOLUTION; p >= BLIPBuffer::RESOLUTION / 2; p--) {
            const int32_t p2 = BLIPBuffer::RESOLUTION - 2 - p;
            int32_t error = kernel_unit;
            for (int32_t i = 1; i < SIZE; i += BLIPBuffer::RESOLUTION) {
                error -= impulses[i + p ];
                error -= impulses[i + p2];
            }
            if (p == p2)  // phase = 0.5 impulse uses same half for both sides
                error /= 2;
            impulses[SIZE - BLIPBuffer::RESOLUTION + p] += error;
        }
    }

 public:
    /// Initialize a new BLIP synthesizer.
    BLIPSynthesizer() { memset(impulses, 0, sizeof impulses); }

    /// Set the volume to a new value.
    ///
    /// @param new_unit the new volume level to use
    ///
    void set_volume(double new_unit) {
        // normalize the new unit by the range
        new_unit = new_unit / abs(DYNAMIC_RANGE);
        // return if the volume has not changed
        if (new_unit == volume_unit) return;
        // use default equalizer if it hasn't been set yet
        if (!kernel_unit) set_treble_eq(BLIPEqualizer<float>(-8.0));
        // set the volume
        volume_unit = new_unit;
        double factor = new_unit * (1L << BLIPBuffer::SAMPLE_BITS) / kernel_unit;
        if (factor > 0.0) {
            int32_t shift = 0;
            while (factor < 2.0) {  // unit is small -> attenuate kernel
                shift++;
                factor *= 2.0;
            }
            if (shift) {
                kernel_unit >>= shift;
                if (kernel_unit <= 0)
                    throw Exception("volume level is too low");
                // keep values positive to avoid round-towards-zero of
                // sign-preserving right shift for negative values
                int32_t offset_hi = 0x8000 + (1 << (shift - 1));
                int32_t offset_lo = 0x8000 >> shift;
                for (int32_t i = impulses_size(); i > 0; i--)
                    impulses[i] = ((impulses[i] + offset_hi) >> shift) - offset_lo;
                adjust_impulse();
            }
        }
        // set the integer-valued delta factor based on the floor of the factor
        // using an epsilon value of 0.5 to account for numerical imprecision.
        delta_factor = floor(factor + 0.5f);
    }

    /// @brief Set treble equalization for the synthesizer.
    ///
    /// @param equalizer the equalization parameter for the synthesizer
    ///
    void set_treble_eq(const BLIPEqualizer<float>& equalizer) {
        static constexpr int32_t HALF_SIZE = BLIPBuffer::RESOLUTION / 2 * (QUALITY - 1);
        float fimpulse[BLIPBuffer::RESOLUTION / 2 * (BLIPBuffer::WIDEST_IMPULSE - 1) + BLIPBuffer::RESOLUTION * 2];
        equalizer(&fimpulse[BLIPBuffer::RESOLUTION], HALF_SIZE);
        int32_t i;
        // need mirror slightly past center for calculation
        for (i = BLIPBuffer::RESOLUTION; i > 0; i--)
            fimpulse[BLIPBuffer::RESOLUTION + HALF_SIZE + i] = fimpulse[BLIPBuffer::RESOLUTION + HALF_SIZE - 1 - i];
        // starts at 0
        for (i = 0; i < BLIPBuffer::RESOLUTION; i++)
            fimpulse[i] = 0;
        // find rescale factor
        double total = 0;
        for (i = 0; i < HALF_SIZE; i++)
            total += fimpulse[BLIPBuffer::RESOLUTION + i];

        // static constexpr double BASE_UNIT = 44800 - 128 * 18; // allows treble up to +0 dB
        // static constexpr double BASE_UNIT = 37888; // allows treble to +5 dB
        static constexpr double BASE_UNIT = 32768; // necessary for blip_unscaled to work
        double rescale = BASE_UNIT / 2 / total;
        kernel_unit = floor(BASE_UNIT);

        // integrate, first difference, rescale, quantize
        double sum = 0;
        double next = 0;
        for (i = 0; i < impulses_size(); i++) {
            impulses[i] = floor((next - sum) * rescale + 0.5);
            sum += fimpulse[i];
            next += fimpulse[i + BLIPBuffer::RESOLUTION];
        }
        adjust_impulse();

        // volume might require rescaling
        const double volume_unit = this->volume_unit;
        if (volume_unit) {
            this->volume_unit = 0;
            set_volume(volume_unit);
        }
    }

    /// Set the buffer used for output.
    ///
    /// @param buffer_ the buffer that this synthesizer will write samples to
    ///
    inline void set_output(BLIPBuffer* buffer_) {
        buffer = buffer_;
        last_amp = 0;
    }

    /// Get the buffer used for output.
    ///
    /// @returns the buffer that this synthesizer is writing samples to
    ///
    inline BLIPBuffer* get_output() const { return buffer; }

    /// @brief Add an amplitude transition of specified delta into the buffer.
    ///
    /// @param time the amount of time between this sample and the last
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    /// @param blip_buffer the buffer to write the data into
    /// @details
    /// Works directly in terms of fractional output samples.
    /// Contact Shay Green for more info.
    ///
    void offset_resampled(uint32_t time, int32_t delta, BLIPBuffer* blip_buffer) const {
        static constexpr int32_t fwd = (BLIPBuffer::WIDEST_IMPULSE - QUALITY) / 2;
        static constexpr int32_t rev = fwd + QUALITY - 2;
        static constexpr int32_t mid = QUALITY / 2 - 1;
        // ensure the time is valid with respect to the accuracy of the buffer
        if (!((time >> BLIPBuffer::ACCURACY) < 1))
            throw Exception("time goes beyond end of buffer");
        // update the delta by the delta factor and cache necessary structures
        delta *= delta_factor;
        int32_t* const BLIP_RESTRICT buffer = blip_buffer->get_buffer() + (time >> BLIPBuffer::ACCURACY);
        const int32_t phase = (time >> (BLIPBuffer::ACCURACY - BLIPBuffer::PHASE_BITS) & (BLIPBuffer::RESOLUTION - 1));
        const int16_t* BLIP_RESTRICT imp = impulses + BLIPBuffer::RESOLUTION - phase;

        #if defined(_M_IX86)    || \
            defined(_M_IA64)    || \
            defined(__i486__)   || \
            defined(__x86_64__) || \
            defined(__ia64__)   || \
            defined(__i386__)  // CISC (true)
            // straight forward implementation resulted in better code on GCC for x86
            #define ADD_IMP(out, in) \
                buffer[out] += (int32_t) imp[BLIPBuffer::RESOLUTION * (in)] * delta

            #define BLIP_FWD(i) {\
                ADD_IMP(fwd     + i, i    );\
                ADD_IMP(fwd + 1 + i, i + 1);\
            }
            #define BLIP_REV(r) {\
                ADD_IMP(rev     - r, r + 1);\
                ADD_IMP(rev + 1 - r, r    );\
            }

            BLIP_FWD(0)
            if (QUALITY > 8 ) BLIP_FWD(2)
            if (QUALITY > 12) BLIP_FWD(4) {
                ADD_IMP(fwd + mid - 1, mid - 1);
                ADD_IMP(fwd + mid    , mid    );
                imp = impulses + phase;
            }
            if (QUALITY > 12) BLIP_REV(6)
            if (QUALITY > 8 ) BLIP_REV(4)
            BLIP_REV(2)

            ADD_IMP(rev    , 1);
            ADD_IMP(rev + 1, 0);

        #else  // CISC (false), i.e., RISC
            // for RISC processors, help compiler by reading ahead of writes
            #define BLIP_FWD(i) {\
                int32_t t0 =                      i0 * delta + buffer[fwd     + i];\
                int32_t t1 = imp[BLIPBuffer::RESOLUTION * (i + 1)] * delta + buffer[fwd + 1 + i];\
                i0 =           imp[BLIPBuffer::RESOLUTION * (i + 2)];\
                buffer[fwd     + i] = t0;\
                buffer[fwd + 1 + i] = t1;\
            }
            #define BLIP_REV(r) {\
                int32_t t0 =                i0 * delta + buffer[rev     - r];\
                int32_t t1 = imp[BLIPBuffer::RESOLUTION * r] * delta + buffer[rev + 1 - r];\
                i0 =           imp[BLIPBuffer::RESOLUTION * (r - 1)];\
                buffer[rev     - r] = t0;\
                buffer[rev + 1 - r] = t1;\
            }

            int32_t i0 = *imp;
            BLIP_FWD(0)
            if (QUALITY > 8 ) BLIP_FWD(2)
            if (QUALITY > 12) BLIP_FWD(4) {
                int32_t t0 =                  i0 * delta + buffer[fwd + mid - 1];
                int32_t t1 = imp[BLIPBuffer::RESOLUTION * mid] * delta + buffer[fwd + mid    ];
                imp = impulses + phase;
                i0 = imp[BLIPBuffer::RESOLUTION * mid];
                buffer[fwd + mid - 1] = t0;
                buffer[fwd + mid    ] = t1;
            }
            if (QUALITY > 12) BLIP_REV(6)
            if (QUALITY > 8 ) BLIP_REV(4)
            BLIP_REV(2)

            int32_t t0 =   i0 * delta + buffer[rev    ];
            int32_t t1 = *imp * delta + buffer[rev + 1];
            buffer[rev    ] = t0;
            buffer[rev + 1] = t1;
        #endif  // CISC

        #undef BLIP_FWD
        #undef BLIP_REV
    }

    /// @brief Add an amplitude transition of specified delta into specified
    /// buffer rather than the instance buffer.
    ///
    /// @param time the amount of time between this sample and the last
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    /// @param buffer the buffer to write the data into
    ///
    inline void offset(int32_t time, int32_t delta, BLIPBuffer* buffer) const {
        offset_resampled(buffer->get_factor() * time, delta, buffer);
    }

    /// @brief Add an amplitude transition of specified delta.
    ///
    /// @param time the amount of time between this sample and the last
    /// @param delta the change in amplitude. can be positive or negative.
    /// The actual change in amplitude is delta * (volume / range)
    ///
    inline void offset(int32_t time, int32_t delta) const {
        offset(time, delta, buffer);
    }

    /// Update amplitude of waveform at given time. Using this requires a
    /// separate BLIPSynthesizer for each waveform.
    ///
    /// @param time the amount of time between this sample and the last
    /// @param amplitude the amplitude of the waveform to synthesizer
    ///
    inline void update(int32_t time, int32_t amplitude) {
        const int32_t delta = amplitude - last_amp;
        last_amp = amplitude;
        offset_resampled(time * buffer->get_factor(), delta, buffer);
    }
};

#undef BLIP_RESTRICT

#endif  // DSP_BLIP_BUFFER_HPP_
