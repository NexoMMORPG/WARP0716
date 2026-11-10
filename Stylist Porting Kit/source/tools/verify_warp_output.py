"""Verify a Stylist module and its hooks in a combined WARP output."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import zlib

def verify(output,warp):
    import pefile
    output,warp=Path(output).resolve(),Path(warp).resolve()
    script=(warp/'Scripts/Patches/NativeStylist.qjs').read_text(encoding='utf-8')
    profile=json.loads(re.search(r'^const NativeStylistProfile = (.+)$',script,re.M)[1])
    if profile['format']!=2:raise ValueError('Unsupported Stylist patch profile format')
    final=pefile.PE(str(output))
    if final.FILE_HEADER.Machine!=0x14c or final.OPTIONAL_HEADER.ImageBase!=0x400000:
        raise ValueError('Expected x86 image at 0x400000')
    def section_for(va,size):
        rva=va-0x400000
        if rva<0 or rva+size>final.OPTIONAL_HEADER.SizeOfImage:return None
        return next((s for s in final.sections if s.VirtualAddress<=rva and
                     rva+size<=s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData)),None)
    call=final.get_data(0xc147fe-0x400000,5)
    if len(call)!=5 or call[0]!=0xe8:raise ValueError('Missing Stylist packet hook')
    payload_va=0xc14803+struct.unpack_from('<i',call,1)[0]-profile['exports']['PacketType']
    if payload_va<0x400000 or payload_va-0x400000+profile['imageSize']>final.OPTIONAL_HEADER.SizeOfImage:
        raise ValueError('Invalid Stylist allocation')
    for hook in profile['hooks']:
        target=payload_va+profile['exports'][hook['symbol']]
        expected=(bytes([hook['opcode']])+struct.pack('<i',target-hook['va']-5)+b'\x90'*(hook['size']-5)) if hook['opcode'] else struct.pack('<I',target)
        if final.get_data(hook['va']-0x400000,hook['size'])!=expected:
            raise ValueError('Hook mismatch: '+hook['symbol']+' at '+hex(hook['va']))
    hook_sites={hook['va'] for hook in profile['hooks']}
    for guard in profile['guards']:
        if guard['va'] not in hook_sites and final.get_data(guard['va']-0x400000,len(bytes.fromhex(guard['hex'])))!=bytes.fromhex(guard['hex']):
            raise ValueError('Native layout mismatch at '+hex(guard['va']))
    limits=[65535,65535]
    for site in profile['hairs']:
        branch=final.get_data(site['va']-0x400000,5)
        if len(branch)!=5 or branch[0]!=0xe9:raise ValueError('Missing hairstyle adapter')
        va=site['va']+5+struct.unpack_from('<i',branch,1)[0]
        data=final.get_data(va-0x400000,38)
        adapter_section=section_for(va,38)
        if len(data)!=38 or data[:2]!=b'\x81\xf9' or data[6:22]!=bytes.fromhex('7e0333c94183f92b7d0a8b14888bcae9') or data[26]!=0xe8 or data[31:34]!=bytes.fromhex('8bcae9'):
            raise ValueError('Invalid hairstyle adapter')
        if adapter_section is None or not adapter_section.Characteristics&0x20000000:
            raise ValueError('Hairstyle adapter is not executable')
        cap=struct.unpack_from('<I',data,2)[0]
        if not 1<=cap<=65535:raise ValueError('Invalid hairstyle limit')
        domain=1 if site['doram'] else 0
        limits[domain]=min(limits[domain],cap)
        if va+26+struct.unpack_from('<i',data,22)[0]!=site['va']+5 or va+38+struct.unpack_from('<i',data,34)[0]!=site['va']+5:
            raise ValueError('Invalid hairstyle adapter return')
        helper_va=va+31+struct.unpack_from('<i',data,27)[0]
        helper_tail=bytes.fromhex('c607008bc1b90a00000033d2f7f180c2304f881785c075f2897c241461c3')
        helper_section=section_for(helper_va,6+len(helper_tail))
        if helper_section is None or not helper_section.Characteristics&0x20000000:
            raise ValueError('Invalid hairstyle number formatter')
        helper=final.get_data(helper_va-0x400000,6+len(helper_tail))
        if helper[:2]!=b'\x60\xbf' or helper[6:]!=helper_tail:
            raise ValueError('Invalid hairstyle number formatter')
        buffer_end=struct.unpack_from('<I',helper,2)[0]
        buffer_section=section_for(buffer_end-15,16)
        if buffer_section is None or not buffer_section.Characteristics&0x80000000:
            raise ValueError('Invalid hairstyle number buffer')
    dll=warp/'Inputs/NativeStylist/native_stylist.dll'
    binary=dll.read_bytes()
    if len(binary)!=profile['fileSize'] or zlib.crc32(binary)!=profile['crc32']:raise ValueError('Payload/QJS mismatch')
    payload=pefile.PE(data=binary)
    payload.relocate_image(payload_va)
    relocated=bytearray(payload.get_memory_mapped_image())
    struct.pack_into('<I',relocated,payload.OPTIONAL_HEADER.get_field_absolute_offset('ImageBase'),profile['linkedBase'])
    struct.pack_into('<II',relocated,profile['exports']['NativeHairLimits'],*limits)
    actual=final.get_memory_mapped_image()
    if actual[payload_va-0x400000:payload_va-0x400000+len(relocated)]!=relocated:
        raise ValueError('Embedded module differs from independent PE relocation')
    section=section_for(payload_va,profile['imageSize'])
    if section is None or section.Characteristics&0xe0000000!=0xe0000000:raise ValueError('Invalid module section permissions')
    if final.OPTIONAL_HEADER.DllCharacteristics&0x40:raise ValueError('Host ASLR is incompatible with this native profile')
    config=getattr(final,'DIRECTORY_ENTRY_LOAD_CONFIG',None)
    if config and (config.struct.SEHandlerTable or config.struct.SEHandlerCount):raise ValueError('Unsupported host SafeSEH table')
    if final.OPTIONAL_HEADER.CheckSum and not final.verify_checksum():raise ValueError('Invalid nonzero PE checksum')
    result=dict(passed=True,output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),bytes=output.stat().st_size,
                payload_va=hex(payload_va),hooks=len(profile['hooks']),hair_sites=len(profile['hairs']),
                relocations=len(profile['relocations']),hair_limits=dict(human=limits[0],doram=limits[1]),payload_matches_independent_relocation=True,
                pe_checksum='valid' if final.OPTIONAL_HEADER.CheckSum else 'omitted')
    print(json.dumps(result,indent=2))
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True)
    parser.add_argument('--warp',required=True)
    args=parser.parse_args()
    verify(args.output,args.warp)
