#include "native.hpp"
#include "palette_hooks.hpp"
namespace {
// Guarded five-byte prologues: 55 8B EC 6A FF.
__declspec(naked) void* __fastcall BodyOriginal(void*,void*,client::NativeString*,int,int,int,int,int,int){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00B42585h}__asm{ret}}
__declspec(naked) void* __fastcall HairOriginal(void*,void*,client::NativeString*,int,int,int,int){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00B42DB5h}__asm{ret}}
// Volatile indirection prevents MSVC's local register analysis from treating the
// naked thunk as preserving registers that the jumped-to native function changes.
using BodyFunction=void*(__fastcall*)(void*,void*,client::NativeString*,int,int,int,int,int,int);
using HairFunction=void*(__fastcall*)(void*,void*,client::NativeString*,int,int,int,int);
BodyFunction volatile bodyFunction=BodyOriginal;
HairFunction volatile hairFunction=HairOriginal;
}
void* client::OriginalBodyPalette(void* context,NativeString* out,int job,int sex,int palette,int bodyOverride,int variant,int extra){return bodyFunction(context,nullptr,out,job,sex,palette,bodyOverride,variant,extra);}
void* client::OriginalHairPalette(void* context,NativeString* out,int hair,int job,int sex,int palette){return hairFunction(context,nullptr,out,hair,job,sex,palette);}
EXPORT void* __fastcall BodyPaletteName(void* context,void*,client::NativeString* out,int job,int sex,int palette,int bodyOverride,int variant,int extra){
 client::OriginalBodyPalette(context,out,job,sex,palette,bodyOverride,variant,extra);
 client::RewriteCharacterPalette(out,static_cast<unsigned>(palette),true,context,job,sex,bodyOverride,variant,0,extra);return out;
}
EXPORT void* __fastcall HairPaletteName(void* context,void*,client::NativeString* out,int hair,int job,int sex,int palette){
 client::OriginalHairPalette(context,out,hair,job,sex,palette);
 client::RewriteCharacterPalette(out,static_cast<unsigned>(palette),false,context,job,sex,0,0,hair,0);return out;
}
// Carry the actor's original class through the native builder's unused final
// DWORD as class+1. Zero remains unknown for unmodified native callers. The
// register exchange preserves EAX and all flags while replacing that argument.
EXPORT __declspec(naked) void WorldPaletteRace(){__asm{push eax}__asm{mov eax,dword ptr[esi+4CCh]}__asm{lea eax,[eax+1]}__asm{xchg eax,dword ptr[esp]}__asm{push eax}__asm{push ebx}__asm{push dword ptr[esi+48h]}__asm{push 00D3DCF9h}__asm{ret}}
EXPORT __declspec(naked) void RenderPaletteRace(){__asm{push eax}__asm{mov eax,dword ptr[edi+14h]}__asm{lea eax,[eax+1]}__asm{xchg eax,dword ptr[esp]}__asm{push dword ptr[ebp-0B4h]}__asm{push 007D8A24h}__asm{ret}}
