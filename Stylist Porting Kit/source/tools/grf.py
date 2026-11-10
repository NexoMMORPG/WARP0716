"""Read-only GRF 2/3 resource access for kit preparation. Standard unencrypted entries."""
from pathlib import Path
import struct,zlib
class GRF:
    def __init__(self,path,predicate=None):
        self.path=Path(path);self.f=self.path.open('rb');h=self.f.read(46)
        version=struct.unpack_from('<I',h,42)[0]
        if version not in (0x200,0x300):raise ValueError('GRF 2/3 required')
        self.v3=version==0x300;self.entries={}
        self.f.seek(46+struct.unpack_from('<Q' if self.v3 else '<I',h,30)[0])
        if self.v3 and self.f.read(4)!=bytes(4):raise ValueError('Encrypted table unsupported')
        packed,real=struct.unpack('<II',self.f.read(8));table=zlib.decompress(self.f.read(packed))
        if len(table)!=real:raise ValueError('Invalid GRF table')
        at=0
        while at<len(table):
            end=table.index(0,at);name=table[at:end].decode('cp949',errors='surrogateescape');at=end+1
            fields=struct.unpack_from('<IIIBQ' if self.v3 else '<IIIBI',table,at);at+=21 if self.v3 else 17
            if predicate is None or predicate(name.lower()):self.entries[name.lower()]=fields
    def read(self,name):
        packed,aligned,real,flags,offset=self.entries[name.lower()]
        if flags!=1:raise ValueError('Encrypted/directory entry unsupported')
        self.f.seek(offset+46);b=zlib.decompress(self.f.read(packed))
        if len(b)!=real:raise ValueError('Invalid resource length')
        return b
