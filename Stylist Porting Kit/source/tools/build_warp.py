"""Build a selectable WARP Stylist patch from a clean native client reference."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import zlib
from client_profile_20250716 import GUARDS, EXTRA_HOOKS, HAIR_SITES

def build(client, out, work):
    import pefile
    kit=Path(__file__).resolve().parents[1]
    client,out,work=(Path(p).resolve() for p in (client,out,work))
    if out.exists() or work.exists():raise ValueError('Choose fresh output and work directories')
    raw=client.read_bytes();host=pefile.PE(data=raw)
    if host.FILE_HEADER.Machine!=0x14c or host.OPTIONAL_HEADER.ImageBase!=0x400000:
        raise ValueError('Expected the supported x86 client at image base 0x400000')
    config=getattr(host,'DIRECTORY_ENTRY_LOAD_CONFIG',None)
    if config and (config.struct.SEHandlerTable or config.struct.SEHandlerCount):
        raise ValueError('This native profile requires a host without a SafeSEH table')
    guards=dict(GUARDS)
    names={0xc147fe:'packet-type call',0xc14824:'packet-type call',0xc9e29c:'map packet dispatch',
           0x7ace6d:'preview scale call',0xdb669b:'window procedure operand',0x725b60:'palette loader'}
    for va,name,expected in EXTRA_HOOKS:guards[va]=expected;names[va]=name
    for va,expected in guards.items():
        if host.get_data(va-0x400000,len(bytes.fromhex(expected)))!=bytes.fromhex(expected):
            raise ValueError('Native reference mismatch at '+hex(va))
    imports={i.address:i.name.decode() for d in host.DIRECTORY_ENTRY_IMPORT for i in d.imports if i.name}
    for va,name in {0xfc1268:'GetModuleHandleA',0xfc138c:'LoadLibraryA',0xfc1390:'GetProcAddress'}.items():
        if imports.get(va)!=name:raise ValueError('Native import mismatch: '+name)
    catalogues={name:[int(n) for n in data.split(',') if n.strip()] for name,data in re.findall(
        r'\b(m|f|dm|df)\[\]\s*=\s*\{([^}]+)\}',(kit/'server/native_stylist_hairs.hpp').read_text())}
    if set(catalogues)!={'m','f','dm','df'}:raise ValueError('Incomplete server hairstyle catalogues')
    human=max(max(catalogues['m']),max(catalogues['f']))
    doram=max(max(catalogues['dm']),max(catalogues['df']))
    if not 1<=human<=65535 or not 1<=doram<=65535:raise ValueError('Invalid catalogue limits')
    work.mkdir(parents=True)
    previous=0xc14803+struct.unpack('<i',host.get_data(0xc147ff-0x400000,4))[0]
    (work/'addresses.hpp').write_text('#define PREVIOUS_TYPE '+hex(previous)+'\n#define PREVIOUS_WNDPROC 0xDB8100\n',encoding='ascii')
    dll=work/'native_stylist.dll'
    cpp=[kit/'client'/name for name in ('window.cpp','palette.cpp','palette_hooks.cpp','costume.cpp','hooks.cpp')]
    subprocess.run([sys.executable,str(kit/'tools/compile_x86.py'),'--out',str(dll),'--base','0x2000000','--relocatable','--include',str(work),*map(str,cpp)],check=True)
    payload=pefile.PE(str(dll))
    if payload.FILE_HEADER.Machine!=0x14c or payload.OPTIONAL_HEADER.AddressOfEntryPoint:
        raise ValueError('Expected a freestanding x86 module without an entry point')
    for d in payload.OPTIONAL_HEADER.DATA_DIRECTORY:
        if d.Size and d.name not in ('IMAGE_DIRECTORY_ENTRY_EXPORT','IMAGE_DIRECTORY_ENTRY_DEBUG','IMAGE_DIRECTORY_ENTRY_BASERELOC'):
            raise ValueError('Unexpected payload directory: '+d.name)
    exports={re.sub(r'@\d+$','',e.name.decode().lstrip('_@')):e.address for e in payload.DIRECTORY_ENTRY_EXPORT.symbols}
    if set(exports)!={'PacketType','Dispatch','WndProc','PaletteLoad','StylistScale','NativeHairLimits'}|{name for _,name,_ in EXTRA_HOOKS}:
        raise ValueError('Unexpected payload exports')
    relocations=[]
    for block in payload.DIRECTORY_ENTRY_BASERELOC:
        for entry in block.entries:
            if entry.type==0:continue
            if entry.type!=3:raise ValueError('Unexpected relocation type')
            relocations.append(entry.rva)
    if not relocations or len(relocations)!=len(set(relocations)):raise ValueError('Invalid relocation table')
    sites=[(0xc147fe,'PacketType',0xe8,5),(0xc14824,'PacketType',0xe8,5),(0xc9e29c,'Dispatch',0xe9,7),
           (0x7ace6d,'StylistScale',0xe8,5),(0xdb669b,'WndProc',0,4),(0x725b60,'PaletteLoad',0xe9,5)]
    sites += [(va,name,0xe9,len(bytes.fromhex(expected))) for va,name,expected in EXTRA_HOOKS]
    profile=dict(format=2,linkedBase=payload.OPTIONAL_HEADER.ImageBase,imageSize=payload.OPTIONAL_HEADER.SizeOfImage,
                 headers=payload.OPTIONAL_HEADER.SizeOfHeaders,fileSize=dll.stat().st_size,crc32=zlib.crc32(dll.read_bytes()),
                 sections=[dict(raw=s.PointerToRawData,size=s.SizeOfRawData,rva=s.VirtualAddress) for s in payload.sections],
                 relocations=relocations,exports=exports,
                 hooks=[dict(va=va,symbol=name,opcode=op,size=size) for va,name,op,size in sites],
                 hairs=[dict(va=va,doram=d,catalogueMax=doram if d else human) for va,d in HAIR_SITES],
                 guards=[dict(va=va,hex=expected,name=names.get(va,'preview palette layout')) for va,expected in guards.items()])
    template=(kit/'tools/native_stylist.qjs.in').read_text(encoding='utf-8')
    helper=(kit/'tools/native_stylist_hair.qjs.in').read_text(encoding='utf-8')
    script=template.replace('__PROFILE__',json.dumps(profile,separators=(',',':'))).replace('__HAIR_HELPER__',helper)
    (out/'Scripts/Patches').mkdir(parents=True)
    (out/'Inputs/NativeStylist').mkdir(parents=True)
    (out/'Patches').mkdir()
    (out/'Scripts/Patches/NativeStylist.qjs').write_bytes(script.encode('utf-8'))
    (out/'Inputs/NativeStylist/native_stylist.dll').write_bytes(dll.read_bytes())
    (out/'Patches/NativeStylist.yml').write_text('''NativeStylistCommunity:
    title: NATIVE STYLIST
    mutex: false
    patches:
        - NativeStylist:
            title: Native Stylist by ICE & SCIEL
            author: ICE & SCIEL
            recommend: no
            desc: Native RGB Stylist for Ragexe 2025-07-16. Select alongside compatible patches on the clean client. Requires the matching GRF and server integration.
''',encoding='ascii')
    receipt=dict(reference_client_sha256=hashlib.sha256(raw).hexdigest(),payload_sha256=hashlib.sha256(dll.read_bytes()).hexdigest(),
                 payload_bytes=dll.stat().st_size,mapped_bytes=profile['imageSize'],relocations=len(relocations),
                 hooks=len(sites),hair_sites=len(HAIR_SITES),protocol=2,authors='ICE & SCIEL',
                 source_sha256={p.relative_to(kit).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((kit/'client').iterdir()) if p.suffix in ('.cpp','.hpp')})
    (work/'WARP_BUILD.json').write_bytes((json.dumps(receipt,indent=2)+'\n').encode('ascii'))
    print(json.dumps(receipt,indent=2))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--client',required=True)
    parser.add_argument('--out',required=True)
    parser.add_argument('--work',required=True)
    args=parser.parse_args()
    build(args.client,args.out,args.work)
