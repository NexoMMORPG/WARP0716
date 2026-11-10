"""Prepare verified indexed body corrections and compatible legacy palettes.

SPDX-License-Identifier: GPL-3.0-or-later
Inputs are local operator-owned resources. No sprite or palette bytes are
embedded in this tool. The output is a new resource overlay for kit preparation.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from grf import GRF
from sprite import read


def sha(content: bytes) -> str:
    return hashlib.sha256(content).hexdigest()


def resource(root: Path, name: str) -> Path:
    parts = name.replace('\\', '/').split('/')
    if any(part in ('', '.', '..') or ':' in part for part in parts):
        raise ValueError('Invalid relative resource path: ' + name)
    result = root.joinpath(*parts).resolve()
    if not result.is_relative_to(root.resolve()):
        raise ValueError('Resource escapes its explicit input directory')
    return result


def compare(old: bytes, new: bytes) -> tuple[list, dict[int, set[int]]]:
    before, old_palette, old_truecolor = read(old)
    after, new_palette, new_truecolor = read(new)
    if old_truecolor or new_truecolor or not before or len(before) != len(after):
        raise ValueError('Expected equal nonempty indexed SPR frame counts')
    new_to_old: dict[int, set[int]] = {}
    for left, right in zip(before, after):
        if left[:2] != right[:2] or len(left[2]) != len(right[2]):
            raise ValueError('Frame dimensions changed')
        for old_index, new_index in zip(left[2], right[2]):
            if bool(old_index) != bool(new_index):
                raise ValueError('Indexed opacity changed')
            if old_index:
                if old_palette[4 * old_index:4 * old_index + 3] != new_palette[4 * new_index:4 * new_index + 3]:
                    raise ValueError('Original RGB changed')
                new_to_old.setdefault(new_index, set()).add(old_index)
    return (before, after), new_to_old


def relocate_palette(palette: bytes, frames: list, new_to_old: dict[int, set[int]]) -> bytes:
    if len(palette) != 1024:
        raise ValueError('Native legacy palette must contain exactly 1024 bytes')
    result = bytearray(palette)
    for new_index, old_indices in new_to_old.items():
        colors = {palette[4 * index:4 * index + 4] for index in old_indices}
        if len(colors) != 1:
            raise ValueError('Legacy palette has conflicting colors for a merged source index')
        result[4 * new_index:4 * new_index + 4] = next(iter(colors))
    for left, right in zip(*frames):
        for old_index, new_index in zip(left[2], right[2]):
            if old_index and palette[4 * old_index:4 * old_index + 4] != result[4 * new_index:4 * new_index + 4]:
                raise ValueError('Legacy palette render changed')
    return bytes(result)


def encode_indexed(frames: list, palette: bytes) -> bytes:
    if not frames or len(frames) > 65535 or len(palette) != 1024:
        raise ValueError('Indexed SPR requires bounded frames and a complete palette')
    result = bytearray(b'SP\x01\x02' + struct.pack('<HH', len(frames), 0))
    for width, height, pixels in frames:
        if not 0 < width <= 4096 or not 0 < height <= 4096 or len(pixels) != width * height:
            raise ValueError('Indexed frame dimensions or pixel count are invalid')
        encoded, at = bytearray(), 0
        while at < len(pixels):
            value = pixels[at]
            if value:
                encoded.append(value)
                at += 1
            else:
                end = at + 1
                while end < len(pixels) and pixels[end] == 0 and end - at < 255:
                    end += 1
                encoded.extend((0, end - at))
                at = end
        if len(encoded) > 65535:
            raise ValueError('SPR 2.1 frame exceeds its encoded length field')
        result.extend(struct.pack('<HHH', width, height, len(encoded)))
        result.extend(encoded)
    result.extend(palette)
    return bytes(result)


def isolate_frames(content: bytes, selected: list[int]) -> tuple[bytes, dict]:
    frames, palette, truecolor = read(content)
    if truecolor or not selected or len(set(selected)) != len(selected) or any(index < 0 or index >= len(frames) for index in selected):
        raise ValueError('Invalid indexed frame isolation request')
    occupied = {index for width, height, pixels in frames for index in pixels}
    isolated = sorted({index for frame in selected for index in frames[frame][2] if index})
    available = [index for index in range(8, 256) if index not in occupied]
    if len(isolated) > len(available):
        raise ValueError('Insufficient unused palette indices for exact frame isolation')
    mapping = dict(zip(isolated, available))
    new_palette = bytearray(palette)
    for old, new in mapping.items():
        new_palette[4 * new:4 * new + 4] = palette[4 * old:4 * old + 4]
    masks, changed = [], []
    for number, (width, height, pixels) in enumerate(frames):
        if number in selected:
            masks.append(dict(frame=number, width=width, height=height,
                              opacity_mask_sha256=sha(bytes(bool(index) for index in pixels)),
                              original_indices=sorted(set(pixels) - {0})))
            pixels = bytes(mapping.get(index, index) for index in pixels)
        changed.append((width, height, pixels))
    return encode_indexed(changed, bytes(new_palette)), dict(frames=masks, palette_remap=[[old, new] for old, new in sorted(mapping.items())],
                               reserved_indices=sorted(mapping.values()))


def isolate_pixels(content: bytes, pixel_masks: list[dict],
                   protect_frames: list[int] | None = None) -> tuple[bytes, dict]:
    protected = dict(palette_remap=[], reserved_indices=[])
    if protect_frames:
        content, protected = isolate_frames(content, protect_frames)
    frames, palette, truecolor = read(content)
    if truecolor or not pixel_masks:
        raise ValueError('Explicit indexed pixel masks are required')
    masks: dict[int, set[int]] = {}
    for item in pixel_masks:
        frame = item['frame']
        if type(frame) is not int or frame in masks or not 0 <= frame < len(frames) or frame in (protect_frames or []):
            raise ValueError('Invalid, duplicate or protected pixel-mask frame')
        pixels = frames[frame][2]
        points: set[int] = set()
        previous_end = 0
        for start, count in item['runs']:
            if type(start) is not int or type(count) is not int or start < previous_end or count <= 0 or start + count > len(pixels):
                raise ValueError('Pixel-mask runs must be sorted, disjoint and in bounds')
            if any(not pixels[index] for index in range(start, start + count)):
                raise ValueError('Pixel-mask run includes a transparent source pixel')
            points.update(range(start, start + count))
            previous_end = start + count
        if not points:
            raise ValueError('Empty pixel-mask frame')
        masks[frame] = points
    occupied = {index for width, height, pixels in frames for index in pixels}
    selected = sorted({frames[frame][2][point] for frame, points in masks.items() for point in points})
    available = [index for index in range(8, 256) if index not in occupied]
    if len(selected) > len(available):
        raise ValueError('Insufficient unused indices for exact pixel isolation')
    mapping = dict(zip(selected, available))
    changed_palette = bytearray(palette)
    for old, new in mapping.items():
        changed_palette[4 * new:4 * new + 4] = palette[4 * old:4 * old + 4]
    changed, proofs = [], []
    for number, (width, height, pixels) in enumerate(frames):
        points = masks.get(number)
        if points:
            copied = bytearray(pixels)
            for point in points:
                copied[point] = mapping[pixels[point]]
            proofs.append(dict(frame=number, width=width, height=height, pixels=len(points),
                               mask_sha256=sha(bytes(index in points for index in range(len(pixels))))))
            pixels = bytes(copied)
        changed.append((width, height, pixels))
    result = encode_indexed(changed, bytes(changed_palette))
    compare(content, result)
    return result, dict(palette_remap=protected['palette_remap'] + [[old, new] for old, new in sorted(mapping.items())],
                        skin_indices=sorted(mapping.values()), reserved_indices=protected['reserved_indices'],
                        pixel_masks=proofs, protected_frames=protect_frames or [])


def prepare(corrected: Path, archives: list[Path], output: Path,
            definitions: Path, loose: list[Path], write_palette_proof: bool = False) -> dict:
    corrected, output = corrected.resolve(), output.resolve()
    if output.exists():
        raise ValueError('Output must be a new directory: ' + str(output))
    authored = json.loads(definitions.read_text(encoding='utf-8'))
    if authored.get('schema') != 1:
        raise ValueError('Unsupported correction definitions schema')
    grfs = [GRF(path, lambda name: name.startswith('data\\palette\\') and name.endswith('.pal'))
            for path in archives]
    pending: dict[str, bytes] = {}
    records = []
    for definition in authored['corrections']:
        name = definition['source']
        source = resource(corrected, name)
        old = source.read_bytes()
        if sha(old) != definition['original_source_sha256']:
            raise ValueError('Strict source fingerprint mismatch: ' + name)
        old_act = source.with_suffix('.act').read_bytes()
        if sha(old_act) != definition['act_sha256']:
            raise ValueError('ACT identity mismatch: ' + name)
        isolation = None
        mode = definition.get('mode', 'equivalent_source')
        if mode not in ('equivalent_source', 'isolate_frames', 'isolate_pixels'):
            raise ValueError('Unknown correction mode: ' + mode)
        if mode in ('isolate_frames', 'isolate_pixels'):
            if mode == 'isolate_frames':
                new, isolation = isolate_frames(old, definition['isolated_frames'])
            else:
                new, isolation = isolate_pixels(old, definition['pixel_masks'], definition.get('protect_frames'))
            if sha(new) != definition['staged_sha256']:
                raise ValueError('Frame isolation output fingerprint mismatch: ' + name)
        else:
            replacement = resource(corrected, definition['replacement_source'])
            new = replacement.read_bytes()
            if sha(new) != definition['replacement_source_sha256']:
                raise ValueError('Strict replacement fingerprint mismatch: ' + name)
            if old_act != replacement.with_suffix('.act').read_bytes():
                raise ValueError('Replacement ACT differs: ' + name)
        frames, new_to_old = compare(old, new)
        pending[name] = new
        legacy = []
        for base in range(256):
            palette_name = 'data\\' + definition['palette_prefix'] + str(base) + definition['palette_suffix']
            palette = None
            provider = None
            for directory in [*loose, corrected]:
                candidate = resource(directory, palette_name)
                if candidate.is_file():
                    palette, provider = candidate.read_bytes(), str(candidate)
                    break
            if palette is None:
                for grf in grfs:
                    if palette_name.lower() in grf.entries:
                        palette, provider = grf.read(palette_name), str(grf.path)
                        break
            if palette is None:
                continue
            relocated = relocate_palette(palette, frames, new_to_old)
            if write_palette_proof:
                proof_name = 'proof-palettes\\' + name.removesuffix('.spr') + '\\' + str(base) + '.pal'
                pending[proof_name] = relocated
            legacy.append(dict(resource=palette_name, base=base, provider=provider,
                               source_sha256=sha(palette), staged_sha256=sha(relocated)))
        changed = {str(index): sorted(indices) for index, indices in sorted(new_to_old.items()) if indices != {index}}
        remap = []
        for new_index, old_indices in sorted(new_to_old.items()):
            if old_indices != {new_index}:
                if len(old_indices) != 1:
                    raise ValueError('Runtime palette remap requires one original index per new target')
                remap.append([next(iter(old_indices)), new_index])
        records.append(dict(source=name, original_source_sha256=sha(old),
                            replacement_source=definition.get('replacement_source'), staged_sha256=sha(new),
                            act_sha256=sha(old_act), indexed_frames=len(frames[0]),
                            all_frames_rgb_equal=True, all_dimensions_equal=True,
                            identical_act=True, opacity_preserved=True,
                            new_to_old_index_mapping=changed, palette_remap=remap,
                            reserved_indices=isolation['reserved_indices'] if isolation else [],
                            frame_isolation=isolation, legacy=legacy,
                            palette_binding_status=definition['palette_binding_status']))
    manifest = dict(schema=1, deployed=False, definitions=str(definitions.resolve()),
                    definitions_sha256=sha(definitions.read_bytes()), corrected_source=str(corrected),
                    loose_priority=[str(path.resolve()) for path in loose],
                    active_grf_priority=[str(path.resolve()) for path in archives],
                    native_bases_checked='0 through255', corrections=records,
                    files=[dict(resource=name, bytes=len(content), sha256=sha(content))
                           for name, content in sorted(pending.items())])
    # All guards and render equivalence checks finish before creating output.
    output.mkdir(parents=True)
    for name, content in pending.items():
        destination = resource(output, name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
    (output / 'BODY_CORRECTIONS.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corrected', type=Path, required=True)
    parser.add_argument('--grf', type=Path, action='append', required=True,
                        help='Active archives in native priority order, highest first')
    parser.add_argument('--loose', type=Path, action='append', default=[],
                        help='Explicit loose resource roots, highest priority first')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--definitions', type=Path,
                        default=Path(__file__).with_name('body_corrections_v2.json'))
    parser.add_argument('--write-palette-proof', action='store_true',
                        help='Save source-specific proof palettes outside the deployable data tree')
    arguments = parser.parse_args()
    result = prepare(arguments.corrected, arguments.grf, arguments.out,
                     arguments.definitions, arguments.loose, arguments.write_palette_proof)
    print(json.dumps(dict(output=str(arguments.out.resolve()),
                          sprites=len(result['corrections']),
                          legacy_palettes=sum(len(row['legacy']) for row in result['corrections']),
                          files=len(result['files']), deployed=False)))


if __name__ == '__main__':
    main()
