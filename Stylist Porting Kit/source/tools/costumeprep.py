"""Author external Native Stylist v2 costume material data from an actual SPR.

SPDX-License-Identifier: GPL-3.0-or-later
The client can classify costumes without a metadata file. This tool freezes
resource-specific references and optionally installs explicitly authored masks.
It does not alter sprites, ACTs, archives, executables, or server state.
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import zlib


def truncdiv(a: int, b: int) -> int:
    return a // b if a >= 0 else -((-a) // b)


def color_group(rgb) -> int:
    red, green, blue = rgb
    high, low = max(rgb), min(rgb)
    span, light = high - low, high + low
    if span * 255 <= 20 * min(light, 510 - light):
        return 12
    if high == red:
        hue = truncdiv(60 * (green - blue), span)
    elif high == green:
        hue = 120 + truncdiv(60 * (blue - red), span)
    else:
        hue = 240 + truncdiv(60 * (red - green), span)
    return ((hue + 375) % 360) // 30


def key_color(rgb) -> bool:
    return max(rgb) < 16 or (rgb[0] >= 240 and rgb[1] < 16 and rgb[2] >= 240)


def canonical_path(text: str) -> bytes:
    raw = text.encode('cp949')
    result, at = bytearray(), 0
    while at < len(raw):
        c = raw[at]
        if c < 32 or c in (127, 58):
            raise ValueError('Control bytes and colon are forbidden in resource paths')
        if c >= 0x81:
            if at + 1 >= len(raw):
                raise ValueError('Incomplete CP949 sequence')
            result.extend(raw[at:at + 2])
            at += 2
            continue
        result.append(92 if c == 47 else c + 32 if 65 <= c <= 90 else c)
        at += 1
    if result.startswith(b'data\\'):
        del result[:5]
    if not result.startswith(b'sprite\\'):
        result[:0] = b'sprite\\'
    if not 12 <= len(result) <= 511 or not result.startswith(b'sprite\\') or not result.endswith(b'.spr'):
        raise ValueError('Use a sprite\\...spr game resource path')
    if any(part in (b'', b'.', b'..') for part in result.split(b'\\')):
        raise ValueError('Empty and relative path components are forbidden')
    return bytes(result)


def path_hash(raw: bytes) -> int:
    value = 2166136261
    for c in raw:
        value = ((value ^ c) * 16777619) & 0xffffffff
    return value


def parse_sprite(data: bytes) -> dict:
    if len(data) < 6 or data[:2] != b'SP':
        raise ValueError('Invalid SPR header')
    version = data[3] * 256 + data[2]
    if version not in (0x100, 0x101, 0x200, 0x201):
        raise ValueError('Supported SPR versions are 1.0, 1.1, 2.0 and 2.1')
    indexed = struct.unpack_from('<H', data, 4)[0]
    truecolor = struct.unpack_from('<H', data, 6)[0] if version >= 0x200 else 0
    at = 8 if version >= 0x200 else 6
    hits, frames, total = Counter(), [], 0

    def take(n):
        nonlocal at
        if n < 0 or at + n > len(data):
            raise ValueError('Truncated SPR resource')
        result = data[at:at + n]
        at += n
        return result

    def dimensions():
        nonlocal total
        width, height = struct.unpack('<HH', take(4))
        if not width or not height or max(width, height) > 4096:
            raise ValueError('SPR dimensions exceed native material bounds')
        total += width * height
        if total > 64 * 1024 * 1024:
            raise ValueError('SPR decoded pixel budget exceeded')
        return width, height

    for _ in range(indexed):
        width, height = dimensions()
        count = width * height
        if version == 0x201:
            encoded = take(struct.unpack('<H', take(2))[0])
            read, decoded = 0, 0
            while read < len(encoded):
                value = encoded[read]
                read += 1
                run = 1
                if not value:
                    if read == len(encoded) or not encoded[read]:
                        raise ValueError('Invalid zero run in SPR')
                    run = encoded[read]
                    read += 1
                decoded += run
                if decoded > count:
                    raise ValueError('SPR run exceeds frame')
                if value:
                    hits[value] += run
            if decoded != count:
                raise ValueError('SPR frame decoded length mismatch')
        else:
            hits.update(take(count))
    for _ in range(truecolor):
        width, height = dimensions()
        abgr = take(width * height * 4)
        rgba = bytearray()
        for y in range(height - 1, -1, -1):
            for x in range(width):
                alpha, blue, green, red = abgr[(y * width + x) * 4:(y * width + x + 1) * 4]
                rgba.extend((red, green, blue, alpha))
        frames.append({'width': width, 'height': height, 'rgba': bytes(rgba),
                       'crc32': zlib.crc32(rgba)})
    palette = take(1024) if indexed else bytes(1024)
    if not indexed:
        # The native truecolor loader ignores trailing palette residue.
        at = len(data)
    if at != len(data):
        raise ValueError('Unexpected data after SPR frames and palette')
    return {'version': version, 'indexed': indexed, 'truecolor': truecolor,
            'palette': palette, 'hits': hits, 'frames': frames}


def parse_cmm(blob: bytes, source_hash: bytes, sprite: dict) -> list[bytes]:
    if len(blob) < 40 or blob[:4] != b'CMM1' or blob[4:36] != source_hash:
        raise ValueError('CMM1 source identity does not match the complete SPR')
    if not sprite['truecolor'] or struct.unpack_from('<I', blob, 36)[0] != sprite['truecolor']:
        raise ValueError('CMM1 frame count must match the truecolor frames')
    at, masks = 40, []
    for frame in sprite['frames']:
        if at + 12 > len(blob):
            raise ValueError('Truncated CMM1 frame')
        width, height, crc, encoded = struct.unpack_from('<HHII', blob, at)
        at += 12
        count = width * height
        if (width, height, crc) != (frame['width'], frame['height'], frame['crc32']):
            raise ValueError('CMM1 dimensions or canonical RGBA CRC mismatch')
        if not encoded or encoded % 3 or encoded > count * 3 or encoded > len(blob) - at:
            raise ValueError('Invalid CMM1 encoded length')
        groups, end = bytearray(), at + encoded
        while at < end:
            run, group = struct.unpack_from('<HB', blob, at)
            at += 3
            if not run or (group > 12 and group != 255) or run > count - len(groups):
                raise ValueError('Invalid CMM1 material run')
            groups.extend(bytes([group]) * run)
        if len(groups) != count:
            raise ValueError('CMM1 mask does not fill the complete frame')
        if any(not alpha and group != 255 for alpha, group in zip(frame['rgba'][3::4], groups)):
            raise ValueError('CMM1 assigns material to transparent pixels')
        masks.append(bytes(groups))
    if at != len(blob):
        raise ValueError('Unexpected data after CMM1 masks')
    return masks


def prepare(data: bytes, resource: str, cmm: bytes = b'', overrides=None) -> tuple[bytes, dict]:
    sprite = parse_sprite(data)
    canonical = canonical_path(resource)
    source_hash = hashlib.sha256(data).digest()
    masks = parse_cmm(cmm, source_hash, sprite) if cmm else []
    palette = sprite['palette']
    index_groups = bytearray([255] + [color_group(palette[4 * i:4 * i + 3]) for i in range(1, 256)])
    if overrides is not None:
        index_groups = bytearray([255] * 256)
        used = set()
        for key, indices in overrides.items():
            group = int(key)
            if not 0 <= group < 13:
                raise ValueError('Index group keys must be 0 through 12')
            for index in indices:
                if type(index) is not int or not 1 <= index < 256 or index in used:
                    raise ValueError('Palette indices must be unique integers from 1 through 255')
                used.add(index)
                index_groups[index] = group
    sums, counts = [[0, 0, 0] for _ in range(13)], [0] * 13

    def add(rgb, group, count=1):
        if group < 13:
            counts[group] += count
            for c in range(3):
                sums[group][c] += rgb[c] * count

    for index, count in sprite['hits'].items():
        rgb = palette[4 * index:4 * index + 3]
        if index and not key_color(rgb):
            add(rgb, index_groups[index], count)
    for frame_index, frame in enumerate(sprite['frames']):
        for pixel, at in enumerate(range(0, len(frame['rgba']), 4)):
            rgb = frame['rgba'][at:at + 3]
            if frame['rgba'][at + 3] and not key_color(rgb):
                add(rgb, masks[frame_index][pixel] if masks else color_group(rgb))
    references = bytes(value // counts[g] if counts[g] else 128 for g in range(13) for value in sums[g])
    flags = (1 if overrides is not None else 0) | (2 if cmm else 0)
    header = struct.pack('<4sI32sHHHH', b'NCM2', 348 + len(cmm), source_hash,
                         sprite['indexed'], sprite['truecolor'], flags, 0)
    result = header + references + b'\0' + index_groups + struct.pack('<I', len(cmm)) + cmm
    assert len(result) == 348 + len(cmm)
    manifest = {'source_sha256': source_hash.hex(), 'canonical_resource_cp949_hex': canonical.hex(),
                'resource': canonical.decode('cp949'), 'material': f'{path_hash(canonical):08x}.ncm',
                'indexed_frames': sprite['indexed'], 'truecolor_frames': sprite['truecolor'],
                'explicit_mask': bool(cmm), 'group_pixel_counts': counts,
                'references': [list(references[3 * g:3 * g + 3]) for g in range(13)],
                'bytes': len(result), 'sha256': hashlib.sha256(result).hexdigest()}
    return result, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sprite', required=True, type=Path)
    parser.add_argument('--resource-path', required=True)
    parser.add_argument('--out', required=True, type=Path)
    parser.add_argument('--mask', type=Path)
    parser.add_argument('--index-groups', type=Path)
    args = parser.parse_args()
    result, manifest = prepare(args.sprite.read_bytes(), args.resource_path,
                               args.mask.read_bytes() if args.mask else b'',
                               json.loads(args.index_groups.read_text()) if args.index_groups else None)
    destination = args.out / 'data/native_stylist/costumes' / manifest['material']
    if destination.exists():
        raise SystemExit(f'Refusing to overwrite existing material: {destination}')
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(result)
    destination.with_suffix('.json').write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    print(json.dumps({'material': str(destination), **manifest}, ensure_ascii=False))


if __name__ == '__main__':
    main()
