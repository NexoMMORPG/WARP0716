#include "native.hpp"
#include "addresses.hpp"
#include "palette_hooks.hpp"
#include "costume.hpp"
extern "C" void* __cdecl memset(void*,int,unsigned);
extern "C" void* __cdecl memcpy(void*,const void*,unsigned);
#pragma function(memset,memcpy)
extern "C" void* __cdecl memset(void* p,int v,unsigned n){auto d=static_cast<volatile u8*>(p);while(n--)*d++=static_cast<u8>(v);return p;}
extern "C" void* __cdecl memcpy(void* p,const void* q,unsigned n){auto d=static_cast<volatile u8*>(p);auto s=static_cast<const volatile u8*>(q);while(n--)*d++=*s++;return p;}
extern "C" {int _fltused=0;}
EXPORT void* __fastcall PacketType(void* self,void*,int* out,int op){
 client::CostumeInitialize();
 if(op==0xD51||op==0xD52||op==0xD53||op==0xD54){out[0]=1;out[1]=op==0xD51?sizeof(ns::State):(op==0xD52||op==0xD54)?sizeof(ns::RecipePacket):sizeof(ns::ItemsPacket);return out;}
 return N<void*(__fastcall*)(void*,void*,int*,int)>(PREVIOUS_TYPE)(self,nullptr,out,op);
}
namespace {
__declspec(naked) bool __fastcall ReadOriginal(void*,void*,void*,ns::u16*){__asm{push ebp}__asm{mov ebp,esp}__asm{sub esp,10h}__asm{push 00C147D6h}__asm{ret}}
using ReadFunction=bool(__fastcall*)(void*,void*,void*,ns::u16*);
ReadFunction volatile readFunction=ReadOriginal;
}
EXPORT bool __fastcall PacketRead(void* self,void*,void* buffer,ns::u16* length){
 for(unsigned i=0;i<64;++i){ns::u16 bytes=0;if(!readFunction(self,nullptr,buffer,&bytes))return false;
  if(bytes>=2&&*static_cast<const ns::u16*>(buffer)==0xD54){if(bytes==sizeof(ns::RecipePacket))client::SelectRecipe(*static_cast<const ns::RecipePacket*>(buffer),bytes);continue;}
  if(length)*length=bytes;return true;
 }return false;
}
extern "C" int __cdecl ReceiveBridge(unsigned op){
 if(op==0x91){client::Close(false);return 0;}
 if(op==0x92||op==0xB3){client::Close(false);if(op==0x92){client::PaletteResetSession();client::CostumeResetSession();}return 0;}
 if(op==0xD51){client::Receive(*reinterpret_cast<const ns::State*>(0x15E8198));return 1;}
 if(op==0xD52){auto& p=*reinterpret_cast<const ns::RecipePacket*>(0x15E8198);if(p.version==ns::Version&&p.length==sizeof(p)&&!p.reserved[0]&&!p.reserved[1]&&!p.reserved[2]&&ns::Valid(p.recipe)){if(p.recipe.domain==ns::Character)client::PaletteRecipe(p.recipe);else if(client::CostumeInitialize())client::CostumeRecipe(p.recipe);}return 1;}
 if(op==0xD53){client::ReceiveItems(*reinterpret_cast<const ns::ItemsPacket*>(0x15E8198));return 1;}
 return 0;
}
EXPORT __declspec(naked) void Dispatch(){__asm{
 pushfd
 pushad
 mov ebx,esp
 sub esp,210h
 and esp,0FFFFFFF0h
 fxsave [esp]
 movzx eax,word ptr [ebp-44C0h]
 push eax
 call ReceiveBridge
 add esp,4
 test eax,eax
 jz stock
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 push 0C9E1DDh
 ret
 stock:
 fxrstor [esp]
 mov esp,ebx
 popad
 popfd
 movsx eax,word ptr [ebp-44C0h]
 push 0C9E2A3h
 ret
}}
namespace {
bool escapeHeld,returnHeld,returnCharacter;
void* Front(){void* head=*reinterpret_cast<void**>(0x131F664);if(!head)return nullptr;void* node=reinterpret_cast<void*>(F(head,4));
 for(unsigned i=0;node&&node!=head&&i<512;++i){void* w=reinterpret_cast<void*>(F(node,8));node=reinterpret_cast<void*>(F(node,4));
  if(!w||!F(w,0x28)||F(w,0x30))continue;unsigned id=F(w,0x2C),vt=F(w,0);
  if(id==0x257||(id==0x133&&vt==0x10281B0))continue;return w;}
 return nullptr;}
}
EXPORT long __stdcall WndProc(void* window,unsigned msg,unsigned key,long detail){
 using Prev=long(__stdcall*)(void*,unsigned,unsigned,long);
 if(msg==0x100&&key==13&&!(static_cast<u32>(detail)&(1u<<30)))returnCharacter=false;
 if(key==13&&(returnHeld||(msg==0x102&&returnCharacter))){if(msg==0x101)returnHeld=false;if(msg==0x102)returnCharacter=false;if(msg==0x100||msg==0x101||msg==0x102)return 0;}
 if(msg==8||(msg==0x1C&&!key)||(msg==6&&!(key&0xffff))){escapeHeld=false;returnHeld=false;returnCharacter=false;}
 if(key==27&&escapeHeld){if(msg==0x101)escapeHeld=false;if(msg==0x100||msg==0x101||msg==0x102)return 0;}
 void* front=Front();
 if(client::Key(front,msg,key)){if(key==27&&msg==0x100)escapeHeld=true;if(key==13&&msg==0x100){returnHeld=true;returnCharacter=true;}return 0;}
 if(msg==0x100&&key==27&&client::Owns(front)){
  escapeHeld=true;if(!(static_cast<u32>(detail)&(1u<<30)))client::Close(true);return 0;}
 return N<Prev>(PREVIOUS_WNDPROC)(window,msg,key,detail);
}
