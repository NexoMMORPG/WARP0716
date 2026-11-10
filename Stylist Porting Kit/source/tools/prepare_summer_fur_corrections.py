"""Prepare reviewed Doram Summer fur and garment separations from local sources.

SPDX-License-Identifier: GPL-3.0-or-later
The frozen masks contain functional coordinates and fingerprints, not artwork.
"""
import argparse
import json
from pathlib import Path
from prepare_mount_skin_corrections import prepare


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--corrected', type=Path, required=True)
    parser.add_argument('--overlay', type=Path, action='append', default=[])
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--definitions', type=Path, default=Path(__file__).with_name('summer_fur_masks.json'))
    args = parser.parse_args()
    report = prepare(args.corrected, args.overlay, args.output, args.definitions,
                     receipt_name='SUMMER_FUR_CORRECTIONS.json')
    print(json.dumps(dict(corrected_sprites=len(report['corrections']), output=report['output'],
                          deployed=False, all_frames_rgba_equal=True,
                          every_native_palette_index_equivalent=True), indent=2))
