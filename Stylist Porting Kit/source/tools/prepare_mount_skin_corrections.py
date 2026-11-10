"""Prepare fingerprint-guarded mount/skin index separations from reviewed masks.

SPDX-License-Identifier: GPL-3.0-or-later
No sprite or palette artwork is embedded in this tool or its functional masks.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

from sprite import read
from prepare_body_corrections import resource, sha, compare, encode_indexed


def find(roots: list[Path], name: str) -> Path:
    for root in roots:
        candidate = resource(root, name)
        if candidate.is_file():
            return candidate
    raise ValueError('Required local source is missing: ' + name)


def prepare(corrected: Path, overlays: list[Path], output: Path, definitions: Path,
            receipt_name: str = 'MOUNT_SKIN_CORRECTIONS.json') -> dict:
    roots = [path.resolve() for path in reversed(overlays)] + [corrected.resolve()]
    output = output.resolve()
    if output.exists():
        raise ValueError('Output must be a new directory: ' + str(output))
    authored = json.loads(definitions.read_text(encoding='utf-8'))
    if authored.get('schema') != 1:
        raise ValueError('Unsupported mount correction schema')
    if Path(receipt_name).name != receipt_name or not receipt_name.endswith('.json'):
        raise ValueError('Receipt must be one JSON file in the output directory')
    pending = {}
    records = []
    for definition in authored['corrections']:
        name = definition['source']
        if definition.get('review', {}).get('status') != 'reviewed':
            raise ValueError('Frozen pixel masks require completed visual review: ' + name)
        source = find(roots, name)
        content = source.read_bytes()
        if sha(content) != definition['original_source_sha256']:
            raise ValueError('Strict SPR source fingerprint mismatch: ' + name)
        act = find(roots, name.removesuffix('.spr') + '.act')
        if sha(act.read_bytes()) != definition['act_sha256']:
            raise ValueError('Strict ACT source fingerprint mismatch: ' + name)
        template = find(roots, definition['template_source'])
        if sha(template.read_bytes()) != definition['template_sha256']:
            raise ValueError('Strict template SPR fingerprint mismatch: ' + name)
        template_act = find(roots, definition['template_source'].removesuffix('.spr') + '.act')
        if sha(template_act.read_bytes()) != definition['template_act_sha256']:
            raise ValueError('Strict template ACT fingerprint mismatch: ' + name)
        frames, palette, truecolor = read(content)
        if truecolor or len(frames) != len(definition['frames']):
            raise ValueError('Expected the reviewed indexed frame set')
        mapping = dict(definition['palette_remap'])
        if len(mapping) != len(definition['palette_remap']) or not mapping:
            raise ValueError('Expected unique source relocation indices')
        targets = set(mapping.values())
        if len(targets) != len(mapping) or any(not 1 <= i <= 255 for i in set(mapping) | targets):
            raise ValueError('Relocation targets must be unique nonzero palette indices')
        occupied = {index for width, height, pixels in frames for index in pixels}
        if occupied & targets:
            raise ValueError('Relocation would overwrite an occupied source index')
        new_palette = bytearray(palette)
        for old, new in mapping.items():
            new_palette[new * 4:new * 4 + 4] = palette[old * 4:old * 4 + 4]
        result = []
        changed_pixels = 0
        affected_frames = 0
        for number, ((width, height, pixels), mask) in enumerate(zip(frames, definition['frames'])):
            if (mask['frame'], mask['width'], mask['height']) != (number, width, height):
                raise ValueError('Frame identity or dimensions differ from reviewed mask')
            new_pixels = bytearray(pixels)
            selected = set()
            for offset, old in mask['pixels']:
                if offset in selected or not 0 <= offset < len(pixels) or pixels[offset] != old or old not in mapping:
                    raise ValueError('Frozen pixel mask differs from its guarded source')
                if old not in definition.get('source_indices', definition.get('target_skin_indices', [])):
                    raise ValueError('A selected pixel is outside the reviewed shared skin group')
                selected.add(offset)
                new_pixels[offset] = mapping[old]
            changed_pixels += len(selected)
            affected_frames += bool(selected)
            result.append((width, height, bytes(new_pixels)))
        new = encode_indexed(result, bytes(new_palette))
        paired, new_to_old = compare(content, new)
        inverse = {new: old for old, new in mapping.items()}
        # This equality proves preservation for every possible native PAL,
        # provided its relocations are copied from the original PAL snapshot.
        for before, after in zip(*paired):
            if any(old != inverse.get(current, current) for old, current in zip(before[2], after[2])):
                raise ValueError('Legacy palette index equivalence failed')
            if any(palette[old * 4:old * 4 + 4] != new_palette[current * 4:current * 4 + 4]
                   for old, current in zip(before[2], after[2])):
                raise ValueError('Complete RGBA preservation failed')
        digest = hashlib.sha256()
        for width, height, pixels in result:
            digest.update(struct.pack('<II', width, height))
            digest.update(pixels)
        masks_hash = sha(json.dumps(definition['frames'], sort_keys=True, separators=(',', ':')).encode())
        pending[name] = new
        records.append(dict(source=name, source_file=str(source), original_source_sha256=sha(content),
                            act_sha256=definition['act_sha256'], staged_sha256=sha(new),
                            layout_sha256=digest.hexdigest(), palette_sha256=sha(bytes(new_palette)),
                            palette_remap=definition['palette_remap'], reserved_indices=sorted(targets),
                            indexed_frames=len(frames), affected_frames=affected_frames,
                            changed_pixels=changed_pixels, all_dimensions_equal=True,
                            all_frames_rgba_equal=True, opacity_preserved=True, identical_act=True,
                            every_native_palette_index_equivalent=True, pixel_masks_sha256=masks_hash,
                            review=definition['review']))
    # Validate every source, ACT, allocation and render invariant before writing.
    output.mkdir(parents=True)
    for name, content in pending.items():
        destination = resource(output, name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(content)
    for record in records:
        record['staged_file'] = str(resource(output, record['source']))
    manifest = dict(schema=1, deployed=False, output=str(output),
                    definitions=str(definitions.resolve()), definitions_sha256=sha(definitions.read_bytes()),
                    source_priority=[str(path) for path in roots], corrections=records,
                    legacy_palette_rule='Copy each oldIndex RGBA into newIndex from the original native PAL snapshot, per SPR source. No global PAL file overrides.')
    (output / receipt_name).write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding='utf-8')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corrected', type=Path, required=True)
    parser.add_argument('--overlay', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--definitions', type=Path, default=Path(__file__).with_name('mount_skin_masks.json'))
    args = parser.parse_args()
    report = prepare(args.corrected, args.overlay, args.output, args.definitions)
    print(json.dumps(dict(staged_sprites=len(report['corrections']), output=report['output'], deployed=False,
                          all_frames_rgba_equal=True, every_native_palette_index_equivalent=True), indent=2))
