"""Indexed SPR 2.x reader used only to prepare external material resources."""
import struct
def read(data):
    if data[:2]!=b'SP' or data[3]!=2 or data[2] not in (0,1):raise ValueError('SPR 2.0/2.1 required')
    count,truecolor=struct.unpack_from('<HH',data,4);at=8;frames=[]
    for _ in range(count):
        w,h=struct.unpack_from('<HH',data,at);at+=4
        if data[2]==1:
            size=struct.unpack_from('<H',data,at)[0];at+=2;end=at+size;p=bytearray()
            while at<end:
                v=data[at];at+=1
                if v:p.append(v)
                else:p.extend(bytes(data[at]));at+=1
        else:p=data[at:at+w*h];at+=w*h
        if len(p)<w*h and not any(p):p=bytes(w*h)
        if len(p)!=w*h:raise ValueError('Invalid sprite frame')
        frames.append((w,h,bytes(p)))
    return frames,data[-1024:],truecolor
