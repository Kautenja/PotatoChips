
#ifndef BLIP_BUFFER_SINC_HPP_
#define BLIP_BUFFER_SINC_HPP_

#include <cmath>

/// the constant value for Pi
static constexpr double BLARGG_PI = 3.1415926535897932384626433832795029;

/// Generate a sinc.
///
/// @param out TODO:
/// @param count TODO:
/// @param oversample TODO:
/// @param treble TODO:
/// @param cutoff TODO:
///
static void gen_sinc(
    float* out,
    int count,
    double oversample,
    double treble,
    double cutoff
) {
    if (cutoff >= 0.999)
        cutoff = 0.999;
    if (treble < -300.0)
        treble = -300.0;
    if (treble > 5.0)
        treble = 5.0;

    static constexpr double maxh = 4096.0;
    double const rolloff = pow(10.0, 1.0 / (maxh * 20.0) * treble / (1.0 - cutoff));
    double const pow_a_n = pow(rolloff, maxh - maxh * cutoff);
    double const to_angle = BLARGG_PI / 2 / maxh / oversample;
    for (int i = 0; i < count; i++) {
        double angle = ((i - count) * 2 + 1) * to_angle;
        double c = rolloff * cos((maxh - 1.0) * angle) - cos(maxh * angle);
        double cos_nc_angle = cos(maxh * cutoff * angle);
        double cos_nc1_angle = cos((maxh * cutoff - 1.0) * angle);
        double cos_angle = cos(angle);

        c = c * pow_a_n - rolloff * cos_nc1_angle + cos_nc_angle;
        double d = 1.0 + rolloff * (rolloff - cos_angle - cos_angle);
        double b = 2.0 - cos_angle - cos_angle;
        double a = 1.0 - cos_angle - cos_nc_angle + cos_nc1_angle;

        out[i] = (float) ((a * d + c * b) / (b * d)); // a / b + c / d
    }
}

#endif  // BLIP_BUFFER_SINC_HPP_
