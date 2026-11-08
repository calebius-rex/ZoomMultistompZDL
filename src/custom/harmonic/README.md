# Harmonic Tremolo

Monument V2-inspired harmonic tremolo approximation with original DSP. The four Zoom controls are Rate, Depth, Shape, and Volume. Harmonic mode uses complementary low/high bands modulated in opposite phase; Shape morphs sine, triangle, square, and ramp. Tap tempo, divisions, the Monument waveform-range toggle, and standard mode are not reproduced.

## Build on Windows

Install TI C6000 CGT, including `bin\cl6x.exe`, plus the repository's required local linker files. In PowerShell, set the TI compiler root to the folder containing `bin` and `include`, then run the builder:

```powershell
$env:TI_CGT_ROOT = 'C:\ti\ti-cgt-c6000_8.5.0.LTS'
python src\custom\harmonic\build.py
```

Use your actual TI installation path. The direct builder writes `dist\Harmonic.ZDL` and embeds the generated 128×64 cover. `python build_all.py harmonic` treats Harmonic as experimental and moves it to `build\probes\Harmonic.ZDL`.

A successful link is not hardware validation. Inspect the object/disassembly for unsafe helpers, verify the ZDL metadata and ID against the target device, then test carefully on supported hardware before treating the sound or stability as confirmed.
