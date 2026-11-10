"""Pack matching sprites, palettes and Stylist resources into one GRF."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zlib
from grf import GRF

def build(data_root, archives, output, report):
    data_root, output, report = map(lambda p: Path(p).resolve(), (data_root, output, report))
    if output.exists():
        raise ValueError('Choose a fresh output GRF')
    metadata = json.loads((data_root / 'native_stylist/materials.json').read_text(encoding='utf-8'))
    files = {}
    opened = []
    # First argument has the highest archive priority. Loose prepared data wins.
    for path in reversed(archives):
        archive = GRF(path)
        opened.append(archive)
        for name in archive.entries:
            if not name.startswith(('data\\sprite\\', 'data\\palette\\')) or not name.endswith(('.spr', '.act', '.pal')):
                raise ValueError('Unexpected resource in visual overlay: ' + name)
            files[name] = (archive, name)
    allowed_materials = {}
    for row in metadata['families']:
        name = 'data\\' + row['file'].replace('/', '\\').lower()
        allowed_materials[name] = row['sha256']
        path = data_root / Path(row['file'].replace('\\', '/'))
        if hashlib.sha256(path.read_bytes()).hexdigest() != row['sha256']:
            raise ValueError('Prepared material changed: ' + name)
        files[name] = path
    for sub in ('sprite', 'palette', 'texture/native_stylist', 'native_stylist'):
        for path in (data_root / sub).rglob('*'):
            if not path.is_file():
                continue
            relative = path.relative_to(data_root).as_posix()
            name = 'data\\' + relative.replace('/', '\\').lower()
            keep = (sub == 'sprite' and path.suffix.lower() in ('.spr', '.act')) or \
                   (sub == 'palette' and path.suffix.lower() == '.pal') or \
                   (sub == 'texture/native_stylist' and path.suffix.lower() == '.bmp') or \
                   (sub == 'native_stylist' and (relative.startswith('native_stylist/palettes/') and path.suffix == '.pal' or
                                               relative.startswith('native_stylist/costumes/') and path.suffix == '.ncm' or
                                               path.name in ('hairs_m.bin', 'hairs_f.bin', 'hairs_dm.bin', 'hairs_df.bin')))
            if keep:
                files[name] = path
    for sex in ('m', 'f', 'dm', 'df'):
        if 'data\\native_stylist\\hairs_' + sex + '.bin' not in files:
            raise ValueError('Missing hairstyle catalogue: ' + sex)
    required_art = ['window.bmp', 'dyes.bmp', 'picker/panel.bmp', 'picker/hue.bmp', 'picker/point.bmp', 'picker/hue_point.bmp']
    required_art += ['picker/h%03d.bmp' % i for i in range(150)]
    required_art += ['chips/c%x%x%x.bmp' % (r, g, b) for r in range(16) for g in range(16) for b in range(16)]
    for name in required_art:
        if 'data\\texture\\native_stylist\\' + name.replace('/', '\\') not in files:
            raise ValueError('Missing UI artwork: ' + name)
    for name in files:
        if not name.startswith('data\\') or '..' in name.split('\\') or ':' in name:
            raise ValueError('Invalid public resource path')
    output.parent.mkdir(parents=True, exist_ok=True)
    shared, records, table = {}, [], bytearray()
    def read(value):
        return value[0].read(value[1]) if isinstance(value, tuple) else value.read_bytes()
    with output.open('xb') as stream:
        stream.write(bytes(46))
        for name in sorted(files):
            data = read(files[name])
            digest = hashlib.sha256(data).hexdigest()
            if digest not in shared:
                compressed = zlib.compress(data, 6)
                shared[digest] = (len(compressed), len(compressed), len(data), 1, stream.tell() - 46)
                stream.write(compressed)
            table.extend(name.encode('cp949', errors='surrogateescape') + b'\0' + struct.pack('<IIIBI', *shared[digest]))
            records.append(dict(path=name, bytes=len(data), sha256=digest))
        offset = stream.tell() - 46
        compressed = zlib.compress(table, 6)
        stream.write(struct.pack('<II', len(compressed), len(table)) + compressed)
        stream.seek(0)
        stream.write(b'Master of Magic\0' + bytes(range(1, 15)) + struct.pack('<IIII', offset, 0, len(files) + 7, 0x200))
    # Verify every decompressed entry against its winning source bytes.
    archive = GRF(output)
    assert set(archive.entries) == set(files)
    for record in records:
        if hashlib.sha256(archive.read(record['path'])).hexdigest() != record['sha256']:
            raise ValueError('GRF round trip mismatch: ' + record['path'])
    archive.f.close()
    for archive in opened:
        archive.f.close()
    from collections import Counter
    result = dict(format=1, authors='ICE & SCIEL', grf='native_stylist.grf', sha256=hashlib.sha256(output.read_bytes()).hexdigest(),
                  bytes=output.stat().st_size, entries=len(files), unique_payloads=len(shared),
                  types=dict(Counter(Path(r['path']).suffix for r in records)),
                  material_profiles=len(allowed_materials), all_entries_verified=True,
                  metadata_included=False, files=records)
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2, ensure_ascii=True) + '\n', encoding='ascii')
    print(json.dumps({k: v for k, v in result.items() if k != 'files'}, indent=2))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', required=True)
    parser.add_argument('--grf', action='append', required=True, help='Highest priority first')
    parser.add_argument('--out', required=True)
    parser.add_argument('--report', required=True)
    args = parser.parse_args()
    build(args.data, args.grf, args.out, args.report)
