"""Generate per-install table identifiers after applying the server patch."""
# Native Stylist by ICE & SCIEL
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import argparse
import hashlib
import json
from server_storage_config import storage_header

def configure(server, backup, inter_config='conf/inter_athena.conf'):
    server, backup = Path(server).resolve(), Path(backup).resolve()
    if not (server / 'rAthena.sln').is_file():
        raise ValueError('Expected the rAthena source root')
    target = server / 'src/custom/native_stylist_storage.hpp'
    if not target.is_file() or not (server / 'src/custom/native_stylist.inc').is_file():
        raise ValueError('Apply native_stylist.patch before configuring its storage header')
    data = storage_header(server, inter_config)
    before = target.read_bytes()
    if before == data:
        return dict(changed=False, configured=True)
    if backup.exists():
        raise ValueError('Choose a fresh backup directory')
    backup.mkdir(parents=True)
    saved = backup / 'native_stylist_storage.hpp'
    saved.write_bytes(before)
    if saved.read_bytes() != before:
        raise ValueError('Storage header backup verification failed')
    result = dict(path='src/custom/native_stylist_storage.hpp',
                  before_sha256=hashlib.sha256(before).hexdigest(),
                  after_sha256=hashlib.sha256(data).hexdigest())
    (backup / 'MANIFEST.json').write_text(json.dumps(result, indent=2) + '\n', encoding='ascii')
    try:
        target.write_bytes(data)
        if target.read_bytes() != data:
            raise ValueError('Storage header write verification failed')
    except BaseException:
        target.write_bytes(before)
        raise
    return dict(changed=True, configured=True, **result)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', required=True)
    parser.add_argument('--backup', required=True)
    parser.add_argument('--inter-config', default='conf/inter_athena.conf')
    args = parser.parse_args()
    print(json.dumps(configure(args.server, args.backup, args.inter_config), indent=2))
