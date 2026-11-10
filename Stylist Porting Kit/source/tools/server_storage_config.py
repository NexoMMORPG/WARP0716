"""Resolve only core SQL identity names, without emitting credential settings."""
from pathlib import Path
import ipaddress
import re


def storage_names(server: Path, config='conf/inter_athena.conf') -> dict:
    server=server.resolve()
    result={'char_server_db':'ragnarok','map_server_db':'ragnarok','char_db':'char','inventory_db':'inventory'}
    endpoints={'char_server_ip':'127.0.0.1','char_server_port':'3306',
               'map_server_ip':'127.0.0.1','map_server_port':'3306'}
    active=set()

    def load(path):
        path=Path(path)
        if not path.is_absolute():path=server/path
        path=path.resolve()
        if path in active:raise ValueError('Recursive core SQL configuration import')
        active.add(path)
        try:
            with path.open(encoding='utf-8') as source:
                for line in source:
                    # Match the core's untrimmed key and leading-value whitespace.
                    key,separator,_=line.partition(':')
                    key=key.lower()
                    if not separator or key not in (*result,*endpoints,'import'):continue
                    value=line[len(key)+1:].lstrip(' \t\v\f').rstrip('\r\n')
                    if not value:continue
                    if key=='import':load(value)
                    elif key in endpoints:endpoints[key]=value
                    else:result[key]=value
        finally:active.remove(path)

    load(config)
    for key,value in result.items():
        if not 1<=len(value)<=64 or not re.fullmatch(r'[A-Za-z0-9_$]+',value):
            raise ValueError('Unsupported core SQL identifier for '+key)

    def host(value):
        # Literal comparison only. Do not infer that different DNS names,
        # localhost and an IP, or different SQL hosts share database storage.
        if not re.fullmatch(r'[A-Za-z0-9_.:-]+',value):
            raise ValueError('Unsupported core SQL endpoint host')
        try:return ipaddress.ip_address(value).compressed.lower()
        except ValueError:return value.lower()

    def port(value):
        value=value.strip()
        if not re.fullmatch(r'\+?[0-9]+',value) or not 1<=int(value)<=65535:
            raise ValueError('Unsupported core SQL endpoint port')
        return int(value)

    char_endpoint=(host(endpoints['char_server_ip']),port(endpoints['char_server_port']))
    map_endpoint=(host(endpoints['map_server_ip']),port(endpoints['map_server_port']))
    if char_endpoint!=map_endpoint:
        raise ValueError('Native Stylist item identity verification requires a shared SQL endpoint: map and char must use the same literal host and numeric port')
    if result['char_server_db'] != result['map_server_db']:
        raise ValueError('Native Stylist appearance tables require char_server_db and map_server_db to name the same database')
    return result


def storage_header(server: Path, config='conf/inter_athena.conf') -> bytes:
    values=storage_names(server,config)
    database=values['char_server_db']
    return ('#pragma once\n'
            '// Generated from core SQL identity names only. No credentials.\n'
            '// SPDX-License-Identifier: GPL-3.0-or-later\n'
            'namespace ns_storage {\n'
            f'static const char CharacterTable[]="`{database}`.`{values["char_db"]}`";\n'
            f'static const char InventoryTable[]="`{database}`.`{values["inventory_db"]}`";\n'
            '}\n').encode('ascii')
