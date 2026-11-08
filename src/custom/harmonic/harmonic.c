/*
 * Harmonic: complementary-band harmonic tremolo for Zoom MultiStomp.
 *
 * A one-pole low-pass creates low and complementary high bands. The bands are
 * amplitude-modulated in opposite phase by a shared LFO, producing the
 * characteristic moving spectral notch rather than ordinary full-band tremolo.
 * Four controls: Rate, Depth, Tone (crossover), Mix.
 *
 * Safe-DSP: scalar state only in ctx[3], no math library, no runtime divide,
 * no arrays or writable globals. Preserve the ctx[11]/ctx[12] magic shuttle.
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
#define HARMONIC_MAGIC 0x484D5431u
#define HARMONIC_VERSION 1u
#define HARMONIC_TWO_PI 6.28318530718f
#define HARMONIC_TWO_PI_SR 0.0001424758573f

typedef struct HarmonicState {
    uint32_t magic;
    uint32_t version;
    uint32_t initialized;
    uint32_t reserved;
    float phase;
    float lowL;
    float lowR;
} HarmonicState;

static inline float harmonic_abs(float x)
{
    return x < 0.0f ? -x : x;
}

static inline float harmonic_sin(float x)
{
    const float twoPi = 6.28318530718f;
    const float pi = 3.14159265359f;
    const float invTwoPi = 0.15915494309f;
    x -= twoPi * (float)((int)(x * invTwoPi));
    if (x < -pi) x += twoPi;
    if (x > pi) x -= twoPi;
    float y = 1.2732395447f * x - 0.4052847346f * x * harmonic_abs(x);
    return y + 0.225f * (y * harmonic_abs(y) - y);
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
    float tone = zoom_param_norm01(params[HARMONIC_TONE_SLOT], HARMONIC_TONE_DEFAULT_NORM);
    float mix = zoom_param_norm01(params[HARMONIC_MIX_SLOT], HARMONIC_MIX_DEFAULT_NORM);

    /* 0.2..8 Hz; precomputed 2*pi/44100 avoids a runtime divide. */
    float phaseInc = (0.2f + 7.8f * rate) * HARMONIC_TWO_PI_SR;
    /* One-pole coefficient sweep: darker/low crossover to brighter/high. */
    float lpCoeff = 0.015f + 0.285f * tone;
    float dry = 1.0f - mix;
    float wet = mix;
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

        float lfo = harmonic_sin(phase);
        float lowGain = 1.0f - depth * (0.5f + 0.5f * lfo);
        float highGain = 1.0f - depth * (0.5f - 0.5f * lfo);
        float effectedL = lowL * lowGain + highL * highGain;
        float effectedR = lowR * lowGain + highR * highGain;

        fxBuf[i] = dry * inL + wet * effectedL;
        fxBuf[i + 8] = dry * inR + wet * effectedR;

        phase += phaseInc;
        if (phase >= HARMONIC_TWO_PI) phase -= HARMONIC_TWO_PI;
    }

    st->phase = phase;
    st->lowL = lowL;
    st->lowR = lowR;
}
