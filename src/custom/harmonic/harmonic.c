/*
 * Harmonic: Monument V2-inspired harmonic tremolo for Zoom MultiStomp.
 *
 * Four control mapping: Rate, Depth, Shape, Volume. This reproduces the broad
 * harmonic-tremolo behavior (complementary low/high bands modulated oppositely)
 * with an original lightweight DSP approximation; it is not a copy of Walrus
 * Audio's proprietary circuit or firmware.
 *
 * Shape continuously blends sine -> triangle -> square -> ramp. The Monument's
 * waveform-range and division toggles, tap-tempo switch, and standard-tremolo
 * mode are not represented: this pedal effect uses the four available knobs for
 * the four main controls and runs harmonic mode only.
 *
 * Safe-DSP: scalar persistent state in ctx[3], no math library, no arrays or
 * writable globals, no runtime divide, no switch/jump table. Preserve the
 * ctx[11]/ctx[12] magic shuttle.
 */
#include <stdint.h>
#include "../../airwindows/common/zoom_params.h"
#include "harmonic_params.h"

#ifndef HARMONIC_AUDIO_FUNC
#define HARMONIC_AUDIO_FUNC Fx_DLY_Harmonic
#endif

#define HARMONIC_DO_PRAGMA(x) _Pragma(#x)
#define HARMONIC_EXPAND_PRAGMA(x) HARMONIC_DO_PRAGMA(x)
#define HARMONIC_CODE_SECTION(f) HARMONIC_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
HARMONIC_CODE_SECTION(HARMONIC_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))
#define HARMONIC_MAGIC 0x484D5432u
#define HARMONIC_VERSION 2u
#define HARMONIC_TWO_PI 6.28318530718f
#define HARMONIC_TWO_PI_SR 0.0001424758573f
#define HARMONIC_PI 3.14159265359f

#define HARMONIC_ALWAYS_INLINE(f) HARMONIC_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

typedef struct HarmonicState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t reserved;
    float phase;
    float lowL;
    float lowR;
} HarmonicState;

HARMONIC_ALWAYS_INLINE(harmonic_abs)
static inline float harmonic_abs(float x)
{
    return x < 0.0f ? -x : x;
}

/* Parabolic sine approximation; phase is wrapped into [0, 2*pi). */
HARMONIC_ALWAYS_INLINE(harmonic_sin)
static inline float harmonic_sin(float phase)
{
    float x = phase;
    if (x > HARMONIC_PI) x -= HARMONIC_TWO_PI;
    float y = 1.2732395447f * x - 0.4052847346f * x * harmonic_abs(x);
    return y + 0.225f * (y * harmonic_abs(y) - y);
}

/* Blend adjacent LFO shapes without tables or indirect branches. */
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

void HARMONIC_AUDIO_FUNC(unsigned int *ctx)
{
    float *params = ZDL_PTR(float *, ctx[1]);
    float *fxBuf = ZDL_PTR(float *, ctx[5]);

    unsigned int *magicSrc = ZDL_PTR(unsigned int *, ctx[12]);
    unsigned int *magicDst = ZDL_PTR(unsigned int *, *(unsigned int *)ZDL_PTR(unsigned int *, ctx[11]));
    *magicDst = *magicSrc;

    if (params[0] < 0.5f) return;

    volatile unsigned int *desc = ZDL_PTR(volatile unsigned int *, ctx[3]);
    if (!desc) return;
    uintptr_t base = (uintptr_t)desc[0];
    uintptr_t end = (uintptr_t)desc[1];
    if (base == 0u || end <= base || (base & 3u) != 0u) return;
    if (end - base < (uintptr_t)sizeof(HarmonicState)) return;

    HarmonicState *st = (HarmonicState *)base;
    if (st->magic != HARMONIC_MAGIC || st->version != HARMONIC_VERSION || !st->initialized) {
        st->magic = HARMONIC_MAGIC;
        st->version = HARMONIC_VERSION;
        st->phase = 0.0f;
        st->lowL = 0.0f;
        st->lowR = 0.0f;
        st->initialized = 1u;
    }

    float rate = zoom_param_norm01(params[HARMONIC_RATE_SLOT], HARMONIC_RATE_DEFAULT_NORM);
    float depth = zoom_param_norm01(params[HARMONIC_DEPTH_SLOT], HARMONIC_DEPTH_DEFAULT_NORM);
    float shape = zoom_param_norm01(params[HARMONIC_SHAPE_SLOT], HARMONIC_SHAPE_DEFAULT_NORM);
    float volume = zoom_param_norm01(params[HARMONIC_VOLUME_SLOT], HARMONIC_VOLUME_DEFAULT_NORM);

    /* 0.5..12 Hz; fixed crossover is intentionally not an extra control. */
    float phaseInc = (0.5f + 11.5f * rate) * HARMONIC_TWO_PI_SR;
    float lpCoeff = 0.08f;
    /* Unity at knob centre, with useful cut/boost range either side. */
    float outGain = 0.5f + volume;
    float phase = st->phase;
    float lowL = st->lowL;
    float lowR = st->lowR;

    int i;
    for (i = 0; i < 8; ++i) {
        float inL = fxBuf[i];
        float inR = fxBuf[i + 8];
        lowL += lpCoeff * (inL - lowL);
        lowR += lpCoeff * (inR - lowR);
        float highL = inL - lowL;
        float highR = inR - lowR;

        float lfo = harmonic_wave(phase, shape);
        float lowGain = 1.0f - depth * (0.5f + 0.5f * lfo);
        float highGain = 1.0f - depth * (0.5f - 0.5f * lfo);
        fxBuf[i] = outGain * (lowL * lowGain + highL * highGain);
        fxBuf[i + 8] = outGain * (lowR * lowGain + highR * highGain);

        phase += phaseInc;
        if (phase >= HARMONIC_TWO_PI) phase -= HARMONIC_TWO_PI;
    }

    st->phase = phase;
    st->lowL = lowL;
    st->lowR = lowR;
}
