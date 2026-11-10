#pragma once
// Native Stylist Kit v2. SPDX-License-Identifier: GPL-3.0-or-later
// Fixed-width client and rAthena packet layouts.
namespace ns {
using u8=unsigned char;using u16=unsigned short;using u32=unsigned int;using u64=unsigned long long;
constexpr unsigned GroupCount=34;
constexpr u16 FirstRecipe=30000,RecipeCount=2048,CharacterCount=1024,FirstCostume=FirstRecipe+CharacterCount;
constexpr u16 CharacterPreview=FirstCostume-1,CostumePreview=FirstRecipe+RecipeCount-1;
constexpr u8 Version=2;
enum Domain : u8 {Character=0,HeadCostume=1,RobeCostume=2};
#pragma pack(push,1)
struct Recipe {u16 id;u8 domain,reserved;u16 headBase,bodyBase,originalView,spare;u64 flags;u32 rgb[GroupCount];};
struct Request {u16 opcode;u8 version,action;u32 session,revision;u16 hair,slot,target,reserved;u64 instance;Recipe recipe;};
struct State {u16 opcode,length;u8 version,event,result,sex;u32 session,revision;u16 hair,head,body,job,target,species;u64 instance,capabilities;Recipe recipe;};
struct RecipePacket {u16 opcode,length;u8 version,reserved[3];Recipe recipe;};
struct Item {u16 index,view;u8 domain,slot;u16 recipe;u64 instance;char name[64];};
struct ItemsPacket {u16 opcode,length;u8 version,count;u16 reserved;u32 session;Item items[4];};
#pragma pack(pop)
static_assert(sizeof(Recipe)==156&&sizeof(Request)==184&&sizeof(State)==200&&sizeof(RecipePacket)==164&&sizeof(Item)==80&&sizeof(ItemsPacket)==332,"Native Stylist v2 wire ABI");
constexpr bool Reserved(unsigned id){return id>=FirstRecipe&&id<FirstRecipe+RecipeCount;}
constexpr bool CharacterID(unsigned id){return id>=FirstRecipe&&id<FirstCostume;}
constexpr bool CostumeID(unsigned id){return id>=FirstCostume&&id<FirstRecipe+RecipeCount;}
constexpr u64 AllGroups=(1ull<<GroupCount)-1;
constexpr u64 CostumeGroups=(1ull<<13)-1;
inline bool Valid(const Recipe& r){
 if(r.reserved||r.spare||r.domain>RobeCostume||r.headBase>=FirstRecipe||r.bodyBase>=FirstRecipe||(r.flags&~(r.domain==Character?AllGroups:CostumeGroups)))return false;
 if(r.domain==Character){if(r.originalView||(r.id&&!CharacterID(r.id)))return false;}
 else if(!r.originalView||r.originalView>=20000||(r.id&&!CostumeID(r.id)))return false;
 for(unsigned i=0;i<GroupCount;++i)if(r.rgb[i]>0xffffffu)return false;
 return true;
}
}
