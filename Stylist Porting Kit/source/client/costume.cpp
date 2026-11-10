#include "costume.hpp"
#include "costume_support.hpp"
#define EXPORT extern "C" __declspec(dllexport)
extern "C" long __cdecl _InterlockedCompareExchange(long volatile*,long,long);
extern "C" long __cdecl _InterlockedExchange(long volatile*,long);
extern "C" void* __cdecl _ReturnAddress();
#pragma intrinsic(_InterlockedCompareExchange,_InterlockedExchange)
#pragma intrinsic(_ReturnAddress)
namespace costume {
template<class T>T Native(u32 a){return reinterpret_cast<T>(a);}
inline u32& Field(void* p,unsigned n){return *reinterpret_cast<u32*>(static_cast<u8*>(p)+n);}
void* Allocate(unsigned n){return n?Native<void*(__cdecl*)(unsigned)>(0xdbbc4f)(n):nullptr;}
void Release(void* p,unsigned n){if(p)Native<void(__cdecl*)(void*,unsigned)>(0xdbbc7f)(p,n);}
using GetModule=void*(__stdcall*)(const char*);using GetProc=void*(__stdcall*)(void*,const char*);
using TlsAllocate=u32(__stdcall*)();using TlsGet=void*(__stdcall*)(u32);using TlsSet=int(__stdcall*)(u32,void*);
TlsGet tlsGet;TlsSet tlsSet;u32 tlsIndex=~0u;volatile long initState,storeLock;
void* sehHandler;
// The host profile has no SafeSEH table. Root initializes this standard CRT
// handler at the host startup bridge before entering any installed hook.
extern "C" __declspec(naked) void __cdecl _except_handler3(){__asm{cmp dword ptr[sehHandler],0}__asm{je unavailable}__asm{jmp dword ptr[sehHandler]}__asm{unavailable:mov eax,1}__asm{ret}}
bool Initialize(){
 if(initState==2)return true;if(initState==3)return false;
 if(_InterlockedCompareExchange(&initState,1,0)==0){
  void* kernel=(*reinterpret_cast<GetModule*>(0xfc1268))("kernel32.dll");auto get=*reinterpret_cast<GetProc*>(0xfc1390);
  auto module=*reinterpret_cast<GetModule*>(0xfc1268);void* crt=module("msvcrt.dll");if(!crt)crt=(*reinterpret_cast<GetModule*>(0xfc138c))("msvcrt.dll");if(crt)sehHandler=get(crt,"_except_handler3");
  auto alloc=reinterpret_cast<TlsAllocate>(get(kernel,"TlsAlloc"));tlsGet=reinterpret_cast<TlsGet>(get(kernel,"TlsGetValue"));tlsSet=reinterpret_cast<TlsSet>(get(kernel,"TlsSetValue"));
  if(alloc&&tlsGet&&tlsSet&&sehHandler)tlsIndex=alloc();_InterlockedExchange(&initState,tlsIndex!=~0u?2:3);
 }else while(initState==1){}
 return initState==2;
}
void Lock(){while(_InterlockedCompareExchange(&storeLock,1,0)){} }
void Unlock(){_InterlockedExchange(&storeLock,0);}
__declspec(naked) u32 __fastcall ReplaySprite(void*,void*,const char*){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 007282D5h}__asm{ret}}
__declspec(naked) void __cdecl ReplayConvert(void*,const void*,int){__asm{push ebp}__asm{mov ebp,esp}__asm{push ecx}__asm{mov eax,[ebp+10h]}__asm{push 00566777h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayHead(void*,void*,void*,u32,u32,u32,u32,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00B41D95h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayRobe(void*,void*,void*,u32,u32,u32,u32,u32,u32,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00B442F5h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayOpen(void*,void*,const char*,u32,u32,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00573755h}__asm{ret}}
__declspec(naked) u32 __cdecl ReplayExists(const char*,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00573275h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayRead(void*,void*,void*,unsigned){__asm{push ebp}__asm{mov ebp,esp}__asm{push ecx}__asm{push esi}__asm{push 00573985h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayMeta(void*,void*,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 007DAF35h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayOrder(void*,void*){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 007D6E25h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayInfo(void*,void*,u32){__asm{push ebp}__asm{mov ebp,esp}__asm{sub esp,0Ch}__asm{push 00D611F6h}__asm{ret}}
__declspec(naked) u32 __fastcall ReplayTop(void*,void*){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00D36D65h}__asm{ret}}
// Volatile indirection is required. MSVC must treat the native target as an
// ordinary ABI call instead of inferring register preservation from replay asm.
auto volatile OriginalSprite=&ReplaySprite;
auto volatile OriginalConvert=&ReplayConvert;
auto volatile OriginalHead=&ReplayHead;
auto volatile OriginalRobe=&ReplayRobe;
auto volatile OriginalOpen=&ReplayOpen;
auto volatile OriginalExists=&ReplayExists;
auto volatile OriginalRead=&ReplayRead;
auto volatile OriginalMeta=&ReplayMeta;
auto volatile OriginalOrder=&ReplayOrder;
auto volatile OriginalInfo=&ReplayInfo;
auto volatile OriginalTop=&ReplayTop;
struct Frame {u16 width,height;u32 crc;u8* groups;u32 offset;};
struct Render {Render* next;u32 serial;ns::Recipe recipe;char source[512],alias[512];u8 sourcePalette[1024],palette[1024];ns::GroupRecipe shading;u8 indexGroups[256];Frame* frames;u32 frameBytes;u16 indexed,truecolor,availableGroups;bool ownsFrames,local;u8 loadStatus,highlight;};
struct Context {Context* previous;Render* render;void* paletteObject;u8* pixels;u32 bytes,position;unsigned frame;bool paletteApplied,failed;};
ns::Recipe recipes[ns::RecipeCount-ns::CharacterCount];bool recipePresent[ns::RecipeCount-ns::CharacterCount];u8 recipeStatus[ns::RecipeCount-ns::CharacterCount];
u16 recipeGroups[ns::RecipeCount-ns::CharacterCount];
bool localPreview;
unsigned highlightView;u8 highlightGroup;
Render* renders;u32 nextSerial=1,renderCount,materialBytes;constexpr u32 MaxResource=64u*1024u*1024u,MaxPixels=64u*1024u*1024u,MaxRenders=4096,MaxMaterials=128u*1024u*1024u;
bool Snapshot(unsigned domain,unsigned view,ns::Recipe& out){if(!ns::CostumeID(view))return false;unsigned i=view-ns::FirstCostume;Lock();bool ok=recipePresent[i]&&recipes[i].domain==domain;if(ok)out=recipes[i];Unlock();return ok;}
void Destroy(Render* r){if(!r)return;if(r->ownsFrames){for(unsigned i=0;i<r->truecolor;++i)if(r->frames&&r->frames[i].groups)Release(r->frames[i].groups,unsigned(r->frames[i].width)*r->frames[i].height);Release(r->frames,r->frameBytes);}Release(r,sizeof(Render));}
u32 Divide(u64 n,u32 d){u32 result=0;if(d){__asm{mov eax,dword ptr[n]}__asm{mov edx,dword ptr[n+4]}__asm{div d}__asm{mov result,eax}}return result;}
struct Totals {u64 rgb[13][3];u32 count[13];};
u64 Product(u32 a,u32 b){u32 loWord,hiWord;__asm{mov eax,a}__asm{mul b}__asm{mov loWord,eax}__asm{mov hiWord,edx}return(u64(hiWord)<<32)|loWord;}
void Accumulate(Totals& t,const u8* rgb,unsigned group,unsigned count=1){if(group>=13||!count)return;t.count[group]+=count;for(unsigned c=0;c<3;++c)t.rgb[group][c]+=Product(rgb[c],count);}
bool PaletteRecords(Render* r,const u8* bytes,unsigned length,Totals& sums,u32* usage=nullptr){
 if(length<6||bytes[0]!='S'||bytes[1]!='P')return false;unsigned version=unsigned(bytes[3])*256+bytes[2];if(version!=0x100&&version!=0x101&&version!=0x200&&version!=0x201)return false;
 const unsigned header=version>=0x200?8:6;if(length<header)return false;
 r->indexed=Half(bytes+4);r->truecolor=version>=0x200?Half(bytes+6):0;unsigned at=header;
 u32 hits[256];Zero(hits,sizeof(hits));unsigned total=0;
 for(unsigned i=0;i<r->indexed;++i){if(length-at<4)return false;unsigned w=Half(bytes+at),h=Half(bytes+at+2);at+=4;unsigned pixels=w*h;if(!w||!h||w>4096||h>4096||pixels>MaxPixels-total)return false;total+=pixels;
  if(version==0x201){if(length-at<2)return false;unsigned end=at+2+Half(bytes+at);at+=2;if(end>length)return false;unsigned decoded=0;while(at<end){u8 value=bytes[at++];unsigned n=1;if(!value){if(at==end)return false;n=bytes[at++];if(!n)return false;}if(n>pixels-decoded)return false;decoded+=n;if(value)hits[value]+=n;}if(decoded!=pixels)return false;}
  else{if(pixels>length-at)return false;for(unsigned k=0;k<pixels;++k)if(bytes[at+k])++hits[bytes[at+k]];at+=pixels;}}
 if(r->truecolor){r->frameBytes=unsigned(r->truecolor)*sizeof(Frame);r->frames=static_cast<Frame*>(Allocate(r->frameBytes));if(!r->frames)return false;r->ownsFrames=true;Zero(r->frames,r->frameBytes);}
 for(unsigned i=0;i<r->truecolor;++i){if(length-at<4)return false;unsigned w=Half(bytes+at),h=Half(bytes+at+2);at+=4;unsigned pixels=w*h;if(!w||!h||w>4096||h>4096||pixels>MaxPixels-total||pixels>(length-at)/4)return false;total+=pixels;r->frames[i].width=static_cast<u16>(w);r->frames[i].height=static_cast<u16>(h);r->frames[i].crc=FrameCrc(bytes+at,w,h);r->frames[i].offset=at;
  for(unsigned k=0;k<pixels;++k){const u8* p=bytes+at+4*k;if(p[0]){u8 rgb[3]={p[3],p[2],p[1]};if(!KeyColor(rgb))Accumulate(sums,rgb,ns::ColorGroup(rgb));}}at+=pixels*4;}
 if(r->indexed){if(length-at!=1024)return false;Copy(r->sourcePalette,bytes+at,1024);for(unsigned i=0;i<256;++i)r->indexGroups[i]=i?static_cast<u8>(ns::ColorGroup(bytes+at+4*i)):255;
  for(unsigned i=1;i<256;++i)if(!KeyColor(bytes+at+4*i))Accumulate(sums,bytes+at+4*i,r->indexGroups[i],hits[i]);}
 // With no indexed frames, the native loader ignores trailing palette residue.
 if(usage)Copy(usage,hits,sizeof(hits));
 return true;
}
bool LoadCmm(Render* r,const u8* p,unsigned n,const u8* sourceHash){
 if(n<40||!r->truecolor||Word(p)!=0x314d4d43||!Equal(p+4,sourceHash,32)||Word(p+36)!=r->truecolor)return false;unsigned at=40,total=0;
 for(unsigned i=0;i<r->truecolor;++i){if(n-at<12)return false;Frame& f=r->frames[i];unsigned w=Half(p+at),h=Half(p+at+2),crc=Word(p+at+4),encoded=Word(p+at+8);at+=12;unsigned pixels=w*h;
  if(w!=f.width||h!=f.height||crc!=f.crc||!encoded||encoded%3||encoded>pixels*3||encoded>n-at||pixels>MaxPixels-total)return false;total+=pixels;
  f.groups=static_cast<u8*>(Allocate(pixels));if(!f.groups)return false;unsigned end=at+encoded,filled=0;while(at<end){unsigned count=Half(p+at);u8 group=p[at+2];at+=3;if(!count||count>pixels-filled||(group>12&&group!=255))return false;for(unsigned k=0;k<count;++k)f.groups[filled++]=group;}if(filled!=pixels)return false;
 }return at==n;
}
bool External(Render* r,const u8* source,unsigned sourceSize,bool& present){
 char path[]="native_stylist\\costumes\\00000000.ncm";Hex8(path+24,PathHash(r->source));void* memory=nullptr;unsigned bytes=0;present=client::ReadResourceAlloc(path,&memory,&bytes);if(!present)return true;const u8* p=static_cast<const u8*>(memory);bool ok=false;
 if(bytes>=348&&Word(p)==0x324d434e&&Word(p+4)==bytes&&Half(p+40)==r->indexed&&Half(p+42)==r->truecolor&&!(Half(p+44)&~3u)&&!Half(p+46)&&!p[87]&&Word(p+344)==bytes-348){
  u8 hash[32];Sha256(source,sourceSize,hash);if(Equal(hash,p+8,32)){ok=true;for(unsigned i=0;i<256;++i)if((p[88+i]>12&&p[88+i]!=255)||(!i&&p[88+i]!=255))ok=false;
   if(ok){Copy(r->indexGroups,p+88,256);Copy(r->shading.reference,p+48,39);if(Half(p+44)&2)ok=LoadCmm(r,p+348,bytes-348,hash);else if(bytes!=348)ok=false;}}
 }
 client::FreeResource(memory,bytes);return ok;
}
// Evaluate actual pixels after optional NCM/CMM masks have been installed.
// Protected keys, transparent pixels and immutable pure white are not controls.
bool Editable(const u8* rgb){return !KeyColor(rgb)&&(rgb[0]!=255||rgb[1]!=255||rgb[2]!=255);}
u16 SourceGroups(Render* r,const u8* source,const u32* hits){
 u16 result=0;for(unsigned i=1;i<256;++i)if(hits[i]&&r->indexGroups[i]<13&&Editable(r->sourcePalette+4*i))result|=static_cast<u16>(1u<<r->indexGroups[i]);
 for(unsigned i=0;i<r->truecolor;++i){const Frame& f=r->frames[i];for(unsigned y=0;y<f.height;++y)for(unsigned x=0;x<f.width;++x){const u8* p=source+f.offset+4*(y*f.width+x);if(!p[0])continue;u8 rgb[3]={p[3],p[2],p[1]};if(!Editable(rgb))continue;unsigned group=f.groups?f.groups[(f.height-1-y)*f.width+x]:ns::ColorGroup(rgb);if(group<13)result|=static_cast<u16>(1u<<group);}}
 return result;
}
Render* Build(const char* path,const ns::Recipe& recipe){
 auto r=static_cast<Render*>(Allocate(sizeof(Render)));if(!r)return nullptr;Zero(r,sizeof(Render));r->recipe=recipe;r->local=localPreview&&recipe.id==ns::CostumePreview;r->highlight=recipe.id==highlightView?highlightGroup:0;if(!Canonical(path,r->source)){Destroy(r);return nullptr;}
 Lock();Render* base=nullptr;for(Render* p=renders;p;p=p->next)if(Equal(p->source,r->source,Length(r->source)+1)){base=p;break;}Unlock();
 if(base){r->indexed=base->indexed;r->truecolor=base->truecolor;r->frames=base->frames;r->frameBytes=base->frameBytes;r->availableGroups=base->availableGroups;Copy(r->sourcePalette,base->sourcePalette,1024);Copy(r->indexGroups,base->indexGroups,256);Copy(r->shading.reference,base->shading.reference,39);}
 else{void* source=nullptr;unsigned bytes=0;if(!client::ReadResourceAlloc(r->source,&source,&bytes)){Destroy(r);return nullptr;}Totals sums{};u32 hits[256]{};bool ok=PaletteRecords(r,static_cast<const u8*>(source),bytes,sums,hits);
  if(ok){for(unsigned g=0;g<13;++g)for(unsigned c=0;c<3;++c)r->shading.reference[g][c]=sums.count[g]?static_cast<u8>(Divide(sums.rgb[g][c],sums.count[g])):128;bool external=false;ok=External(r,static_cast<const u8*>(source),bytes,external);if(ok)r->availableGroups=SourceGroups(r,static_cast<const u8*>(source),hits);}
  client::FreeResource(source,bytes);if(!ok){Destroy(r);return nullptr;}}
 r->shading.enabled=static_cast<u16>(recipe.flags);for(unsigned g=0;g<13;++g)for(unsigned c=0;c<3;++c)r->shading.target[g][c]=static_cast<u8>(recipe.rgb[g]>>(16-8*c));
 Copy(r->palette,r->sourcePalette,1024);for(unsigned i=1;i<256;++i){u8* p=r->palette+4*i;if(r->indexGroups[i]<13&&!KeyColor(p)){if(r->highlight==r->indexGroups[i]+1&&Editable(p)){p[0]=255;p[1]=64;p[2]=255;}else ns::Shade(r->shading,r->indexGroups[i],p);ProtectColor(p);}}
 return r;
}
bool SameRecipe(const ns::Recipe& a,const ns::Recipe& b){return Equal(&a,&b,sizeof(a));}
// A transport ID is a mutable binding. Immutable resource content and its
// validated load state remain reusable after that binding is released.
bool SameAppearance(const ns::Recipe& a,const ns::Recipe& b){return Equal(reinterpret_cast<const u8*>(&a)+2,reinterpret_cast<const u8*>(&b)+2,sizeof(a)-2);}
void Mark(const ns::Recipe& r,u8 state,u16 groups=0){if((r.id==highlightView&&highlightGroup)||(localPreview&&r.id==ns::CostumePreview))return;if(!ns::CostumeID(r.id))return;unsigned i=r.id-ns::FirstCostume;Lock();if(recipePresent[i]&&SameRecipe(recipes[i],r)&&(state==2||recipeStatus[i]!=2)){recipeStatus[i]=state;if(state==1)recipeGroups[i]|=groups;else recipeGroups[i]=0;}Unlock();}
u8 CachedStatus(const ns::Recipe& r){u8 status=0;for(Render* p=renders;p;p=p->next)if(!p->local&&!p->highlight&&SameAppearance(p->recipe,r)){if(p->loadStatus==2)return 2;if(p->loadStatus==1)status=1;}return status;}
u16 CachedGroups(const ns::Recipe& r){u16 groups=0;for(Render* p=renders;p;p=p->next)if(!p->local&&!p->highlight&&p->loadStatus==1&&SameAppearance(p->recipe,r))groups|=p->availableGroups;return groups;}
void Loaded(Render* r,u8 state){if(r->local||r->highlight)return;Lock();if(state==2||r->loadStatus!=2)r->loadStatus=state;for(unsigned i=0;i<ns::RecipeCount-ns::CharacterCount;++i)if(recipePresent[i]&&SameAppearance(recipes[i],r->recipe)&&(r->loadStatus==2||recipeStatus[i]!=2)){recipeStatus[i]=r->loadStatus;if(r->loadStatus==1)recipeGroups[i]|=r->availableGroups;else recipeGroups[i]=0;}Unlock();}
Render* Resolve(const char* path,const ns::Recipe& recipe){
 char canonical[512];if(!Canonical(path,canonical))return nullptr;Lock();for(Render* p=renders;p;p=p->next)if(p->local==(localPreview&&recipe.id==ns::CostumePreview)&&p->highlight==(recipe.id==highlightView?highlightGroup:0)&&SameAppearance(p->recipe,recipe)&&Equal(p->source,canonical,Length(canonical)+1)){u8 status=p->loadStatus;Unlock();if(status&&!p->local&&!p->highlight)Mark(recipe,status,p->availableGroups);return p;}bool full=renderCount>=MaxRenders;Unlock();if(full)return nullptr;
 Render* r=Build(canonical,recipe);if(!r)return nullptr;Lock();for(Render* p=renders;p;p=p->next)if(p->local==(localPreview&&recipe.id==ns::CostumePreview)&&p->highlight==(recipe.id==highlightView?highlightGroup:0)&&SameAppearance(p->recipe,recipe)&&Equal(p->source,canonical,Length(canonical)+1)){u8 status=p->loadStatus;Unlock();Destroy(r);if(status&&!p->local&&!p->highlight)Mark(recipe,status,p->availableGroups);return p;}
 unsigned storage=r->ownsFrames?r->frameBytes:0;if(r->ownsFrames)for(unsigned i=0;i<r->truecolor;++i)if(r->frames[i].groups)storage+=unsigned(r->frames[i].width)*r->frames[i].height;
 if(renderCount>=MaxRenders||nextSerial==0||storage>MaxMaterials-materialBytes){Unlock();Destroy(r);return nullptr;}r->serial=nextSerial++;unsigned n=Length(r->source),dir=0;for(unsigned i=0;i<n;++i)if(r->source[i]=='\\')dir=i+1;if(dir+16>512){Unlock();Destroy(r);return nullptr;}Copy(r->alias,r->source,dir);r->alias[dir]='~';for(unsigned i=0;i<10;++i)r->alias[dir+1+i]='0';u32 id=r->serial;for(unsigned i=0;i<7;++i){r->alias[dir+10-i]="0123456789abcdefghijklmnopqrstuv"[id&31];id>>=5;}Copy(r->alias+dir+11,".spr",5);r->next=renders;renders=r;++renderCount;materialBytes+=storage;Unlock();return r;
}
Render* Alias(const char* input,bool& act){act=false;unsigned n=Length(input);if(n<15||n>511)return nullptr;char path[512];Copy(path,input,n+1);for(unsigned i=0;i<n;++i){u8 c=static_cast<u8>(path[i]);if(c>=0x81){if(++i>=n)return nullptr;continue;}if(c=='/')path[i]='\\';else if(c>='A'&&c<='Z')path[i]=static_cast<char>(c+32);}if(Equal(path+n-4,".act",4)){act=true;Copy(path+n-4,".spr",4);}char canonical[512];if(!Canonical(path,canonical))return nullptr;Lock();Render* found=nullptr;for(Render* p=renders;p;p=p->next)if(Equal(p->alias,canonical,Length(p->alias)+1)){found=p;break;}Unlock();return found;}
const char* Source(Render* r,bool act,char* buffer){if(!act)return r->source;unsigned n=Length(r->source);Copy(buffer,r->source,n+1);Copy(buffer+n-4,".act",4);return buffer;}
void Rewrite(void* nativeString,Render* r,bool act){char path[512];const char* alias=r->alias;if(act){unsigned n=Length(alias);Copy(path,alias,n+1);Copy(path+n-4,".act",4);alias=path;}Native<void(__thiscall*)(void*,const char*,unsigned)>(0x4f1940)(nativeString,alias,Length(alias));}
void RewritePath(void* out,const ns::Recipe& recipe,unsigned type){
 // ACT placement/animation can have a different basename from its shared SPR.
 // Preserve the native ACT path. Only the independently selected SPR is dyed.
 if(type==0)return;
 if(type>1||Field(out,16)>511||Field(out,16)>Field(out,20)){Mark(recipe,2);return;}const char* p=Field(out,20)<16?static_cast<const char*>(out):reinterpret_cast<const char*>(Field(out,0));char source[512];unsigned n=Field(out,16);Copy(source,p,n);source[n]=0;Render* r=Resolve(source,recipe);if(r)Rewrite(out,r,type==0);else Mark(recipe,2);
}
Context* Current(){return initState==2?static_cast<Context*>(tlsGet(tlsIndex)):nullptr;}
bool Prepare(Context& c,void* file,unsigned width,unsigned height){
 if(c.pixels){Release(c.pixels,c.bytes);c.pixels=nullptr;}c.bytes=c.position=0;if(c.frame>=c.render->truecolor||!width||!height||width>4096||height>4096)return false;Frame& f=c.render->frames[c.frame++];if(width!=f.width||height!=f.height)return false;
 c.bytes=width*height*4;c.pixels=static_cast<u8*>(Allocate(c.bytes));if(!c.pixels)return false;if(!(OriginalRead(file,nullptr,c.pixels,c.bytes)&255))return false;if(FrameCrc(c.pixels,width,height)!=f.crc)return false;
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){u8* p=c.pixels+(y*width+x)*4;unsigned group=f.groups?f.groups[(height-1-y)*width+x]:255;if(!p[0]){if(group!=255)return false;continue;}u8 rgb[3]={p[3],p[2],p[1]};if(KeyColor(rgb))continue;if(!f.groups)group=ns::ColorGroup(rgb);if(group<13){if(c.render->highlight==group+1&&Editable(rgb)){rgb[0]=255;rgb[1]=64;rgb[2]=255;}else ns::Shade(c.render->shading,group,rgb);ProtectColor(rgb);p[3]=rgb[0];p[2]=rgb[1];p[1]=rgb[2];}}
 return true;
}
}
bool client::ReadResourceAlloc(const char* path,void** data,unsigned* bytes){
 if(!data||!bytes)return false;*data=nullptr;*bytes=0;unsigned file[16]{};file[0]=0xfd3074;file[1]=~0u;file[10]=15;bool ok=false;void* memory=nullptr;unsigned n=0;
 if(costume::OriginalOpen(file,nullptr,path,0,1,0)&255){n=file[4];if(file[2]&&n&&n<=costume::MaxResource){memory=costume::Allocate(n);if(memory)ok=(costume::OriginalRead(file,nullptr,memory,n)&255)!=0;}}
 costume::Native<void(__thiscall*)(void*)>(0x573060)(file);costume::Native<void(__thiscall*)(void*)>(0x4f08f0)(reinterpret_cast<ns::u8*>(file)+0x14);if(!ok){costume::Release(memory,n);return false;}*data=memory;*bytes=n;return true;
}
void client::FreeResource(void* data,unsigned bytes){costume::Release(data,bytes);}
bool client::CostumeInitialize(){return costume::Initialize();}
void client::CostumeRecipe(const ns::Recipe& r){if(costume::initState!=2||!ns::Valid(r)||r.domain==ns::Character||!ns::CostumeID(r.id))return;unsigned i=r.id-ns::FirstCostume;costume::Lock();if(!costume::recipePresent[i]||!costume::SameRecipe(costume::recipes[i],r)){costume::recipeStatus[i]=costume::CachedStatus(r);costume::recipeGroups[i]=costume::CachedGroups(r);}costume::recipes[i]=r;costume::recipePresent[i]=true;costume::Unlock();}
void client::CostumeResetSession(){CostumePreviewHighlight(0,-1);costume::Lock();costume::Zero(costume::recipePresent,sizeof(costume::recipePresent));costume::Zero(costume::recipeStatus,sizeof(costume::recipeStatus));costume::Zero(costume::recipeGroups,sizeof(costume::recipeGroups));costume::Unlock();}
bool client::CostumeRecipeReady(unsigned id){if(!ns::CostumeID(id))return false;costume::Lock();bool ok=costume::recipePresent[id-ns::FirstCostume];costume::Unlock();return ok;}
unsigned client::CostumeRenderStatus(unsigned id){if(!ns::CostumeID(id))return 0;costume::Lock();unsigned status=costume::recipePresent[id-ns::FirstCostume]?costume::recipeStatus[id-ns::FirstCostume]:0;costume::Unlock();return status;}
unsigned client::CostumeCapabilities(unsigned id){if(!ns::CostumeID(id))return 0;costume::Lock();unsigned i=id-ns::FirstCostume;unsigned groups=costume::recipePresent[i]&&costume::recipeStatus[i]==1?costume::recipeGroups[i]:0;costume::Unlock();return groups;}
unsigned client::CostumeSourceRGB(unsigned id,unsigned group){if(group>=13||!ns::CostumeID(id))return 0;ns::Recipe recipe{};costume::Lock();unsigned i=id-ns::FirstCostume;unsigned rgb=0;if(costume::recipePresent[i]){recipe=costume::recipes[i];for(auto p=costume::renders;p;p=p->next)if(!p->local&&!p->highlight&&costume::SameAppearance(p->recipe,recipe)&&(p->availableGroups&(1u<<group))){const auto color=p->shading.reference[group];rgb=unsigned(color[0])<<16|unsigned(color[1])<<8|color[2];break;}}costume::Unlock();return rgb;}
void client::LocalCostumePreview(bool active){costume::localPreview=active;}
void client::ReleaseLocalCostumes(){costume::Lock();costume::Render** link=&costume::renders;while(*link){auto p=*link;if(!p->local||p->ownsFrames){link=&p->next;continue;}*link=p->next;costume::Destroy(p);--costume::renderCount;}costume::Unlock();}
void client::CostumePreviewHighlight(unsigned id,int group){costume::highlightView=id==ns::CostumePreview&&group>=0&&group<13?id:0;costume::highlightGroup=costume::highlightView?static_cast<ns::u8>(group+1):0;}
bool client::CostumeRecipeFailed(unsigned id){return CostumeRenderStatus(id)==2;}
unsigned client::CostumeOriginalView(unsigned domain,unsigned view){if(!ns::CostumeID(view))return view;ns::Recipe r{};return costume::Snapshot(domain,view,r)?r.originalView:0;}
EXPORT ns::u32 __fastcall CostumeHeadPath(void* self,void*,void* out,ns::u32 type,ns::u32 a,ns::u32 b,ns::u32 c,ns::u32 view){ns::Recipe r{};bool found=costume::Snapshot(ns::HeadCostume,view,r);ns::u32 result=costume::OriginalHead(self,nullptr,out,type,a,b,c,found?r.originalView:(ns::CostumeID(view)?0:view));if(found)costume::RewritePath(out,r,type);return result;}
EXPORT ns::u32 __fastcall CostumeRobePath(void* self,void*,void* out,ns::u32 type,ns::u32 a,ns::u32 b,ns::u32 view,ns::u32 flag,ns::u32 c,ns::u32 d){ns::Recipe r{};bool found=costume::Snapshot(ns::RobeCostume,view,r);ns::u32 result=costume::OriginalRobe(self,nullptr,out,type,a,b,found?r.originalView:(ns::CostumeID(view)?0:view),flag,c,d);if(found)costume::RewritePath(out,r,type);return result;}
EXPORT ns::u32 __fastcall CostumeFileOpen(void* file,void*,const char* path,ns::u32 flags,ns::u32 mode,ns::u32 tail){bool act=false;costume::Render* r=costume::Alias(path,act);char original[512];return costume::OriginalOpen(file,nullptr,r?costume::Source(r,act,original):path,flags,mode,tail);}
EXPORT ns::u32 __cdecl CostumeExists(const char* path,ns::u32 mode){bool act=false;costume::Render* r=costume::Alias(path,act);char original[512];return costume::OriginalExists(r?costume::Source(r,act,original):path,mode);}
EXPORT ns::u32 __fastcall CostumeSpriteLoad(void* sprite,void*,const char* path){
 bool act=false;costume::Render* r=costume::Alias(path,act);if(!r||act||costume::initState!=2)return costume::OriginalSprite(sprite,nullptr,path);costume::Context c{};c.previous=costume::Current();c.render=r;c.paletteObject=static_cast<ns::u8*>(sprite)+0x110;ns::u32 result=0;if(!costume::tlsSet(costume::tlsIndex,&c)){costume::Loaded(r,2);return costume::OriginalSprite(sprite,nullptr,r->source);}
 __try{result=costume::OriginalSprite(sprite,nullptr,r->source);}__finally{costume::tlsSet(costume::tlsIndex,c.previous);costume::Release(c.pixels,c.bytes);}
 if((result&255)&&!c.failed&&(!r->indexed||c.paletteApplied)){costume::Loaded(r,1);return result;}costume::Loaded(r,2);
 // CSprRes::Unload clears frame elements and retains its vector storage. A
 // failed colored load must not leave an invisible or partially colored item.
 costume::Context* previous=costume::Current();costume::tlsSet(costume::tlsIndex,nullptr);
 __try{costume::Native<void(__fastcall*)(void*,void*)>(0x728ea0)(sprite,nullptr);result=costume::OriginalSprite(sprite,nullptr,r->source);}__finally{costume::tlsSet(costume::tlsIndex,previous);}return result;
}
EXPORT void __cdecl CostumePaletteConvert(void* destination,const void* source,int count){costume::Context* c=costume::Current();if(c&&c->render->indexed&&c->paletteObject==destination&&count==256){if(costume::Equal(source,c->render->sourcePalette,1024)){source=c->render->palette;c->paletteApplied=true;}else c->failed=true;}costume::OriginalConvert(destination,source,count);}
EXPORT ns::u32 __fastcall CostumeRead(void* file,void*,void* data,unsigned bytes){
 void* caller=_ReturnAddress();
 costume::Context* c=costume::Current();if(c&&caller==reinterpret_cast<void*>(0x728c94)&&bytes==4&&c->pixels){if(c->position>c->bytes||c->bytes-c->position<4){c->failed=true;return 0;}costume::Copy(data,c->pixels+c->position,4);c->position+=4;return 1;}
 ns::u32 ok=costume::OriginalRead(file,nullptr,data,bytes);if(c&&caller==reinterpret_cast<void*>(0x728bc6)&&bytes==4&&(ok&255)){if(!costume::Prepare(*c,file,costume::Half(data),costume::Half(static_cast<ns::u8*>(data)+2)))c->failed=true;}return ok;
}
EXPORT ns::u32 __fastcall CostumeRobeMetadata(void* self,void*,ns::u32 view){unsigned original=client::CostumeOriginalView(ns::RobeCostume,view);ns::u32 result=costume::OriginalMeta(self,nullptr,original);if(original!=view)costume::Field(self,0xbc)=view;return result;}
EXPORT ns::u32 __fastcall CostumeRobeOrder(void* self,void*){unsigned view=costume::Field(self,0xbc),original=client::CostumeOriginalView(ns::RobeCostume,view);if(view==original)return costume::OriginalOrder(self,nullptr);ns::u32 copy[49];costume::Copy(copy,self,sizeof(copy));copy[47]=original;return costume::OriginalOrder(copy,nullptr);}
EXPORT ns::u32 __fastcall CostumeRobeInfo(void* self,void*,ns::u32 view){return costume::OriginalInfo(self,nullptr,client::CostumeOriginalView(ns::RobeCostume,view));}
EXPORT ns::u32 __fastcall CostumeRobeDrawOnTop(void* self,void*){unsigned view=costume::Field(self,0x494),original=client::CostumeOriginalView(ns::RobeCostume,view);if(view==original)return costume::OriginalTop(self,nullptr);ns::u32 result=0;costume::Field(self,0x494)=original;__try{result=costume::OriginalTop(self,nullptr);}__finally{costume::Field(self,0x494)=view;}return result;}
