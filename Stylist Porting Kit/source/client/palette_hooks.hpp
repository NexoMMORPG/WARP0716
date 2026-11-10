#pragma once
#include "protocol.hpp"
namespace client {
struct NativeString {unsigned char storage[16];ns::u32 size,capacity;};
static_assert(sizeof(NativeString)==24,"Target native string ABI");
struct CharacterMaterialInfo {ns::u64 capabilities,headGroups,bodyGroups;ns::u32 headFamily,bodyFamily;bool headReady,bodyReady;ns::u32 sourceRGB[ns::GroupCount];};
bool CharacterMaterials(void* actor,ns::u16 logicalHair,ns::u16 headBase,ns::u16 bodyBase,CharacterMaterialInfo* out);
bool PaletteCacheFull();
// Only around native UI preview construction/draw. Release after renderer teardown.
void LocalPalettePreview(bool);
void ReleaseLocalPalettes();
void PaletteResetSession();
bool SelectRecipe(const ns::RecipePacket&,unsigned);
void PaletteRecipe(const ns::Recipe&);
bool ReadResource(const char*,void*,unsigned);
bool ReadResourceAlloc(const char*,void**,unsigned*);
void FreeResource(void*,unsigned);
void RewriteCharacterPalette(NativeString*,unsigned,bool,void*,int,int,int,int,int,int);
void* OriginalBodyPalette(void*,NativeString*,int,int,int,int,int,int);
void* OriginalHairPalette(void*,NativeString*,int,int,int,int);
}
extern "C" __declspec(dllexport) void* __fastcall BodyPaletteName(void*,void*,client::NativeString*,int,int,int,int,int,int);
extern "C" __declspec(dllexport) void* __fastcall HairPaletteName(void*,void*,client::NativeString*,int,int,int,int);
