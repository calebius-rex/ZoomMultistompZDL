/*
 * Harmonic: Monument V2-inspired harmonic tremolo for Zoom MultiStomp.
 *
 * Controls: Rate, Depth, Shape, Volume, Division, Mode. Division scales the
 * free-running Rate control (quarter, triplet, eighth); this is not tap tempo.
 * Mode selects standard full-band tremolo or complementary-band harmonic
 * tremolo. This is an original DSP approximation, not a Walrus Audio port.
 *
 * Safe-DSP: scalar persistent state in ctx[3], no math library, no arrays or
 * writable globals, no runtime divide, no switch/jump table. Preserve the
 * ctx[11]/ctx[12] magic shuttle.
 */
#include <stdint.h>

/* harmonic_wave.h uses this macro to request TI's always-inline pragma.
 * Define it before including that header so the compiler sees the expansion. */
#define HARMONIC_DO_PRAGMA(x) _Pragma(#x)
#define HARMONIC_EXPAND_PRAGMA(x) HARMONIC_DO_PRAGMA(x)
#define HARMONIC_ALWAYS_INLINE(f) HARMONIC_EXPAND_PRAGMA(FUNC_ALWAYS_INLINE(f))

#include "../../airwindows/common/zoom_params.h"
#include "harmonic_params.h"
#include "harmonic_wave.h"

#ifndef HARMONIC_AUDIO_FUNC
#define HARMONIC_AUDIO_FUNC Fx_DLY_Harmonic
#endif

#define HARMONIC_CODE_SECTION(f) HARMONIC_EXPAND_PRAGMA(CODE_SECTION(f, ".audio"))
HARMONIC_CODE_SECTION(HARMONIC_AUDIO_FUNC)

#define ZDL_PTR(type, word) ((type)(uintptr_t)(word))
#define HARMONIC_MAGIC 0x484D5433u
#define HARMONIC_VERSION 3u
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

HARMONIC_ALWAYS_INLINE(harmonic_division_multiplier)
static inline float harmonic_division_multiplier(float division)
{
    /* Quarter = 1x, quarter-note triplet = 1.5x, eighth = 2x. */
    if (division < 0.33333334f) return 1.0f;
    if (division < 0.66666669f) return 1.5f;
    return 2.0f;
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
    float division = zoom_param_norm01(params[HARMONIC_DIVISION_SLOT], HARMONIC_DIVISION_DEFAULT_NORM);
    float mode = zoom_param_norm01(params[HARMONIC_MODE_SLOT], HARMONIC_MODE_DEFAULT_NORM);

    /* Rate sets quarter-note speed (0.5..12 Hz); Division multiplies it. */
    float lfoHz = (0.5f + 11.5f * rate) * harmonic_division_multiplier(division);
    if (lfoHz > 24.0f) lfoHz = 24.0f;
    float phaseInc = lfoHz * HARMONIC_TWO_PI_SR;
    float lpCoeff = 0.08f;
    /* Unity at centre, attenuation below, boost above. */
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
        float harmonicL = lowL * lowGain + highL * highGain;
        float harmonicR = lowR * lowGain + highR * highGain;
        float tremoloGain = 1.0f - depth * (0.5f + 0.5f * lfo);

        /* Mode=0 is Standard (all frequencies together); Mode=1 is Harmonic. */
        float outL = mode < 0.5f ? inL * tremoloGain : harmonicL;
        float outR = mode < 0.5f ? inR * tremoloGain : harmonicR;
        fxBuf[i] = outGain * outL;
        fxBuf[i + 8] = outGain * outR;

        phase += phaseInc;
        if (phase >= HARMONIC_TWO_PI) phase -= HARMONIC_TWO_PI;
    }

    st->phase = phase;
    st->lowL = lowL;
    st->lowR = lowR;
}
