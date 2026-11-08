#include <math.h>
#include <stdio.h>
#define HARMONIC_EXPAND_PRAGMA(x)
#include "harmonic_wave.h"

static int near(float a, float b, float eps)
{
    return fabsf(a - b) <= eps;
}

int main(void)
{
    const float pi = HARMONIC_PI;
    const float phases[] = {0.0f, pi * 0.25f, pi * 0.5f, pi, pi * 1.5f, pi * 1.999f};
    int i;

    /* Endpoint and landmark checks exercise the exact functions included by DSP. */
    if (!near(harmonic_wave(pi * 0.5f, 0.0f), 1.0f, 0.06f)) return 1;      /* sine */
    if (!near(harmonic_wave(pi * 0.5f, 0.33333334f), 0.0f, 0.01f)) return 2; /* triangle */
    if (!near(harmonic_wave(pi * 0.25f, 0.66666669f), -1.0f, 0.01f)) return 3; /* square */
    if (!near(harmonic_wave(pi, 1.0f), 0.0f, 0.01f)) return 4;             /* ramp */
    if (!near(harmonic_wave(pi * 0.25f, 0.0f), harmonic_sin(pi * 0.25f), 0.001f)) return 5;

    /* Full phase/shape sweep: finite and bounded outputs, including morph joins. */
    for (i = 0; i <= 2000; ++i) {
        float phase = HARMONIC_TWO_PI * ((float)i / 2001.0f);
        int j;
        for (j = 0; j <= 100; ++j) {
            float shape = (float)j * 0.01f;
            float y = harmonic_wave(phase, shape);
            if (!(y == y) || y < -1.08f || y > 1.08f) {
                fprintf(stderr, "invalid waveform: phase=%g shape=%g value=%g\n", phase, shape, y);
                return 6;
            }
        }
    }
    puts("Harmonic waveform tests passed (actual DSP header; 200,301 phase/shape points).");
    return 0;
}
