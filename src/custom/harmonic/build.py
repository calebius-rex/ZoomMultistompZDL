#!/usr/bin/env python3
"""Build Harmonic.ZDL from harmonic.c + manifest_pedal.json.

Set TI_CGT_ROOT to the TI C6000 compiler installation directory when it is not
installed at the default location. On Windows, for example:
    $env:TI_CGT_ROOT = 'C:\\ti\\ti-cgt-c6000_8.5.0.LTS'
"""

from __future__ import annotations

import json
import math
import os
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))

from linker import LinkerConfig, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402
import custom_covers  # noqa: E402

DEFAULT_TI_ROOTS = [
    Path(os.environ.get("TI_CGT_ROOT", "")) if os.environ.get("TI_CGT_ROOT") else None,
    Path("C:/ti/ti-cgt-c6000_8.5.0.LTS"),
    Path("C:/ti/ti-cgt-c6000_8.5.0"),
    Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS"),
]
TI_ROOT = next((p for p in DEFAULT_TI_ROOTS if p and
                any((p / "bin" / exe).is_file() for exe in ("cl6x.exe", "cl6x"))),
               DEFAULT_TI_ROOTS[0] or Path("C:/ti/ti-cgt-c6000_8.5.0.LTS"))
CL6X = next((TI_ROOT / "bin" / exe for exe in ("cl6x.exe", "cl6x")
             if (TI_ROOT / "bin" / exe).is_file()), TI_ROOT / "bin" / "cl6x")
CFLAGS = [
    "--c99", "--opt_level=2", "-mv6740", "--abi=eabi",
    "--mem_model:data=far", f"--include_path={TI_ROOT / 'include'}",
]


def _harmonic_emblem(canvas) -> None:
    """Draw two opposed harmonic-tremolo traces in the standard cover emblem."""
    previous_low = None
    previous_high = None
    for x in range(43, 86):
        phase = (x - 43) * (2.0 * math.pi / 28.0)
        y_low = int(round(23 - 7 * math.sin(phase)))
        y_high = int(round(23 + 7 * math.sin(phase)))
        if previous_low is not None:
            custom_covers._line(canvas, previous_low[0], previous_low[1], x, y_low)
            custom_covers._line(canvas, previous_high[0], previous_high[1], x, y_high)
        previous_low = (x, y_low)
        previous_high = (x, y_high)
    canvas.vline(64, 15, 31)
    canvas.hline(61, 67, 23)


def main() -> None:
    if not CL6X.is_file():
        raise FileNotFoundError(
            f"TI C6000 compiler not found: {CL6X}\n"
            "Install TI C6000 CGT, then set TI_CGT_ROOT to its install folder. "
            "For example in PowerShell: $env:TI_CGT_ROOT = 'C:\\ti\\ti-cgt-c6000_8.5.0.LTS'"
        )

    manifest = json.loads((HERE / "manifest_pedal.json").read_text())
    write_param_header(manifest, HERE / "harmonic_params.h", "HARMONIC")

    src_c = HERE / "harmonic.c"
    out_dir = ROOT / "dist"
    out_dir.mkdir(exist_ok=True)
    effect_name = manifest["effect_name"]
    audio_func = manifest["audio_func_name"]
    obj = HERE / f"{effect_name.lower()}.obj"
    out_zdl = out_dir / f"{effect_name}.ZDL"

    print(f"[harmonic] compiling {src_c.name} -> {obj.name}")
    subprocess.run(
        [str(CL6X), *CFLAGS, f"--define=HARMONIC_AUDIO_FUNC={audio_func}",
         "-c", str(src_c), f"--output_file={obj}"],
        check=True, cwd=HERE,
    )
    for junk in ("compiler.opt", "linker.cmd"):
        p = HERE / junk
        if p.exists():
            p.unlink()

    custom_covers.EMBLEMS["Harmonic"] = _harmonic_emblem
    cfg = LinkerConfig(
        materialize_init=True,
        effect_name=effect_name,
        screen_image=custom_covers.make_cover(effect_name, [p["name"] for p in manifest["params"]]),
        audio_func_name=audio_func,
        gid=manifest["gid"],
        fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj,
        output_path=out_zdl,
        fxid_version=manifest.get("fxid_version", "1.00").encode("ascii"),
        flags_byte=manifest.get("flags_byte", 0x01),
        audio_nop=manifest.get("audio_nop", False),
        knob_positions=[(2, 14, 46), (3, 55, 46), (4, 96, 46)],
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__",
    )
    link(cfg)
    print(f"\n[harmonic] done -> {out_zdl}")
    print("[harmonic] cover includes opposed-band waveform emblem and Rate/Depth/Shape labels")


if __name__ == "__main__":
    main()
