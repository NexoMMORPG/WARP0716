#pragma once
#include "protocol.hpp"
#include "shading.hpp"
// External resource-specific character materials. SPDX-License-Identifier: GPL-3.0-or-later
namespace ns {
constexpr u32 MaterialMagic=0x344d534e;
constexpr u8 NoGroup=0xff;
#pragma pack(push,1)
struct Material {u32 magic,hash;u8 rgba[1024],group[256],reference[GroupCount][3],anchor[GroupCount];};
struct BodyRemap {u32 magic;u16 count,reserved;u8 from[256],to[256];};
struct BodyMaterial {Material material;BodyRemap remap;};
#pragma pack(pop)
static_assert(sizeof(Material)==1424,"NSM4 character material ABI");
static_assert(sizeof(BodyRemap)==520&&sizeof(BodyMaterial)==1944,"NSB body material ABI");
inline bool ValidBodyRemap(const BodyRemap& remap){
 if(remap.magic!=0x3152534eu||remap.count>256||remap.reserved)return false;
 bool targets[256]={};for(unsigned i=0;i<remap.count;++i){if(!remap.from[i]||!remap.to[i]||targets[remap.to[i]])return false;targets[remap.to[i]]=true;}return true;
}
inline void RemapBody(u8* rgba,const BodyRemap& remap){
 if(!remap.count)return;u8 source[1024];for(unsigned i=0;i<1024;++i)source[i]=rgba[i];
 for(unsigned i=0;i<remap.count;++i)for(unsigned c=0;c<4;++c)rgba[remap.to[i]*4+c]=source[remap.from[i]*4+c];
}
inline u32 Hash(const char* s,unsigned n){u32 h=2166136261u;for(unsigned i=0;i<n;++i){u8 c=static_cast<u8>(s[i]);if(c>='A'&&c<='Z')c+=32;h=(h^c)*16777619u;}return h;}
// Resolve masks from the physical SPR even when legacy palettes are shared.
inline bool HeadMaterialHash(const char* path,unsigned n,u32* out){
 if(!path||!out||n<12||n>=252||path[n-4]!='.'||path[n-3]!='s'||path[n-2]!='p'||path[n-1]!='r')return false;
 const char* doram="\x73\x70\x72\x69\x74\x65\x5c\xb5\xb5\xb6\xf7\xc1\xb7\x5c";unsigned match=0;while(doram[match]&&match<n&&path[match]==doram[match])++match;
 const char* prefix=!doram[match]?"\x70\x61\x6c\x65\x74\x74\x65\x5c\xb5\xb5\xb6\xf7\xc1\xb7\x5c\xb8\xd3\xb8\xae\x5c\xb8\xd3\xb8\xae":"\x70\x61\x6c\x65\x74\x74\x65\x5c\xb8\xd3\xb8\xae\x5c\xb8\xd3\xb8\xae";
 unsigned first=0;for(unsigned i=0;i<n;++i)if(path[i]=='\\')first=i+1;
 if(first>=n-4)return false;
 u32 hash=2166136261u;
 for(unsigned i=0;prefix[i];++i){u8 c=static_cast<u8>(prefix[i]);if(c>='A'&&c<='Z')c+=32;hash=(hash^c)*16777619u;}
 for(unsigned i=first;i<n-4;++i){u8 c=static_cast<u8>(path[i]);if(c>='A'&&c<='Z')c+=32;hash=(hash^c)*16777619u;}
 *out=(hash^static_cast<u8>('_'))*16777619u;return true;
}
inline u64 GroupFlag(unsigned g){u64 value=0;reinterpret_cast<u32*>(&value)[g<32?0:1]=1u<<(g&31);return value;}
inline bool GroupEnabled(u64 flags,unsigned g){return g<32?(static_cast<u32>(flags)&(1u<<g))!=0:(static_cast<u32>(flags>>32)&(1u<<(g-32)))!=0;}
inline unsigned Light(const u8* p){return (p[0]*54u+p[1]*183u+p[2]*19u+128u)>>8;}
inline bool NativeColorKey(const u8* rgb){return rgb[0]>=254&&rgb[1]<4&&rgb[2]>=254;}
inline int ReferenceIndex(const Material& m,unsigned group){
 if(group>=GroupCount)return -1;const unsigned i=m.anchor[group];
 return i&&m.group[i]==group&&m.rgba[i*4]==m.reference[group][0]&&m.rgba[i*4+1]==m.reference[group][1]&&m.rgba[i*4+2]==m.reference[group][2]?static_cast<int>(i):-1;
}
inline bool ValidMaterial(const Material& m,u32 hash){
 if(m.magic!=MaterialMagic||m.hash!=hash||m.group[0]!=NoGroup)return false;
 u64 present=0;for(unsigned i=1;i<256;++i){if(m.group[i]!=NoGroup&&m.group[i]>=GroupCount)return false;if(m.group[i]<GroupCount)present|=GroupFlag(m.group[i]);}
 for(unsigned g=0;g<GroupCount;++g){if(GroupEnabled(present,g)){if(ReferenceIndex(m,g)<0)return false;}else if(m.anchor[g])return false;}
 return true;
}
inline u64 MaterialGroups(const Material& m){u64 result=0;for(unsigned i=1;i<256;++i)if(m.group[i]<GroupCount)result|=GroupFlag(m.group[i]);return result;}
inline bool Recolor(u8* out,const Material& m,const Recipe& r,const u8* source){
 for(unsigned i=0;i<1024;++i)out[i]=source[i];
 for(unsigned g=0;g<GroupCount;++g){
  if(!GroupEnabled(r.flags,g))continue;
  const int anchor=ReferenceIndex(m,g);if(anchor<0)continue;
  GroupRecipe shader;shader.enabled=1;
  for(unsigned c=0;c<3;++c){shader.reference[0][c]=source[static_cast<unsigned>(anchor)*4+c];shader.target[0][c]=static_cast<u8>(r.rgb[g]>>(16-8*c));}
  for(unsigned i=1;i<256;++i)if(m.group[i]==g&&!NativeColorKey(source+4*i)){
   Shade(shader,0,out+4*i);
   // The target converter treats near-magenta as a transparency sentinel.
   // Preserve source keys and keep newly dyed opaque entries above its G limit.
   if(NativeColorKey(out+4*i))out[4*i+1]=4;
  }
 }
 return true;
}
}
