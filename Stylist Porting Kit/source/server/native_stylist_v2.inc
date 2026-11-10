// Native Stylist Kit v2. SPDX-License-Identifier: GPL-3.0-or-later
// Included by clif.cpp. Rendering IDs are transient and never become stored identity.
#include <common/sql.hpp>
#include <array>
#include <algorithm>
#include <unordered_map>
#include <cstring>
#include "native_stylist_protocol.hpp"
#include "native_stylist.hpp"
#include "native_stylist_hairs.hpp"
#include "native_stylist_storage.hpp"
static_assert(MAX_PACKET_DB>=0x0D53,"Native Stylist v2 packet range");
int32 native_stylist_sql_reconnect(Sql*,bool);
namespace native_stylist {
namespace {
struct Entry {ns::Recipe recipe{};unsigned refs=0;};
struct Active {uint32 login=0;uint16 character=0;std::array<uint16,4> costume{};std::array<uint64,4> instance{};};
struct Session {uint32 token=0,revision=1,login=0;uint16 hair=0,target=0;uint64 instance=0;ns::Recipe character{};std::unordered_map<uint64,ns::Recipe> items,originalItems;std::unordered_map<uint64,t_itemid> itemDesign;};
struct StoredItem {t_itemid item=0;ns::Recipe recipe{};bool valid=false,verified=false;};
Entry entries[ns::RecipeCount]{};
std::unordered_map<uint32,Active> active;
std::unordered_map<uint32,Session> sessions;
std::unordered_map<uint64,StoredItem> item_cache;
void forget_items(const Session& s){for(const auto& pair:s.items)item_cache.erase(pair.first);}
void forget_items(const Active& a){for(auto uid:a.instance)if(uid)item_cache.erase(uid);}
bool schema=false;
constexpr int equip_slots[4]={EQI_COSTUME_HEAD_TOP,EQI_COSTUME_HEAD_MID,EQI_COSTUME_HEAD_LOW,EQI_COSTUME_GARMENT};
constexpr int looks[4]={LOOK_HEAD_TOP,LOOK_HEAD_MID,LOOK_HEAD_BOTTOM,LOOK_ROBE};
bool ready(){
 if(schema)return true;if(!mmysql_handle)return false;
 const char* tables[]={"native_stylist_character_v2","native_stylist_item_v2","native_stylist_look_v2"};
 for(auto table:tables){if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT 1 FROM `%s` LIMIT 0",table))return false;Sql_FreeResult(mmysql_handle);}
 if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT identity_verified FROM native_stylist_item_v2 LIMIT 0"))return false;Sql_FreeResult(mmysql_handle);
 if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=DATABASE() AND TABLE_NAME IN ('native_stylist_character_v2','native_stylist_item_v2') AND ENGINE='InnoDB'"))return false;
 bool transactional=false;if(SQL_SUCCESS==Sql_NextRow(mmysql_handle)){char* count=nullptr;Sql_GetData(mmysql_handle,0,&count,nullptr);transactional=count&&count[0]=='2'&&!count[1];}Sql_FreeResult(mmysql_handle);if(!transactional)return false;
 schema=true;return true;
}
bool blocked(map_session_data* sd){return pc_isdead(sd)||sd->npc_id||sd->state.vending||sd->state.buyingstore||sd->state.trading||sd->state.storage_flag;}
bool hair_valid(map_session_data* sd,unsigned id){
 if((sd->class_&MAPID_FIRSTMASK)==MAPID_SUMMONER)return id>=1&&id<=10;
 if(sd->status.sex==SEX_MALE){for(auto value:ns_hairs::m)if(value==id)return true;}
 else{for(auto value:ns_hairs::f)if(value==id)return true;}return false;
}
uint16 session_hair(map_session_data* sd){
 const unsigned value=sd->status.hair;if(hair_valid(sd,value))return static_cast<uint16>(value);
 if((sd->class_&MAPID_FIRSTMASK)==MAPID_SUMMONER)return static_cast<uint16>(cap_value(static_cast<int>(value),1,10));
 return sd->status.sex==SEX_MALE?ns_hairs::m[0]:ns_hairs::f[0];
}
ns::Recipe canonical(ns::Recipe r){r.id=0;for(unsigned i=0;i<ns::GroupCount;++i)if(!(r.flags&(1ull<<i)))r.rgb[i]=0;return r;}
bool same(ns::Recipe a,ns::Recipe b){a=canonical(a);b=canonical(b);return std::memcmp(&a,&b,sizeof(a))==0;}
void send_recipe(const ns::Recipe& r,map_session_data* sd,send_target target){ns::RecipePacket p{0xD52,sizeof(p),ns::Version,{0,0,0},r};clif_send(&p,sizeof(p),sd,target);}
void release(uint16 id){if(ns::Reserved(id)){auto& e=entries[id-ns::FirstRecipe];if(e.refs&&!--e.refs)e.recipe={};}}
uint16 acquire(const ns::Recipe& definition,map_session_data* sd){
 if(!definition.flags)return 0;
 unsigned begin=definition.domain==ns::Character?0:ns::CharacterCount,end=begin+ns::CharacterCount-1;
 for(unsigned i=begin;i<end;++i)if(entries[i].refs&&same(entries[i].recipe,definition)){++entries[i].refs;return entries[i].recipe.id;}
 for(unsigned i=begin;i<end;++i)if(!entries[i].refs){auto& e=entries[i];e.recipe=definition;e.recipe.id=static_cast<uint16>(ns::FirstRecipe+i);e.refs=1;send_recipe(e.recipe,sd,ALL_CLIENT);return e.recipe.id;}
 return 0;
}
int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
bool decode(const char* hex,ns::Recipe& out){
 if(!hex||std::strlen(hex)!=sizeof(out)*2)return false;
 auto bytes=reinterpret_cast<unsigned char*>(&out);
 for(unsigned i=0;i<sizeof(out);++i){int a=nibble(hex[i*2]),b=nibble(hex[i*2+1]);if(a<0||b<0)return false;bytes[i]=static_cast<unsigned char>((a<<4)|b);}
 return !out.id&&ns::Valid(out);
}
std::array<char,sizeof(ns::Recipe)*2+1> encode(ns::Recipe r){
 r=canonical(r);std::array<char,sizeof(r)*2+1> out{};const char* digits="0123456789ABCDEF";const auto bytes=reinterpret_cast<const unsigned char*>(&r);
 for(unsigned i=0;i<sizeof(r);++i){out[i*2]=digits[bytes[i]>>4];out[i*2+1]=digits[bytes[i]&15];}return out;
}
ns::Recipe base(map_session_data* sd){
 ns::Recipe r{};r.headBase=ns::Reserved(sd->status.hair_color)?0:static_cast<uint16>(sd->status.hair_color);r.bodyBase=ns::Reserved(sd->status.clothes_color)?0:static_cast<uint16>(sd->status.clothes_color);
 r.rgb[0]=0xC7A16A;r.rgb[1]=0xFFC6B2;for(unsigned i=2;i<ns::GroupCount;++i)r.rgb[i]=0x806070;return r;
}
ns::Recipe character(map_session_data* sd){
 auto found=active.find(sd->status.char_id);if(found!=active.end()&&found->second.login==sd->login_id1&&found->second.character){auto r=entries[found->second.character-ns::FirstRecipe].recipe;if(!(r.flags&3))r.headBase=static_cast<uint16>(sd->status.hair_color);if(!(r.flags&~1ull))r.bodyBase=static_cast<uint16>(sd->status.clothes_color);return r;}
 return base(sd);
}
StoredItem stored(const item& it){
 if(!it.unique_id)return {};
 auto found=item_cache.find(it.unique_id);if(found!=item_cache.end())return found->second.item==it.nameid?found->second:StoredItem{};
 StoredItem result{};result.item=it.nameid;
 if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT item_id,HEX(definition),identity_verified FROM native_stylist_item_v2 WHERE unique_id=%llu",static_cast<unsigned long long>(it.unique_id)))return result;
 if(SQL_SUCCESS==Sql_NextRow(mmysql_handle)){char* id=nullptr;char* data=nullptr;char* verified=nullptr;Sql_GetData(mmysql_handle,0,&id,nullptr);Sql_GetData(mmysql_handle,1,&data,nullptr);Sql_GetData(mmysql_handle,2,&verified,nullptr);result.valid=id&&static_cast<t_itemid>(strtoul(id,nullptr,10))==it.nameid&&decode(data,result.recipe)&&result.recipe.domain!=ns::Character;result.verified=result.valid&&verified&&verified[0]=='1'&&!verified[1];}
 Sql_FreeResult(mmysql_handle);item_cache[it.unique_id]=result;return result;
}
enum class CoreIdentity {Ready,Pending,Unsupported,StorageError};
CoreIdentity core_identity(map_session_data* sd,const item& it){
 const uint32 origin=static_cast<uint32>(it.unique_id>>32),sequence=static_cast<uint32>(it.unique_id);
 if(!origin||sequence==0xffffffffu)return CoreIdentity::Unsupported;
 if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT c.uniqueitem_counter,EXISTS(SELECT 1 FROM %s i WHERE i.char_id=%u AND i.unique_id=%llu AND i.nameid=%u AND i.amount=1) FROM %s c WHERE c.char_id=%u LIMIT 1",ns_storage::InventoryTable,sd->status.char_id,static_cast<unsigned long long>(it.unique_id),it.nameid,ns_storage::CharacterTable,origin))return CoreIdentity::StorageError;
 const int next=Sql_NextRow(mmysql_handle);if(next!=SQL_SUCCESS){
  Sql_FreeResult(mmysql_handle);if(next!=SQL_NO_DATA)return CoreIdentity::StorageError;
  // A previously verified instance can outlive its issuing character after trade.
  // Require both persisted ownership and persisted verification, never cache alone.
  if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT EXISTS(SELECT 1 FROM %s i JOIN native_stylist_item_v2 n ON n.unique_id=i.unique_id AND n.item_id=i.nameid AND n.identity_verified=1 WHERE i.char_id=%u AND i.unique_id=%llu AND i.nameid=%u AND i.amount=1)",ns_storage::InventoryTable,sd->status.char_id,static_cast<unsigned long long>(it.unique_id),it.nameid))return CoreIdentity::StorageError;
  if(SQL_SUCCESS!=Sql_NextRow(mmysql_handle)){Sql_FreeResult(mmysql_handle);return CoreIdentity::StorageError;}
  char* verified=nullptr;Sql_GetData(mmysql_handle,0,&verified,nullptr);
  const bool valid=verified&&(verified[0]=='0'||verified[0]=='1')&&!verified[1];const bool exists=valid&&verified[0]=='1';Sql_FreeResult(mmysql_handle);
  return !valid?CoreIdentity::StorageError:exists?CoreIdentity::Ready:CoreIdentity::Unsupported;
 }
 char* counter=nullptr;char* present=nullptr;Sql_GetData(mmysql_handle,0,&counter,nullptr);Sql_GetData(mmysql_handle,1,&present,nullptr);
 uint64 durable=0;bool valid=counter&&*counter&&present&&(present[0]=='0'||present[0]=='1')&&!present[1];
 if(valid)for(const char* digit=counter;*digit;++digit){if(*digit<'0'||*digit>'9'){valid=false;break;}durable=durable*10+static_cast<unsigned>(*digit-'0');if(durable>0xffffffffu){valid=false;break;}}
 const bool exists=valid&&present[0]=='1';Sql_FreeResult(mmysql_handle);
 if(!valid)return CoreIdentity::StorageError;return exists&&durable>sequence?CoreIdentity::Ready:CoreIdentity::Pending;
}
void request_core_save(map_session_data* sd,const item* it=nullptr){
 chrif_save(sd,CSAVE_NORMAL|CSAVE_INVENTORY);
 if(it){const uint32 origin=static_cast<uint32>(it->unique_id>>32);if(origin&&origin!=sd->status.char_id&&origin<=0x7fffffffu)if(auto creator=map_charid2sd(static_cast<int>(origin)))chrif_save(creator,CSAVE_NORMAL);}
}
int costume_slot(map_session_data* sd,unsigned index){
 if(index>=MAX_INVENTORY)return -1;auto data=sd->inventory_data[index];const auto& it=sd->inventory.u.items_inventory[index];
 if(!data||!it.nameid||it.amount!=1||!it.equip||!it.unique_id||data->look<=0||data->look>=20000)return -1;
 for(int slot=0;slot<4;++slot)if(sd->equip_index[equip_slots[slot]]==static_cast<int>(index))return slot;return -1;
}
ns::Recipe item_base(map_session_data* sd,unsigned index,int slot){
 ns::Recipe r{};auto saved=stored(sd->inventory.u.items_inventory[index]);if(saved.valid)r=saved.recipe;
 r.id=0;r.domain=slot==3?ns::RobeCostume:ns::HeadCostume;r.originalView=static_cast<uint16>(sd->inventory_data[index]->look);r.headBase=r.bodyBase=0;
 for(unsigned g=0;g<13;++g)if(!(r.flags&(1ull<<g)))r.rgb[g]=0xA08060;return r;
}
ns::Recipe& draft(Session& s){return s.target?s.items[s.instance]:s.character;}
void items(map_session_data* sd,const Session& s){
 ns::ItemsPacket p{};p.opcode=0xD53;p.length=sizeof(p);p.version=ns::Version;p.session=s.token;
 for(int slot=0;slot<4;++slot){int index=sd->equip_index[equip_slots[slot]];if(index<0||costume_slot(sd,index)!=slot)continue;const auto& it=sd->inventory.u.items_inventory[index];
  auto& out=p.items[p.count++];out.index=static_cast<uint16>(index+1);out.view=static_cast<uint16>(sd->inventory_data[index]->look);out.domain=slot==3?ns::RobeCostume:ns::HeadCostume;out.slot=static_cast<uint8>(slot);out.instance=it.unique_id;
  auto a=active.find(sd->status.char_id);if(a!=active.end())out.recipe=a->second.costume[slot];
  const auto& name=sd->inventory_data[index]->ename;std::memcpy(out.name,name.c_str(),std::min(name.size(),sizeof(out.name)-1));
 }
 clif_send(&p,sizeof(p),sd,SELF);
}
void reply(map_session_data* sd,Session& s,uint8 event,uint8 result){
 auto r=draft(s);r.id=s.target?ns::CostumePreview:ns::CharacterPreview;send_recipe(r,sd,SELF);
 ns::State p{};p.opcode=0xD51;p.length=sizeof(p);p.version=ns::Version;p.event=event;p.result=result;p.sex=sd->status.sex;p.session=s.token;p.revision=s.revision;p.hair=s.hair;
 p.head=static_cast<uint16>(sd->status.hair_color);p.body=static_cast<uint16>(sd->status.clothes_color);p.job=static_cast<uint16>(sd->vd.look[LOOK_BASE]);p.target=s.target;p.species=(sd->class_&MAPID_FIRSTMASK)==MAPID_SUMMONER?1:0;p.instance=s.instance;p.capabilities=s.target?ns::CostumeGroups:ns::AllGroups;p.recipe=r;
 clif_send(&p,sizeof(p),sd,SELF);if(event!=3)items(sd,s);
}
bool select(map_session_data* sd,Session& s,const ns::Request& r){
 if(!r.target){s.target=0;s.instance=0;return true;}
 unsigned index=r.target-1;int slot=costume_slot(sd,index);if(slot<0||sd->inventory.u.items_inventory[index].unique_id!=r.instance)return false;
 s.target=r.target;s.instance=r.instance;if(!s.items.count(s.instance)){s.items[s.instance]=item_base(sd,index,slot);s.originalItems[s.instance]=s.items[s.instance];s.itemDesign[s.instance]=sd->inventory.u.items_inventory[index].nameid;}return true;
}
bool save_character(map_session_data* sd,const ns::Recipe& r){auto hex=encode(r);return SQL_SUCCESS==Sql_Query(mmysql_handle,"INSERT INTO native_stylist_character_v2 (char_id,definition) VALUES (%u,UNHEX('%s')) ON DUPLICATE KEY UPDATE definition=VALUES(definition)",sd->status.char_id,hex.data());}
}
int clamp_palette(int value,int low,int high){return cap_value(value,low,high);}
int clamp_hair(map_session_data* sd,int value,int low,int high){if((sd->class_&MAPID_FIRSTMASK)==MAPID_SUMMONER)return cap_value(value,1,10);return hair_valid(sd,static_cast<unsigned>(value))?value:cap_value(value,low,high);}
int view(map_session_data* sd,int type,int value){
 if(!sd||sd->disguise)return value;auto a=active.find(sd->status.char_id);if(a==active.end()||a->second.login!=sd->login_id1)return value;
 if(a->second.character){auto& r=entries[a->second.character-ns::FirstRecipe].recipe;if(type==LOOK_HAIR_COLOR)return r.flags&3?r.id:value;if(type==LOOK_CLOTHES_COLOR)return r.flags&~1ull?r.id:value;}
 for(int slot=0;slot<4;++slot)if(type==looks[slot]&&value&&a->second.costume[slot]){const auto& r=entries[a->second.costume[slot]-ns::FirstRecipe].recipe;if(value==r.originalView||value==r.id)return r.id;}return value;
}
void apply_view(map_session_data* sd){
 if(!sd)return;sd->vd.look[LOOK_HAIR_COLOR]=view(sd,LOOK_HAIR_COLOR,sd->status.hair_color);sd->vd.look[LOOK_CLOTHES_COLOR]=view(sd,LOOK_CLOTHES_COLOR,sd->status.clothes_color);
 int original[4]={sd->status.head_top,sd->status.head_mid,sd->status.head_bottom,sd->status.robe};for(int i=0;i<4;++i)sd->vd.look[looks[i]]=view(sd,looks[i],original[i]);
}
void refresh_views(map_session_data* sd){
 if(!sd||!ready())return;auto found=active.find(sd->status.char_id);if(found==active.end()||found->second.login!=sd->login_id1)return;auto& a=found->second;
 auto previous=a.costume;auto previousIdentity=a.instance;std::array<uint16,4> next{};std::array<uint64,4> identity{};
 int original[4]={sd->status.head_top,sd->status.head_mid,sd->status.head_bottom,sd->status.robe};
 if(!map_getmapflag(sd->m,MF_NOCOSTUME)&&!sd->status.disable_showcostumes){
  for(int slot=0;slot<4;++slot){int index=sd->equip_index[equip_slots[slot]];if(index<0||costume_slot(sd,index)!=slot||!original[slot])continue;const auto& it=sd->inventory.u.items_inventory[index];
   // A returned or newly equipped instance may have changed on another map process.
   if(a.instance[slot]!=it.unique_id)item_cache.erase(it.unique_id);identity[slot]=it.unique_id;auto saved=stored(it);
   if(!saved.valid||!saved.verified||!saved.recipe.flags||saved.recipe.originalView!=original[slot]||saved.recipe.domain!=(slot==3?ns::RobeCostume:ns::HeadCostume))continue;
   if(previous[slot]&&a.instance[slot]==it.unique_id&&same(entries[previous[slot]-ns::FirstRecipe].recipe,saved.recipe))next[slot]=previous[slot];else next[slot]=acquire(saved.recipe,sd);
  }
 }
 a.costume=next;a.instance=identity;
 for(int slot=0;slot<4;++slot)if(previousIdentity[slot]&&previousIdentity[slot]!=identity[slot])item_cache.erase(previousIdentity[slot]);
 for(int slot=0;slot<4;++slot){int desired=view(sd,looks[slot],original[slot]);if(sd->vd.look[looks[slot]]!=desired)clif_changelook(sd,looks[slot],original[slot]);}
 for(int slot=0;slot<4;++slot)if(previous[slot]!=next[slot])release(previous[slot]);
 auto session=sessions.find(sd->status.char_id);if(session!=sessions.end()&&session->second.login==sd->login_id1)items(sd,session->second);
}
void quit(map_session_data* sd){
 if(!sd)return;auto session=sessions.find(sd->status.char_id);if(session!=sessions.end()&&session->second.login==sd->login_id1){forget_items(session->second);sessions.erase(session);}
 auto found=active.find(sd->status.char_id);if(found!=active.end()&&found->second.login==sd->login_id1){forget_items(found->second);release(found->second.character);for(auto id:found->second.costume)release(id);active.erase(found);}
}
void sync(map_session_data* sd){
 auto session=sessions.find(sd->status.char_id);if(session!=sessions.end()){forget_items(session->second);sessions.erase(session);}auto previous=active.find(sd->status.char_id);if(previous!=active.end()){forget_items(previous->second);release(previous->second.character);for(auto id:previous->second.costume)release(id);active.erase(previous);}if(!ready())return;
 for(auto& entry:entries)if(entry.refs)send_recipe(entry.recipe,sd,SELF);
 Active state{};state.login=sd->login_id1;ns::Recipe r{};
 if(SQL_SUCCESS==Sql_Query(mmysql_handle,"SELECT HEX(definition) FROM native_stylist_character_v2 WHERE char_id=%u",sd->status.char_id)){
  if(SQL_SUCCESS==Sql_NextRow(mmysql_handle)){char* data=nullptr;Sql_GetData(mmysql_handle,0,&data,nullptr);if(!decode(data,r)||r.domain!=ns::Character)r={};}Sql_FreeResult(mmysql_handle);
 }
 if(r.flags)state.character=acquire(r,sd);active[sd->status.char_id]=state;if(state.character)send_recipe(entries[state.character-ns::FirstRecipe].recipe,sd,SELF);apply_view(sd);
}
void map_loaded(map_session_data* sd){
 if(!sd)return;auto found=active.find(sd->status.char_id);if(found==active.end()||found->second.login!=sd->login_id1)return;
 // Authentication reserves the saved views. Replay definitions once the client
 // has created its world actor, then restore that actor's transient view IDs.
 for(const auto& entry:entries)if(entry.refs)send_recipe(entry.recipe,sd,SELF);
 apply_view(sd);const auto& a=found->second;
 if(a.character){const auto& r=entries[a.character-ns::FirstRecipe].recipe;
  if(r.flags&3)clif_sprite_change(sd,sd->id,LOOK_HAIR_COLOR,sd->vd.look[LOOK_HAIR_COLOR],0,SELF);
  if(r.flags&~1ull)clif_sprite_change(sd,sd->id,LOOK_CLOTHES_COLOR,sd->vd.look[LOOK_CLOTHES_COLOR],0,SELF);
 }
 for(int slot=0;slot<4;++slot)if(a.costume[slot])clif_sprite_change(sd,sd->id,looks[slot],sd->vd.look[looks[slot]],0,SELF);
}
bool open(map_session_data* sd){
 if(!ready()){clif_displaymessage(sd->fd,"Native Stylist: install or upgrade schema_v2.sql first.");return false;}
 auto previousSession=sessions.find(sd->status.char_id);if(previousSession!=sessions.end()){forget_items(previousSession->second);sessions.erase(previousSession);}
 // Start each editing session from durable item colors, including hidden costumes.
 for(int slot=0;slot<4;++slot){const int index=sd->equip_index[equip_slots[slot]];if(index>=0&&costume_slot(sd,index)==slot)item_cache.erase(sd->inventory.u.items_inventory[index].unique_id);}
 if(!active.count(sd->status.char_id))sync(sd);refresh_views(sd);
 if(!blocked(sd))for(int slot=0;slot<4;++slot){const int index=sd->equip_index[equip_slots[slot]];if(index>=0&&costume_slot(sd,index)==slot){const auto& it=sd->inventory.u.items_inventory[index];if(core_identity(sd,it)==CoreIdentity::Pending){request_core_save(sd,&it);break;}}}
 Session s{};s.token=static_cast<uint32>(rnd());if(!s.token)s.token=1;s.login=sd->login_id1;s.hair=session_hair(sd);s.character=character(sd);s.character.id=0;
 sessions[sd->status.char_id]=s;reply(sd,sessions[sd->status.char_id],0,blocked(sd)?2:0);return true;
}
// Validate and reserve the full draft before persisting any appearance row.
uint8 confirm_all(map_session_data* sd,Session& s){
 struct PendingItem {uint64 uid=0;t_itemid item=0;ns::Recipe recipe{};uint16 reserved=0;};
 std::array<PendingItem,4> pending{};unsigned count=0;
 for(const auto& pair:s.items){
  auto original=s.originalItems.find(pair.first);auto design=s.itemDesign.find(pair.first);
  if(original==s.originalItems.end()||design==s.itemDesign.end())return 9;
  if(pair.first!=s.instance&&same(pair.second,original->second))continue;
  unsigned index=MAX_INVENTORY;int slot=-1;
  for(int candidate=0;candidate<4;++candidate){int at=sd->equip_index[equip_slots[candidate]];if(at>=0&&costume_slot(sd,static_cast<unsigned>(at))==candidate&&sd->inventory.u.items_inventory[at].unique_id==pair.first){if(slot>=0)return 9;index=static_cast<unsigned>(at);slot=candidate;}}
  if(slot<0||count>=pending.size())return 9;
  const auto& it=sd->inventory.u.items_inventory[index];auto recipe=canonical(pair.second);
  if(it.nameid!=design->second||!ns::Valid(recipe)||recipe.originalView!=sd->inventory_data[index]->look||recipe.domain!=(slot==3?ns::RobeCostume:ns::HeadCostume))return 9;
  auto identity=core_identity(sd,it);if(identity!=CoreIdentity::Ready){if(identity==CoreIdentity::Pending)request_core_save(sd,&it);return identity==CoreIdentity::Pending?10:identity==CoreIdentity::Unsupported?11:5;}
  pending[count++]={it.unique_id,it.nameid,recipe,0};
 }
 auto characterRecipe=canonical(s.character);if(!ns::Valid(characterRecipe)||characterRecipe.domain!=ns::Character||!hair_valid(sd,s.hair))return 4;
 uint16 characterId=acquire(characterRecipe,sd);
 auto release_all=[&](){release(characterId);for(unsigned i=0;i<count;++i)release(pending[i].reserved);};
 if(characterRecipe.flags&&!characterId){release_all();return 8;}
 for(unsigned i=0;i<count;++i){pending[i].reserved=acquire(pending[i].recipe,sd);if(pending[i].recipe.flags&&!pending[i].reserved){release_all();return 8;}}
 // Core connections normally auto-reconnect. Disable it for this transaction so
 // a connection loss cannot silently continue later writes on a new connection.
 struct ReconnectFence {bool valid;ReconnectFence():valid(native_stylist_sql_reconnect(mmysql_handle,false)==SQL_SUCCESS){}~ReconnectFence(){if(valid)native_stylist_sql_reconnect(mmysql_handle,true);}} fence;
 if(!fence.valid||SQL_ERROR==Sql_Query(mmysql_handle,"START TRANSACTION")){release_all();return 5;}
 bool saved=save_character(sd,characterRecipe);
 for(unsigned i=0;saved&&i<count;++i){auto& item=pending[i];auto hex=encode(item.recipe);saved=SQL_SUCCESS==Sql_Query(mmysql_handle,"INSERT INTO native_stylist_item_v2 (unique_id,item_id,definition,identity_verified) VALUES (%llu,%u,UNHEX('%s'),1) ON DUPLICATE KEY UPDATE item_id=VALUES(item_id),definition=VALUES(definition),identity_verified=1",static_cast<unsigned long long>(item.uid),item.item,hex.data());}
 if(!saved||SQL_ERROR==Sql_Query(mmysql_handle,"COMMIT")){Sql_Query(mmysql_handle,"ROLLBACK");release_all();return 5;}
 // Only committed definitions may replace the live actor or cache.
 auto& a=active[sd->status.char_id];auto previous=a.character;a.character=characterId;
 for(unsigned i=0;i<count;++i){const auto& item=pending[i];item_cache[item.uid]=StoredItem{item.item,item.recipe,true,true};s.originalItems[item.uid]=s.items[item.uid];}
 if(sd->status.hair!=s.hair)pc_changelook(sd,LOOK_HAIR,s.hair);
 pc_changelook(sd,LOOK_HAIR_COLOR,characterRecipe.headBase);pc_changelook(sd,LOOK_CLOTHES_COLOR,characterRecipe.bodyBase);release(previous);
 refresh_views(sd);for(unsigned i=0;i<count;++i)release(pending[i].reserved);
 chrif_save(sd,CSAVE_NORMAL);return 0;
}
void parse(int fd,map_session_data* sd){
 ns::Request r{};std::memcpy(&r,RFIFOP(fd,0),sizeof(r));
 if(r.opcode!=0xD50||r.version!=ns::Version||r.action<1||r.action>8||r.reserved||r.slot>=8)return;
 auto found=sessions.find(sd->status.char_id);if(found==sessions.end())return;auto& s=found->second;
 if(s.login!=sd->login_id1||r.session!=s.token)return;
 if(r.action==1){reply(sd,s,3,0);forget_items(s);sessions.erase(found);return;}
 if(r.revision!=s.revision){reply(sd,s,1,3);return;}if(blocked(sd)){reply(sd,s,1,2);return;}
 if(r.action==7||r.action==8){ns::Request selection=r;if(r.action==8){selection.target=0;selection.instance=0;}if(!select(sd,s,selection)){reply(sd,s,1,9);return;}if(r.action==7&&s.target){const auto& it=sd->inventory.u.items_inventory[s.target-1];if(core_identity(sd,it)==CoreIdentity::Pending)request_core_save(sd,&it);}++s.revision;reply(sd,s,1,0);return;}
 if(r.target!=s.target||r.instance!=s.instance||!select(sd,s,r)){reply(sd,s,1,9);return;}
 if(r.action==6){if(s.target){unsigned index=s.target-1;draft(s)=item_base(sd,index,costume_slot(sd,index));if(!hair_valid(sd,s.hair))s.hair=session_hair(sd);}else{s.hair=session_hair(sd);s.character=character(sd);s.character.id=0;}++s.revision;reply(sd,s,1,0);return;}
 if(!hair_valid(sd,r.hair)){reply(sd,s,1,4);return;}
 if(r.action==4){
  if(s.target){reply(sd,s,1,4);return;}auto hex=encode(s.character);
  if(SQL_ERROR==Sql_Query(mmysql_handle,"INSERT INTO native_stylist_look_v2 (char_id,slot,hair,definition) VALUES (%u,%u,%u,UNHEX('%s')) ON DUPLICATE KEY UPDATE hair=VALUES(hair),definition=VALUES(definition)",sd->status.char_id,r.slot,s.hair,hex.data())){reply(sd,s,1,5);return;}
  ++s.revision;reply(sd,s,1,7);return;
 }
 if(r.action==5){
  if(s.target){reply(sd,s,1,4);return;}
  if(SQL_ERROR==Sql_Query(mmysql_handle,"SELECT hair,HEX(definition) FROM native_stylist_look_v2 WHERE char_id=%u AND slot=%u",sd->status.char_id,r.slot)){reply(sd,s,1,5);return;}
  if(SQL_SUCCESS!=Sql_NextRow(mmysql_handle)){Sql_FreeResult(mmysql_handle);reply(sd,s,1,6);return;}
  char* hair=nullptr;char* data=nullptr;Sql_GetData(mmysql_handle,0,&hair,nullptr);Sql_GetData(mmysql_handle,1,&data,nullptr);unsigned selected=hair?static_cast<unsigned>(strtoul(hair,nullptr,10)):0;ns::Recipe saved{};bool ok=decode(data,saved)&&saved.domain==ns::Character&&hair_valid(sd,selected);Sql_FreeResult(mmysql_handle);
  if(!ok){reply(sd,s,1,4);return;}s.hair=static_cast<uint16>(selected);s.character=saved;++s.revision;reply(sd,s,1,0);return;
 }
 auto candidate=canonical(r.recipe);
 if(!ns::Valid(candidate)||candidate.domain!=(s.target?draft(s).domain:ns::Character)||candidate.originalView!=draft(s).originalView||candidate.headBase!=draft(s).headBase||candidate.bodyBase!=draft(s).bodyBase){reply(sd,s,1,4);return;}
 if(r.action==2){s.hair=r.hair;draft(s)=candidate;++s.revision;reply(sd,s,1,0);return;}
 if(r.hair!=s.hair||!same(candidate,draft(s))){reply(sd,s,1,3);return;}
 uint8 result=confirm_all(sd,s);if(result){reply(sd,s,1,result);return;}
 ++s.revision;reply(sd,s,2,1);
}
}
