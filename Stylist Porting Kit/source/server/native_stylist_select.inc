// Saved character colors for the native character-list preview.
// Included by char_clif.cpp. SPDX-License-Identifier: GPL-3.0-or-later
#include <custom/native_stylist_protocol.hpp>
#include <custom/native_stylist_storage.hpp>
namespace native_stylist_select {
bool decode(const char* hex,ns::Recipe& out){
 if(!hex||std::strlen(hex)!=sizeof(out)*2)return false;
 auto bytes=reinterpret_cast<unsigned char*>(&out);
 for(unsigned i=0;i<sizeof(out);++i){unsigned value=0;for(unsigned j=0;j<2;++j){char c=hex[i*2+j];unsigned n=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:16;if(n>15)return false;value=value*16+n;}bytes[i]=static_cast<unsigned char>(value);}
 return !out.id&&ns::Valid(out);
}
// Costume recipes are read-only overlays on constructed character-list fields.
void costumes(int32 fd,CHARACTER_INFO& info,unsigned row){
 ns::Recipe saved[4]{};bool conflict[4]{};
 const unsigned original[4]={static_cast<unsigned>(info.accessory2),static_cast<unsigned>(info.accessory3),static_cast<unsigned>(info.accessory),static_cast<unsigned>(info.robePalette)};
 if(SQL_SUCCESS!=Sql_Query(sql_handle,"SELECT HEX(n.definition),i.equip FROM %s i JOIN native_stylist_item_v2 n ON n.unique_id=i.unique_id AND n.item_id=i.nameid WHERE i.char_id=%u AND i.amount=1 AND i.unique_id<>0 AND n.identity_verified=1 AND (i.equip & 15360)<>0",ns_storage::InventoryTable,info.GID))return;
 while(SQL_SUCCESS==Sql_NextRow(sql_handle)){
  char* data=nullptr;char* equipment=nullptr;Sql_GetData(sql_handle,0,&data,nullptr);Sql_GetData(sql_handle,1,&equipment,nullptr);ns::Recipe recipe{};
  if(!equipment||!decode(data,recipe)||recipe.domain==ns::Character||!recipe.flags)continue;
  const unsigned equip=static_cast<unsigned>(std::strtoul(equipment,nullptr,10));
  for(unsigned slot=0;slot<4;++slot)if((equip&(1024u<<slot))&&original[slot]==recipe.originalView&&recipe.domain==(slot==3?ns::RobeCostume:ns::HeadCostume)){
   if(saved[slot].flags)conflict[slot]=true;else saved[slot]=recipe;
  }
 }
 Sql_FreeResult(sql_handle);
 for(unsigned slot=0;slot<4;++slot)if(saved[slot].flags&&!conflict[slot]){
  auto recipe=saved[slot];recipe.id=static_cast<ns::u16>(ns::FirstCostume+row*4+slot);
  ns::RecipePacket packet{};packet.opcode=0xD54;packet.length=sizeof(packet);packet.version=ns::Version;packet.recipe=recipe;
  if(!socket_send(fd,packet))continue;
  switch(slot){case 0:info.accessory2=static_cast<int16>(recipe.id);break;case 1:info.accessory3=static_cast<int16>(recipe.id);break;case 2:info.accessory=static_cast<int16>(recipe.id);break;case 3:info.robePalette=recipe.id;break;}
 }
}
void send(int32 fd,CHARACTER_INFO* characters,unsigned count){
 ns::RecipePacket reset{};reset.opcode=0xD54;reset.length=sizeof(reset);reset.version=ns::Version;reset.reserved[0]=1;if(!socket_send(fd,reset))return;
 for(unsigned i=0;i<count&&i<MAX_CHARS&&i<ns::CharacterCount-1;++i){
  auto& info=characters[i];ns::Recipe recipe{};bool valid=false;
  if(SQL_SUCCESS==Sql_Query(sql_handle,"SELECT HEX(definition) FROM native_stylist_character_v2 WHERE char_id=%u",info.GID)){
   if(SQL_SUCCESS==Sql_NextRow(sql_handle)){char* data=nullptr;Sql_GetData(sql_handle,0,&data,nullptr);valid=decode(data,recipe)&&recipe.domain==ns::Character;}Sql_FreeResult(sql_handle);
  }
  if(valid&&recipe.flags){
   recipe.id=static_cast<ns::u16>(ns::FirstRecipe+i);ns::RecipePacket packet{};packet.opcode=0xD54;packet.length=sizeof(packet);packet.version=ns::Version;packet.recipe=recipe;
   if(socket_send(fd,packet)){
    if(recipe.flags&3)info.headpalette=static_cast<int16>(recipe.id);
    if(recipe.flags&~1ull)info.bodypalette=static_cast<int16>(recipe.id);
   }
  }
  costumes(fd,info,i);
 }
}
}
