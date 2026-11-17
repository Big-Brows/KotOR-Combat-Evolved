# Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
"""Repack HD glyphs into an owned K1 font's original UV rectangles and metrics.

Requires the existing Blender Python/numpy. Assets/reports remain in ignored out/.
Source: RaymanGT's KMRP Original HD atlas, SIL Open Font License 1.1.
https://github.com/RayesDiyab/KMRP-KOTOR-Modern-Restoration-Patch
No patcher/source code or enlarged-text metrics are installed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import subprocess
import re
import numpy as np

DEFAULT_NAME = "dialogfont10x10b"
REPO = Path(__file__).resolve().parents[2]


def digest(data):
    return hashlib.sha256(data).hexdigest()


def ignored(path):
    path = path.resolve()
    path.relative_to(REPO / "platforms" / "out")
    if subprocess.run(["git", "check-ignore", "-q", str(path)], cwd=REPO).returncode:
        raise ValueError("Generated destination must be ignored")
    return path


def native_font(game, font_name):
    archive = game / "TexturePacks" / "swpc_tex_gui.erf"
    with archive.open("rb") as stream:
        header = stream.read(160)
        assert header[:8] == b"ERF V1.0"
        count, keys, resources = struct.unpack_from("<I4xII", header, 16)
        stream.seek(keys)
        table = stream.read(count * 24)
        for index in range(count):
            name, resource, kind = struct.unpack_from("<16sIH", table, index * 24)
            if name.rstrip(b"\0").decode().lower() != font_name or kind != 3007:
                continue
            stream.seek(resources + resource * 8)
            offset, size = struct.unpack("<II", stream.read(8))
            stream.seek(offset)
            raw = stream.read(size)
            compressed, alpha, width, height, fmt, mips = struct.unpack_from("<IfHHBB", raw)
            # Native GUI fonts are single-mip RGBA or DXT5. Refuse other layouts.
            assert fmt == 4 and mips == 1 and compressed in (0, width * height)
            payload = compressed if compressed else width * height * 4
            return raw, raw[128 + payload:], alpha, width, height
    raise ValueError("Native font not found")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game", type=Path, required=True)
    parser.add_argument("--atlas", type=Path, required=True)
    parser.add_argument("--metrics", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--resource", default=DEFAULT_NAME)
    args = parser.parse_args()
    name = args.resource.lower()
    assert name.isascii() and name.replace('_', '').isalnum() and len(name) <= 16
    out = ignored(args.out)
    out.mkdir(parents=True, exist_ok=True)
    raw, metrics, alpha, native_w, native_h = native_font(args.game, name)
    image = args.atlas.read_bytes()
    mod_metrics = args.metrics.read_bytes()
    identifier, cmap, kind = struct.unpack_from("<BBB", image)
    width, height, bits, descriptor = struct.unpack_from("<HHBB", image, 12)
    assert cmap == 0 and kind == 2 and bits == 32 and descriptor == 8
    assert width >= native_w * 4 and height >= native_h * 4
    assert width <= 4096 and height <= 4096
    bgra = image[18 + identifier:18 + identifier + width * height * 4]
    assert len(bgra) == width * height * 4
    source = np.frombuffer(bgra, dtype=np.uint8).reshape(height, width, 4)
    # TGA is bottom-up BGRA. Repack by codepoint in the same orientation as TPC.
    # Only coverage is resampled; text RGB stays white for native GUI tinting.
    rgba = np.full((height, width, 4), 255, dtype=np.uint8)
    rgba[:, :, 3] = 0
    def rectangles(data):
        tokens = data.split()
        count = int(tokens[tokens.index(b'numchars') + 1])
        assert 127 <= count <= 256
        result = []
        for field in (b'upperleftcoords', b'lowerrightcoords'):
            start = tokens.index(field) + 2
            assert int(tokens[start - 1]) == count
            result.append(np.array([float(x) for x in tokens[start:start + count*3]]).reshape(count, 3))
        return result
    src_lo, src_hi = rectangles(mod_metrics)
    dst_lo, dst_hi = rectangles(metrics)
    assert len(src_lo) == len(dst_lo), 'Source/native character counts differ'
    native_metrics = metrics
    native_lo, native_hi = dst_lo.copy(), dst_hi.copy()
    relocated = bool(np.any(dst_hi[:, 0] > 1))
    if relocated:
        # Native 32-column atlases share a wrapped edge gutter. HD glyph art
        # cannot share those texels. Move each rectangle into its own 16x16 cell;
        # retain its UV span (therefore advance), texturewidth and line metrics.
        spans = dst_hi - dst_lo
        for c in range(len(dst_lo)):
            dst_lo[c, :2] = [(c % 16)/16 + 2/width, 1-(c // 16)/16 - 2/height]
        dst_hi = dst_lo + spans
        assert np.all(dst_hi[:, 0] < 1) and np.all(dst_hi[:, 1] >= 0)
        def replace_rectangles(match):
            data = dst_lo if match.group(1) == b'upperleftcoords' else dst_hi
            return match.group(1) + b' ' + str(len(data)).encode() + b'\r\n' + b''.join(
                ('%.9f %.9f %.9f\r\n' % tuple(v)).encode() for v in data)
        metrics = re.sub(rb'(upperleftcoords|lowerrightcoords)\s+\d+\s*\n(?:[^\n]*\n){%d}' % len(dst_lo),
                         replace_rectangles, metrics)
        dst_lo, dst_hi = rectangles(metrics)
    width_scale = float(native_metrics.split()[native_metrics.split().index(b'texturewidth') + 1])
    advances = lambda lo, hi: (hi[:, 0].astype(np.float32)-lo[:, 0].astype(np.float32))*np.float32(width_scale)*100
    advance_error = float(np.max(np.abs(advances(native_lo, native_hi)-advances(dst_lo, dst_hi))))
    assert advance_error < .0001, 'Glyph advances changed'
    coverage = []
    wrapped_glyphs = []
    for c in range(len(dst_lo)):
        sx0, sx1 = round(src_lo[c, 0] * width), round(src_hi[c, 0] * width)
        sy0, sy1 = round(src_hi[c, 1] * height), round(src_lo[c, 1] * height)
        dx0, dx1 = round(dst_lo[c, 0] * width), round(dst_hi[c, 0] * width)
        dy0, dy1 = round(dst_hi[c, 1] * height), round(dst_lo[c, 1] * height)
        assert 0 <= sx0 <= sx1 <= width and 0 <= sy0 <= sy1 <= height
        # Two native atlases include a one-native-pixel right gutter beyond U=1.
        # Their original clamp=0 sampler repeats. Preserve that wrap and UVs.
        assert 0 <= dx0 < width and dx0 <= dx1 <= width + width // native_w
        assert 0 <= dy0 <= dy1 <= height
        if sx1 == sx0 or sy1 == sy0 or dx1 == dx0 or dy1 == dy0:
            continue
        glyph = source[sy0:sy1, sx0:sx1, 3].astype(float)
        # Bilinear coverage resampling at pixel centers, with clamped edges.
        x = np.clip((np.arange(dx1-dx0)+.5)*glyph.shape[1]/(dx1-dx0)-.5, 0, glyph.shape[1]-1)
        y = np.clip((np.arange(dy1-dy0)+.5)*glyph.shape[0]/(dy1-dy0)-.5, 0, glyph.shape[0]-1)
        xi, yi = x.astype(int), y.astype(int)
        xj, yj = np.minimum(xi+1, glyph.shape[1]-1), np.minimum(yi+1, glyph.shape[0]-1)
        fx, fy = x-xi, (y-yi)[:, None]
        values = ((glyph[yi[:, None], xi]*(1-fx)+glyph[yi[:, None], xj]*fx)*(1-fy)
                  +(glyph[yj[:, None], xi]*(1-fx)+glyph[yj[:, None], xj]*fx)*fy)
        values = np.rint(values).astype(np.uint8)
        columns = np.arange(dx0, dx1) % width
        previous = rgba[dy0:dy1, columns, 3]
        assert not np.any((previous > 0) & (values > 0) & (np.abs(previous.astype(int)-values.astype(int)) > 1)), 'Conflicting wrapped glyph coverage'
        rgba[dy0:dy1, columns, 3] = np.maximum(previous, values)
        if dx1 > width:
            wrapped_glyphs.append(c)
        if c > 32 and c < 127:
            assert np.any(values > 0), f'Missing printable glyph {c}'
        coverage.append(c)
    header = struct.pack("<IfHHBB", 0, alpha, width, height, 4, 1).ljust(128, b"\0")
    result = header + rgba.tobytes() + metrics
    output = out / (name + ".tpc")
    output.write_bytes(result)
    (out / "native.tpc").write_bytes(raw)
    (out / "native.txi").write_bytes(native_metrics)
    report = dict(resource=name, source_url="https://github.com/RayesDiyab/KMRP-KOTOR-Modern-Restoration-Patch",
                  author="RaymanGT", license="SIL OFL 1.1", atlas_sha256=digest(image),
                  native_sha256=digest(raw), output_sha256=digest(result),
                  native_size=[native_w, native_h], output_size=[width, height],
                  uv_equal=not relocated, metrics_equal=not relocated, layout_equal=True,
                  max_advance_error_pixels=advance_error, glyphs=len(coverage), metrics_sha256=digest(metrics),
                  wrapped_glyphs=wrapped_glyphs,
                  installed=False)
    if args.profile:
        profile = ignored(args.profile)
        assert profile.is_dir() and not profile.is_symlink()
        override = profile / "override"
        assert override.is_dir() and not override.is_symlink()
        target = override / output.name
        # Do not silently change precedence of an existing TGA/TXI font mod.
        assert not any((override / (name + ext)).exists() for ext in (".tga", ".txi"))
        if target.exists() and digest(target.read_bytes()) == report["output_sha256"]:
            previous_report = out / "report.json"
            if previous_report.exists():
                report["previous_sha256"] = json.loads(previous_report.read_text()).get("previous_sha256")
        elif target.exists():
            backup = out / "before" / output.name
            backup.parent.mkdir(exist_ok=True)
            if not backup.exists():
                shutil.copy2(target, backup)
            report["previous_sha256"] = digest(target.read_bytes())
        else:
            report["previous_sha256"] = None
        shutil.copy2(output, target)
        assert digest(target.read_bytes()) == report["output_sha256"]
        report["installed"] = True
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
