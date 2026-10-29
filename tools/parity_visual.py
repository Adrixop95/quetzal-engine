#!/usr/bin/env python3
"""L4/L5 comparison and isolated capture probes. All run output is private."""
import argparse
import json
import math
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import zlib

# Direct execution and importlib-based tests use the same sibling helper.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import ROOT, execute, json_object, private_game, private_output, write_json

REGIONS = ("map", "units", "hud", "minimap", "command_card", "overlays")
ALIGNMENT = {"map", "setup", "phase", "camera", "selection", "money", "side", "ui_scale",
             "resolution", "light", "animation_ms", "bindings", "tooltip_ms", "messages"}
STATE = {"buttons", "cursor", "tooltip", "menu", "target_mode"}
MAX_PIXELS = 4096 * 4096


def require(condition, message):
    if not condition:
        raise ValueError(message)


def load(path):
    value = json_object(path.read_text())
    # Reject non-JSON numeric constants, including nested command arguments.
    json.dumps(value, allow_nan=False)
    return value


def identical(a, b):
    """JSON equality preserves boolean/integer and integer/float distinctions."""
    return json.dumps(a, sort_keys=True, allow_nan=False) == json.dumps(b, sort_keys=True, allow_nan=False)


def image_file(path):
    """Read runtime P6/P5 and clone RGB/RGBA8 PNG without optional packages."""
    data = path.read_bytes()
    if data.startswith((b"P6", b"P5")):
        match = re.match(rb"P([56])\s+(\d+)\s+(\d+)\s+255(?:\r\n|\s)", data)
        require(match is not None, "expected binary P5/P6 with max value 255")
        mode, w, h = map(int, match.groups())
        channels = 3 if mode == 6 else 1
        pixels = data[match.end():]
        require(0 < w * h <= MAX_PIXELS and len(pixels) == w * h * channels, "invalid raster length")
        return w, h, channels, pixels
    require(data.startswith(b"\x89PNG\r\n\x1a\n"), "unsupported image format")
    offset, packed, header, ended = 8, bytearray(), None, False
    while offset < len(data):
        require(offset + 12 <= len(data), "truncated PNG chunk")
        length, = struct.unpack_from('>I', data, offset)
        kind = data[offset + 4:offset + 8]
        chunk = data[offset + 8:offset + 8 + length]
        require(offset + 12 + length <= len(data), "truncated PNG payload")
        crc, = struct.unpack_from('>I', data, offset + 8 + length)
        require(zlib.crc32(kind + chunk) == crc, "PNG checksum mismatch")
        if kind == b'IHDR':
            require(header is None and offset == 8 and length == 13, "invalid PNG header")
            header = struct.unpack('>IIBBBBB', chunk)
        elif kind == b'IDAT':
            packed.extend(chunk)
        elif kind == b'IEND':
            ended = True
            require(length == 0 and offset + 12 == len(data), "invalid PNG end")
            break
        elif kind[0] & 32 == 0:
            raise ValueError("unsupported critical PNG chunk")
        offset += length + 12
    require(header is not None and ended, "incomplete PNG")
    w, h, depth, color, compression, filtering, interlace = header
    require(0 < w * h <= MAX_PIXELS and w > 0 and h > 0 and depth == 8 and color in (2, 6)
            and (compression, filtering, interlace) == (0, 0, 0), "unsupported PNG encoding")
    channels = 3 if color == 2 else 4
    stride = w * channels
    decoder = zlib.decompressobj()
    raw = decoder.decompress(packed, (stride + 1) * h + 1)
    require(decoder.eof and not decoder.unused_data and len(raw) == (stride + 1) * h, "invalid PNG deflate length")
    previous, rgb = bytearray(stride), bytearray()
    for y in range(h):
        start = y * (stride + 1)
        filter_id = raw[start]
        require(filter_id <= 4, "invalid PNG filter")
        row = bytearray(raw[start + 1:start + stride + 1])
        for i in range(stride):
            a = row[i - channels] if i >= channels else 0
            b = previous[i]
            c = previous[i - channels] if i >= channels else 0
            p = a + b - c
            distances = (abs(p - a), abs(p - b), abs(p - c))
            predictor = (a, b, c)[distances.index(min(distances))]
            row[i] = (row[i] + (0, a, b, (a + b) // 2, predictor)[filter_id]) & 255
        for x in range(w):
            require(channels == 3 or row[x * channels + 3] == 255, "captures must be opaque")
            rgb.extend(row[x * channels:x * channels + 3])
        previous = row
    return w, h, 3, bytes(rgb)


def asset(manifest, name):
    require(isinstance(name, str) and name, "missing capture path")
    path = (manifest.parent / name).resolve()
    require(path.is_relative_to(manifest.parent.resolve()), "capture path escapes bundle")
    return image_file(path)


def validate_spec(spec):
    require(spec.get("schema") == 1 and spec.get("evidence") in ("P", "M", "V")
            and isinstance(spec.get("id"), str) and spec["id"], "invalid scenario")
    require(isinstance(spec.get("alignment"), dict) and set(spec["alignment"]) == ALIGNMENT,
            "scenario must declare all alignment fields")
    alignment = spec["alignment"]
    require(alignment["phase"] == "after_input_before_present", "unsupported observation phase")
    require(isinstance(alignment["resolution"], list) and len(alignment["resolution"]) == 2
            and all(type(v) is int and v > 0 for v in alignment["resolution"])
            and math.prod(alignment["resolution"]) <= MAX_PIXELS, "invalid resolution")
    require(isinstance(alignment["camera"], list) and len(alignment["camera"]) == 3
            and all(type(v) in (int, float) and math.isfinite(v) for v in alignment["camera"])
            and alignment["camera"][2] > 0, "invalid camera")
    require(isinstance(spec.get("regions"), dict) and set(spec["regions"]) == set(REGIONS), "all six regions required")
    for tolerance in spec["regions"].values():
        require(isinstance(tolerance, dict) and set(tolerance) == {"channel", "max_bad_fraction", "max_mae"}, "invalid region thresholds")
        require(type(tolerance["channel"]) is int and 0 <= tolerance["channel"] <= 255, "invalid channel tolerance")
        for key, maximum in (("max_bad_fraction", 1), ("max_mae", 255)):
            v = tolerance[key]
            require(type(v) in (int, float) and math.isfinite(v) and 0 <= v <= maximum, "invalid metric threshold")
    samples = spec.get("observe")
    require(isinstance(samples, list) and samples, "observation points required")
    last = -1
    for sample in samples:
        require(isinstance(sample, dict) and set(sample) == {"step", "tick"} and isinstance(sample["step"], str) and sample["step"]
                and type(sample["tick"]) is int and sample["tick"] > last, "invalid observation schedule")
        last = sample["tick"]
    require(len({s["step"] for s in samples}) == len(samples), "duplicate step")


def validate_bundle(path, spec):
    bundle = load(path)
    require(bundle.get("schema") == 1 and bundle.get("scenario") == spec["id"]
            and isinstance(bundle.get("contract"), str) and bundle["contract"], "invalid bundle header")
    require(identical(bundle.get("alignment"), spec["alignment"]), "unaligned capture: setup, phase or viewing state")
    samples = bundle.get("samples")
    require(isinstance(samples, list) and len(samples) == len(spec["observe"]), "truncated/extra UI observations")
    decoded = []
    for row, expected in zip(samples, spec["observe"], strict=True):
        require(isinstance(row, dict) and row.get("step") == expected["step"] and type(row.get("tick")) is int
                and row["tick"] == expected["tick"], "UI observation schedule differs")
        state = row.get("state")
        require(isinstance(state, dict) and set(state) == STATE, "incomplete UI state")
        require(isinstance(state["buttons"], dict) and state["buttons"], "button observations required")
        for button in state["buttons"].values():
            require(isinstance(button, dict) and set(button) == {"frame", "highlight", "enabled"}
                    and type(button["frame"]) is int and button["frame"] >= 0
                    and type(button["highlight"]) is bool and type(button["enabled"]) is bool,
                    "invalid button observation")
        require(isinstance(state["cursor"], str) and state["cursor"] and isinstance(state["tooltip"], str)
                and isinstance(state["target_mode"], str) and state["target_mode"]
                and isinstance(state["menu"], str) and state["menu"], "invalid UI state types")
        commands = row.get("commands")
        require(isinstance(commands, list), "command list required, including empty observations")
        for command in commands:
            require(isinstance(command, dict) and set(command) == {"name", "arguments"}
                    and isinstance(command["name"], str) and command["name"]
                    and isinstance(command["arguments"], dict), "invalid semantic command")
        image = asset(path, row.get("image"))
        require(list(image[:2]) == spec["alignment"]["resolution"] and image[2] == 3, "capture resolution differs")
        require(isinstance(row.get("masks"), dict) and set(row["masks"]) == set(REGIONS), "all structural masks required")
        masks = {}
        for region in REGIONS:
            mask = asset(path, row["masks"][region])
            require(mask[:2] == image[:2] and mask[2] == 1 and set(mask[3]) <= {0, 255}
                    and 255 in mask[3], "invalid/empty structural mask")
            masks[region] = mask[3]
        decoded.append((row, image[3], masks))
    return bundle["contract"], decoded


def compare(spec_path, reference, clone):
    spec = load(spec_path)
    validate_spec(spec)
    # Decode and validate the complete horizon before returning any differences.
    ac, left = validate_bundle(reference, spec)
    bc, right = validate_bundle(clone, spec)
    require(ac == bc, "normalization contracts differ")
    rows, first = [], None
    for (a, ap, am), (b, bp, bm) in zip(left, right, strict=True):
        metrics = {}
        for region in REGIONS:
            mask_difference = sum(x != y for x, y in zip(am[region], bm[region], strict=True))
            exact, bad, total, error = 0, 0, 0, 0
            tolerance = spec["regions"][region]
            # Union ensures that missing/shifted structure cannot hide pixel error.
            for i, (x, y) in enumerate(zip(am[region], bm[region], strict=True)):
                if not (x or y):
                    continue
                delta = [abs(ap[3 * i + c] - bp[3 * i + c]) for c in range(3)]
                total += 1
                exact += max(delta) != 0
                bad += max(delta) > tolerance["channel"]
                error += sum(delta)
            mae, fraction = error / (3 * total), bad / total
            passed = (mask_difference == 0 and mae <= tolerance["max_mae"]
                      and fraction <= tolerance["max_bad_fraction"])
            metrics[region] = dict(passed=passed, pixels=total, mask_difference=mask_difference,
                                   exact_mismatch_fraction=exact / total, bad_fraction=fraction, mae=mae)
            if not passed and first is None:
                first = dict(tick=a["tick"], step=a["step"], category="visual_structure" if mask_difference else "visual_pixels", field=region)
        fields = [field for field in sorted(STATE) if not identical(a["state"][field], b["state"][field])]
        if not identical(a["commands"], b["commands"]):
            fields.append("commands")
        if fields and first is None:
            first = dict(tick=a["tick"], step=a["step"], category="ui_sequence", field=fields[0])
        rows.append(dict(step=a["step"], tick=a["tick"], regions=metrics, ui_differences=fields))
    return dict(scenario=spec["id"], category="different" if first else "equal_observed_visual_ui",
                first=first, samples=rows, evidence=spec["evidence"])


def probe(args):
    output = private_output(args.output)
    require(not output.exists(), "use a fresh output directory")
    output.mkdir(parents=True)
    if not args.game.is_dir() or not args.runtime.is_file() or not args.clone.is_file():
        write_json(output / "report.json", dict(category="skipped", reason="private game/runtime or clone unavailable"))
        return 77
    results = {}
    for runner in ("reference", "clone"):
        directory = output / runner
        directory.mkdir()
        game = directory / "game"
        try:
            private_game(args.game, game)
            if runner == "reference":
                script = directory / "input.txt"
                script.write_text("wait 1500\nclick 140 239\nwait 300\nclick 160 543\nwait 500\n"
                                  "click 140 257\nclick 140 544\nwait 500\nclick 700 430\nwait 1500\nshot\nquit\n")
                execute([args.runtime.resolve(), game / "Aztec.exe"], directory, args.timeout,
                        dict(RT_NO_GRAB="1", RT_NO_VIDEO="1", RT_RESOLUTION="800x600",
                             RT_SCRIPT=str(script), RT_DUMP_DIR=str(directory)))
            else:
                env = {k: v for k, v in os.environ.items() if not k.startswith("RT_")}
                env.update(SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
                argv = ["timeout", "-s", "KILL", str(args.timeout), "unshare", "-rn",
                        str(args.clone.resolve()), "--game", str(game), "1", "--size", "800x600",
                        "--rules-dir", str(ROOT / "docs/data"), "--software", "--no-audio", "--no-hud", "--no-light", "--no-sim", "--cam", "8,8",
                        "--zoom", "1", "--ui-trace", str(directory / "ui.jsonl"),
                        "--screenshot", str(directory / "frame.png"), "--frames", "4"]
                write_json(directory / "invocation.json", dict(argv=argv))
                with (directory / "process.log").open("w") as log:
                    completed = subprocess.run(argv, env=env, cwd=directory, stdout=log,
                                               stderr=subprocess.STDOUT, check=False)
                require(completed.returncode == 0, f"clone exited {completed.returncode}")
            if runner == "reference":
                captures = sorted(directory.glob("shot_*.ppm"))
                require(bool(captures), "reference produced no frame capture")
                image_file(captures[-1])
            else:
                image_file(directory / "frame.png")
                require(bool((directory / "ui.jsonl").read_text().strip()), "clone produced no UI trace")
            results[runner] = "capture_probe_completed"
        except (ValueError, RuntimeError, OSError) as exc:
            results[runner] = dict(category="experiment_failed", reason=str(exc))
    # RT_SCRIPT wait/shot is wall-clock driven. It proves no tick/camera alignment.
    write_json(output / "report.json", dict(category="blocked_alignment_contract", runners=results,
               reason="reference fixed-tick camera/UI observer and L0 shared runner are absent; probe frames are unpaired"))
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="action", required=True)
    p = sub.add_parser("compare")
    p.add_argument("scenario", type=Path)
    p.add_argument("reference", type=Path)
    p.add_argument("clone", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p = sub.add_parser("probe")
    p.add_argument("--game", type=Path, required=True)
    p.add_argument("--runtime", type=Path, required=True)
    p.add_argument("--clone", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--timeout", type=int, default=30)
    args = parser.parse_args()
    try:
        if args.action == "probe":
            require(1 <= args.timeout <= 300, "timeout must be 1..300 seconds")
            return probe(args)
        output = private_output(args.output)
        require(not output.exists(), "use a fresh output file")
        output.parent.mkdir(parents=True, exist_ok=True)
        try:
            report = compare(args.scenario, args.reference, args.clone)
        except (ValueError, OSError, KeyError, TypeError, struct.error, zlib.error) as exc:
            write_json(output, dict(category="invalid_observation", reason=str(exc)))
            return 2
        write_json(output, report)
        return 0 if report["first"] is None else 1
    except (ValueError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
