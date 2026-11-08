# Harmonic Tremolo

A custom Zoom MultiStomp effect that splits audio into complementary low and high bands and modulates them in opposite phase.

## Build

With the TI C6000 compiler installed at the path configured in `build.py`, run this direct builder from the repository root:

```sh
python3 src/custom/harmonic/build.py
```

A successful direct build writes `dist/Harmonic.ZDL`. It also builds the 128×64 monochrome ZDL cover with an opposed-wave emblem and the first three control labels.

`python3 build_all.py harmonic` treats Harmonic as experimental, so that command relocates its output to `build/probes/Harmonic.ZDL` rather than leaving it in `dist/`. This is intentional to avoid accidentally shipping an unvalidated effect. Hardware listening and safe-DSP checks are still required before moving it into the release list or installing it on a pedal.

The builder requires the project's real linker, shared cover and parameter helpers, and TI CGT. Do not use any stub linker or placeholder ZDL.
