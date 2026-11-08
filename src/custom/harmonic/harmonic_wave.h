/* Shared actual LFO functions used by pedal DSP and host-side tests. */
#ifndef HARMONIC_WAVE_H
#define HARMONIC_WAVE_H

#define HARMONIC_PI 3.14159265359f
#define HARMONIC_TWO_PI 6.28318530718f
#define HARMONIC_ALWAYS_INLINE(f) HARMONIC_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

HARMONIC_ALWAYS_INLINE(harmonic_abs)
static inline float harmonic_abs(float x)
{
    return x < 0.0f ? -x : x;
}

/* Parabolic sine approximation; phase must be wrapped to [0, 2*pi). */
HARMONIC_ALWAYS_INLINE(harmonic_sin)
static inline float harmonic_sin(float phase)
{
    float x = phase;
    if (x > HARMONIC_PI) x -= HARMONIC_TWO_PI;
    float y = 1.2732395447f * x - 0.4052847346f * x * harmonic_abs(x);
    return y + 0.225f * (y * harmonic_abs(y) - y);
}

/* Shape 0..1 morphs sine -> triangle -> square -> rising ramp. */
HARMONIC_ALWAYS_INLINE(harmonic_wave)
static inline float harmonic_wave(float phase, float shape)
{
    float sine = harmonic_sin(phase);
    float triangle;
    float square;
    float ramp = phase * (1.0f / HARMONIC_PI) - 1.0f;
    float t;

    if (phase < HARMONIC_PI) triangle = -1.0f + phase * (2.0f / HARMONIC_PI);
    else triangle = 3.0f - phase * (2.0f / HARMONIC_PI);
    square = phase < HARMONIC_PI ? -1.0f : 1.0f;

    if (shape < 0.33333334f) {
        t = shape * 3.0f;
        return sine + t * (triangle - sine);
    }
    if (shape < 0.66666669f) {
        t = (shape - 0.33333334f) * 3.0f;
        return triangle + t * (square - triangle);
    }
    t = (shape - 0.66666669f) * 3.0f;
    return square + t * (ramp - square);
}

#endif
