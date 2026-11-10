#pragma once
#include "protocol.hpp"
#include "shading.hpp"
namespace costume {
using ns::u8;using ns::u16;using ns::u32;using ns::u64;
inline void Copy(void* d,const void* s,unsigned n){auto a=static_cast<u8*>(d);auto b=static_cast<const u8*>(s);while(n--)*a++=*b++;}
inline void Zero(void* d,unsigned n){auto a=static_cast<u8*>(d);while(n--)*a++=0;}
inline bool Equal(const void* a,const void* b,unsigned n){auto x=static_cast<const u8*>(a);auto y=static_cast<const u8*>(b);while(n--)if(*x++!=*y++)return false;return true;}
inline unsigned Length(const char* p,unsigned max=512){unsigned n=0;if(!p)return max+1;while(n<=max&&p[n])++n;return n;}
inline u32 Word(const void* p){auto b=static_cast<const u8*>(p);return b[0]|u32(b[1])<<8|u32(b[2])<<16|u32(b[3])<<24;}
inline u16 Half(const void* p){auto b=static_cast<const u8*>(p);return static_cast<u16>(b[0]|u16(b[1])<<8);}
inline u32 CrcByte(u32 crc,u8 b){crc^=b;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&(0u-(crc&1)));return crc;}
inline u32 FrameCrc(const u8* abgr,unsigned w,unsigned h){u32 c=~0u;for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const u8* p=abgr+((h-1-y)*w+x)*4;c=CrcByte(c,p[3]);c=CrcByte(c,p[2]);c=CrcByte(c,p[1]);c=CrcByte(c,p[0]);}return ~c;}
inline u32 Rotate(u32 a,unsigned n){return(a>>n)|(a<<(32-n));}
inline void ShaBlock(u32* h,const u8* p){
 static const u32 k[64]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 u32 w[64];for(unsigned i=0;i<16;++i)w[i]=u32(p[4*i])<<24|u32(p[4*i+1])<<16|u32(p[4*i+2])<<8|p[4*i+3];
 for(unsigned i=16;i<64;++i)w[i]=(Rotate(w[i-2],17)^Rotate(w[i-2],19)^(w[i-2]>>10))+w[i-7]+(Rotate(w[i-15],7)^Rotate(w[i-15],18)^(w[i-15]>>3))+w[i-16];
 u32 a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],v=h[7];
 for(unsigned i=0;i<64;++i){u32 t=v+(Rotate(e,6)^Rotate(e,11)^Rotate(e,25))+((e&f)^(~e&g))+k[i]+w[i];u32 z=(Rotate(a,2)^Rotate(a,13)^Rotate(a,22))+((a&b)^(a&c)^(b&c));v=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+z;}
 h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=v;
}
inline void Sha256(const void* data,unsigned n,u8* out){
 u32 h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};const u8* p=static_cast<const u8*>(data);unsigned remain=n;
 while(remain>=64){ShaBlock(h,p);p+=64;remain-=64;}u8 tail[128];Zero(tail,sizeof(tail));Copy(tail,p,remain);tail[remain]=0x80;unsigned total=remain<56?64:128;u64 bits=u64(n)*8;for(unsigned i=0;i<8;++i)tail[total-1-i]=static_cast<u8>(bits>>(8*i));ShaBlock(h,tail);if(total==128)ShaBlock(h,tail+64);
 for(unsigned i=0;i<8;++i)for(unsigned c=0;c<4;++c)out[4*i+c]=static_cast<u8>(h[i]>>(24-8*c));
}
inline bool Canonical(const char* input,char* out){
 unsigned n=Length(input);if(!n||n>511)return false;unsigned at=0;
 for(unsigned i=0;i<n;++i){u8 c=static_cast<u8>(input[i]);if(c<32||c==127||c==':')return false;if(c=='/')c='\\';if(c>='A'&&c<='Z')c+=32;
  if(c>=0x81){if(i+1>=n)return false;u8 trail=static_cast<u8>(input[i+1]);if(!((trail>='A'&&trail<='Z')||(trail>='a'&&trail<='z')||(trail>=0x81&&trail<=0xfe)))return false;out[at++]=static_cast<char>(c);out[at++]=input[++i];}else out[at++]=static_cast<char>(c);}
 out[at]=0;if(at>5&&Equal(out,"data\\",5)){for(unsigned i=0;i<=at-5;++i)out[i]=out[i+5];at-=5;}
 if(at<7||!Equal(out,"sprite\\",7)){if(at>504)return false;for(unsigned i=at+1;i>0;--i)out[i+6]=out[i-1];Copy(out,"sprite\\",7);at+=7;}
 if(at<12||out[at-4]!='.'||!Equal(out+at-3,"spr",3))return false;
 for(unsigned i=6;i<at;++i)if(out[i]=='\\'&&(out[i+1]=='\\'||(out[i+1]=='.'&&(out[i+2]=='\\'||(out[i+2]=='.'&&out[i+3]=='\\')))))return false;
 return true;
}
inline u32 PathHash(const char* p){u32 h=2166136261u;while(*p)h=(h^static_cast<u8>(*p++))*16777619u;return h;}
inline void Hex8(char* out,u32 n){const char* d="0123456789abcdef";for(unsigned i=0;i<8;++i)out[7-i]=d[(n>>(4*i))&15];}
inline bool KeyColor(const u8* p){return(ns::Maximum(p)<16)||(p[0]>=240&&p[1]<16&&p[2]>=240);}
inline void ProtectColor(u8* p){if(p[0]>=240&&p[1]<16&&p[2]>=240)p[1]=16;if(ns::Maximum(p)<16){unsigned c=p[1]>p[0]?1:0;if(p[2]>p[c])c=2;p[c]=16;}}
}
