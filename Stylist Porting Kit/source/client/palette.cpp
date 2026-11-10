#include "native.hpp"
#include "palette_hooks.hpp"
#include "costume.hpp"
namespace {
constexpr unsigned MaxContents=16384,MaxBindings=8192,PathLimit=252,Buckets=256;
struct Content {Content* next;Content* identityNext;u32 hash,low,high;bool local;u8 rgba[1024];};
struct Binding {Binding* next;u32 family,source;u16 base;u8 body,reserved;Content* content;};
struct StoredRecipe {ns::Recipe recipe;Binding* bindings;};
StoredRecipe* recipes[ns::CharacterCount];Content* contents[Buckets];Content* identities[Buckets];
bool localPalettePreview;
unsigned contentCount,bindingCount;u32 nextLow=1,nextHigh;bool capacityFull;
volatile long cacheLock;
void Lock(){__asm{mov ecx,offset cacheLock}__asm{retry:xor eax,eax}__asm{mov edx,1}__asm{lock cmpxchg dword ptr[ecx],edx}__asm{test eax,eax}__asm{jz acquired}__asm{pause}__asm{jmp retry}__asm{acquired:}}
void Unlock(){__asm{mov dword ptr[cacheLock],0}}
void* Alloc(unsigned n){return N<void*(__cdecl*)(unsigned)>(0xDBBC4F)(n);}
void Free(void* p,unsigned n){N<void(__cdecl*)(void*,unsigned)>(0xDBBC7F)(p,n);}
void Copy(void* a,const void* b,unsigned n){auto out=static_cast<u8*>(a);auto in=static_cast<const u8*>(b);for(unsigned i=0;i<n;++i)out[i]=in[i];}
bool Equal(const void* a,const void* b,unsigned n){auto x=static_cast<const u8*>(a);auto y=static_cast<const u8*>(b);for(unsigned i=0;i<n;++i)if(x[i]!=y[i])return false;return true;}
unsigned Length(const char* p,unsigned limit=PathLimit){unsigned n=0;while(n<limit&&p[n])++n;return n;}
bool Starts(const char* p,unsigned n,const char* prefix){unsigned i=0;while(prefix[i]){if(i==n||p[i]!=prefix[i])return false;++i;}return true;}
void Hex(char* out,u32 n){const char* digits="0123456789abcdef";for(int i=7;i>=0;--i){out[i]=digits[n&15];n>>=4;}}
unsigned Decimal(char* out,unsigned value){char reverse[10];unsigned n=0;do{reverse[n++]=static_cast<char>('0'+value%10);value/=10;}while(value);for(unsigned i=0;i<n;++i)out[i]=reverse[n-1-i];return n;}
const char* Text(const client::NativeString& s){return s.capacity>15?*reinterpret_cast<const char* const*>(s.storage):reinterpret_cast<const char*>(s.storage);}
void Init(client::NativeString& s){for(unsigned i=0;i<16;++i)s.storage[i]=0;s.size=0;s.capacity=15;}
void Destroy(client::NativeString& s){N<void(__thiscall*)(client::NativeString*)>(0x4F08F0)(&s);}
void Assign(client::NativeString* s,const char* bytes,unsigned n){N<void*(__thiscall*)(client::NativeString*,const char*,unsigned)>(0x4F1940)(s,bytes,n);}
struct Family {u32 hash,source,material;unsigned colorAt,colorBytes,pathBytes;char prefix[PathLimit];};
bool Number(const char* p,unsigned first,unsigned end,unsigned* result){
 if(first==end)return false;unsigned value=0;for(unsigned i=first;i<end;++i){if(p[i]<'0'||p[i]>'9')return false;value=value*10+static_cast<unsigned>(p[i]-'0');if(value>65535)return false;}*result=value;return true;
}
bool FamilyOf(const client::NativeString& name,unsigned expected,bool body,Family& out){
 const char* p=Text(name);const unsigned n=name.size;
 if(!p||!n||n>=PathLimit||name.capacity<n||n<6||!Equal(p+n-4,".pal",4))return false;
 unsigned end=n-4,first=end;while(first&&p[first-1]>='0'&&p[first-1]<='9')--first;
 bool alternate=false;if(body)for(unsigned i=0;i+9<n;++i)if(Equal(p+i,"costume_",8)&&(i==0||p[i-1]=='\\')){alternate=true;break;}
 if(alternate){if(!first||p[first-1]!='_')return false;end=first-1;first=end;while(first&&p[first-1]>='0'&&p[first-1]<='9')--first;}
 unsigned id=0;if(!first||p[first-1]!='_'||!Number(p,first,end,&id)||id!=expected)return false;
 out.source=0;out.colorAt=first;out.colorBytes=end-first;out.pathBytes=n;
 unsigned at=0,start=0;if(Starts(p,n,"data\\palette\\"))start=5;else if(!Starts(p,n,"palette\\")){const char* prefix="palette\\";while(prefix[at]){out.prefix[at]=prefix[at];++at;}}
 if(at+first>=PathLimit)return false;for(unsigned i=start;i<first;++i)out.prefix[at++]=p[i];out.prefix[at]=0;out.hash=ns::Hash(out.prefix,at);out.material=out.hash;return true;
}
void MaterialPath(char* path,u32 hash){const char* prefix="native_stylist\\materials\\";unsigned at=0;while(prefix[at]){path[at]=prefix[at];++at;}Hex(path+at,hash);at+=8;const char* ext=".nsm";for(unsigned i=0;i<5;++i)path[at+i]=ext[i];}
bool SpritePath(client::NativeString& source,char* path){
 const char* p=Text(source);unsigned at=0,n=source.size;if(!p||!n||n>=PathLimit||source.capacity<n)return false;
 unsigned start=0;if(Starts(p,n,"data\\sprite\\"))start=5;else if(!Starts(p,n,"sprite\\")){const char* prefix="sprite\\";while(prefix[at]){path[at]=prefix[at];++at;}}
 if(at+n-start>=PathLimit)return false;Copy(path+at,p+start,n-start);path[at+n-start]=0;return true;
}
void BodyName(void* context,client::NativeString& source,int job,int sex,int bodyOverride,int bodyStyle,int raceToken){
 // Native resource type 1 selects SPR. Type 0 selects ACT and has a different source hash.
 if(job==22&&raceToken>0)N<void*(__thiscall*)(void*,client::NativeString*,int,int,int,int,int)>(0xB42910)(context,&source,1,raceToken-1,22,sex,0);
 else N<void*(__thiscall*)(void*,client::NativeString*,int,int,int,int,int)>(0xB43AF0)(context,&source,1,job,sex,bodyOverride,bodyStyle);
}
bool BodySource(Family& family,void* context,int job,int sex,int bodyOverride,int bodyStyle,int raceToken){
 client::NativeString source;Init(source);BodyName(context,source,job,sex,bodyOverride,bodyStyle,raceToken);
 char path[PathLimit];const bool ok=SpritePath(source,path);if(ok)family.source=ns::Hash(path,Length(path));Destroy(source);return ok;
}
bool HairSource(Family& family,void* context,int job,int sex,int hair){
 client::NativeString source;Init(source);
 N<void*(__thiscall*)(void*,client::NativeString*,int,int,int,int*)>(0xB433B0)(context,&source,1,job,sex,&hair);
 char path[PathLimit];const bool ok=SpritePath(source,path);Destroy(source);if(!ok)return false;
 const unsigned n=Length(path);if(n<5||!Equal(path+n-4,".spr",4))return false;
 unsigned first=0;for(unsigned i=0;i<n;++i)if(path[i]=='\\')first=i+1;
 if(first>=n-4)return false;
 family.source=ns::Hash(path,n);
 return ns::HeadMaterialHash(path,n,&family.material);
}
bool LoadMaterial(const Family& family,bool body,ns::Material& material,ns::BodyRemap& remap){
 char path[64];const u32 key=body?family.source:family.material;
 if(body){const char* prefix="native_stylist\\body_materials\\";unsigned at=0;while(prefix[at]){path[at]=prefix[at];++at;}Hex(path+at,key);at+=8;const char* ext=".nsb";for(unsigned i=0;i<5;++i)path[at+i]=ext[i];}
 else MaterialPath(path,key);
 if(body){ns::BodyMaterial data;if(!client::ReadResource(path,&data,sizeof(data))||!ns::ValidMaterial(data.material,key)||!ns::ValidBodyRemap(data.remap))return false;Copy(&material,&data.material,sizeof(material));Copy(&remap,&data.remap,sizeof(remap));return true;}
 if(client::ReadResource(path,&material,sizeof(material))&&ns::ValidMaterial(material,key))return true;
 if(family.material!=family.hash){MaterialPath(path,family.hash);return client::ReadResource(path,&material,sizeof(material))&&ns::ValidMaterial(material,family.hash);}
 return false;
}
bool BasePath(const client::NativeString& original,const Family& family,unsigned base,char* out,unsigned* size){
 const char* p=Text(original);char digits[10];const unsigned count=Decimal(digits,base),tail=family.pathBytes-family.colorAt-family.colorBytes;
 if(family.colorAt+count+tail>=PathLimit)return false;
 Copy(out,p,family.colorAt);Copy(out+family.colorAt,digits,count);Copy(out+family.colorAt+count,p+family.colorAt+family.colorBytes,tail);*size=family.colorAt+count+tail;out[*size]=0;return true;
}
bool ReadOriginalBase(const client::NativeString& original,const Family& family,unsigned base,u8* rgba){
 char path[PathLimit],name[PathLimit];unsigned n=0;if(!BasePath(original,family,base,name,&n))return false;
 unsigned at=0,start=0;if(Starts(name,n,"data\\palette\\"))start=5;else if(!Starts(name,n,"palette\\")){const char* prefix="palette\\";while(prefix[at]){path[at]=prefix[at];++at;}}
 if(at+n-start>=PathLimit)return false;Copy(path+at,name+start,n-start);path[at+n-start]=0;return client::ReadResource(path,rgba,1024);
}
bool EmbeddedPalette(bool body,void* context,int job,int sex,int bodyOverride,int bodyStyle,int raceToken,int hair,u8* rgba){
 client::NativeString source;Init(source);
 if(body)BodyName(context,source,job,sex,bodyOverride,bodyStyle,raceToken);
 else N<void*(__thiscall*)(void*,client::NativeString*,int,int,int,int*)>(0xB433B0)(context,&source,1,job,sex,&hair);
 char path[PathLimit];const bool valid=SpritePath(source,path);
 void* data=nullptr;unsigned bytes=0;const bool loaded=valid&&client::ReadResourceAlloc(path,&data,&bytes);bool ok=false;
 if(loaded){auto raw=static_cast<const u8*>(data);if(bytes>=1032&&raw[0]=='S'&&raw[1]=='P'&&raw[3]==2&&(raw[2]==0||raw[2]==1)&&(raw[4]||raw[5])){Copy(rgba,raw+bytes-1024,1024);ok=true;}client::FreeResource(data,bytes);}
 Destroy(source);return ok;
}
bool LoadBase(const client::NativeString& original,const Family& family,unsigned base,const ns::Material* material,const ns::BodyRemap* remap,bool body,void* context,int job,int sex,int bodyOverride,int bodyStyle,int raceToken,int hair,u8* rgba){
 if(!base&&material){Copy(rgba,material->rgba,1024);return true;}
 if(base&&body){if(!ReadOriginalBase(original,family,base,rgba))return false;if(remap)ns::RemapBody(rgba,*remap);return true;}
 if(base){if(family.material!=family.hash&&ReadOriginalBase(original,family,base,rgba))return true;char path[64];const char* prefix="native_stylist\\palettes\\";unsigned at=0;while(prefix[at]){path[at]=prefix[at];++at;}Hex(path+at,family.material);at+=8;path[at++]='_';at+=Decimal(path+at,base);const char* ext=".pal";for(unsigned i=0;i<5;++i)path[at+i]=ext[i];if(client::ReadResource(path,rgba,1024))return true;return ReadOriginalBase(original,family,base,rgba);}
 return EmbeddedPalette(body,context,job,sex,bodyOverride,bodyStyle,raceToken,hair,rgba)||ReadOriginalBase(original,family,0,rgba);
}
Content* Intern(const u8* rgba,bool local=false){
 u32 hash=2166136261u;for(unsigned i=0;i<1024;++i)hash=(hash^rgba[i])*16777619u;
 const unsigned bucket=hash&(Buckets-1);for(Content* p=contents[bucket];p;p=p->next)if(p->local==local&&p->hash==hash&&Equal(p->rgba,rgba,1024))return p;
 if(contentCount>=MaxContents||nextHigh>=0x40000){capacityFull=true;return nullptr;}
 auto p=static_cast<Content*>(Alloc(sizeof(Content)));if(!p){capacityFull=true;return nullptr;}p->local=local;p->hash=hash;p->low=nextLow;p->high=nextHigh;if(!++nextLow)++nextHigh;Copy(p->rgba,rgba,1024);p->next=contents[bucket];contents[bucket]=p;const unsigned idBucket=p->low&(Buckets-1);p->identityNext=identities[idBucket];identities[idBucket]=p;++contentCount;return p;
}
void VirtualName(const Content& content,char* out){const char* digits="0123456789abcdefghijklmnopqrstuv";u32 low=content.low,high=content.high;out[0]='~';for(int i=10;i>0;--i){out[i]=digits[low&31];low=(low>>5)|(high<<27);high>>=5;}out[11]='.';out[12]='p';out[13]='a';out[14]='l';out[15]=0;}
bool Identity(const char* path,u32& low,u32& high){unsigned n=Length(path);if(Starts(path,n,"data\\palette\\")){path+=13;n-=13;}else if(Starts(path,n,"palette\\")){path+=8;n-=8;}if(n!=15||path[0]!='~'||!Equal(path+11,".pal",4))return false;low=high=0;for(unsigned i=1;i<11;++i){unsigned digit=static_cast<unsigned>(path[i]-'0');if(digit>9){digit=static_cast<unsigned>(path[i]-'a'+10);if(digit<10||digit>31)return false;}high=(high<<5)|(low>>27);low=(low<<5)|digit;}return low||high;}
Content* Resolve(client::NativeString* name,const Family& family,StoredRecipe& stored,bool body,void* context,int job,int sex,int bodyOverride,int bodyStyle,int raceToken,int hair){
 const unsigned base=body?stored.recipe.bodyBase:stored.recipe.headBase;
 for(Binding* b=stored.bindings;b;b=b->next)if(b->family==family.hash&&b->source==family.source&&b->base==base&&b->body==static_cast<u8>(body))return b->content;
 ns::Material material;ns::BodyRemap remap;const bool reviewed=LoadMaterial(family,body,material,remap);u8 rgba[1024];
 if(!LoadBase(*name,family,base,reviewed?&material:nullptr,reviewed?&remap:nullptr,body,context,job,sex,bodyOverride,bodyStyle,raceToken,hair,rgba))return nullptr;
 if(reviewed)ns::Recolor(rgba,material,stored.recipe,rgba);
 Content* content=Intern(rgba,localPalettePreview&&stored.recipe.id==ns::CharacterPreview);if(!content)return nullptr;
 if(bindingCount<MaxBindings){auto b=static_cast<Binding*>(Alloc(sizeof(Binding)));if(b){b->family=family.hash;b->source=family.source;b->base=static_cast<u16>(base);b->body=static_cast<u8>(body);b->reserved=0;b->content=content;b->next=stored.bindings;stored.bindings=b;++bindingCount;}}
 return content;
}
void ClearBindings(StoredRecipe& stored){Binding* b=stored.bindings;while(b){Binding* next=b->next;Free(b,sizeof(Binding));--bindingCount;b=next;}stored.bindings=nullptr;}
__declspec(naked) bool __fastcall OriginalLoad(void*,void*,const char*){__asm{push ebp}__asm{mov ebp,esp}__asm{push -1}__asm{push 00725B65h}__asm{ret}}
using OriginalLoadFunction=bool(__fastcall*)(void*,void*,const char*);
OriginalLoadFunction volatile originalLoadFunction=OriginalLoad;
bool QueryFamily(client::NativeString& name,bool body,unsigned base,void* context,int job,int sex,int bodyOverride,int bodyStyle,int raceToken,int hair,ns::u64& groups,u32& hash,u32* rgb,bool preserveSkin=false){
 Family family;groups=0;hash=0;if(!FamilyOf(name,ns::FirstRecipe,body,family))return false;if(body?!BodySource(family,context,job,sex,bodyOverride,bodyStyle,raceToken):!HairSource(family,context,job,sex,hair))return false;hash=body?family.source:family.material;
 ns::Material material;ns::BodyRemap remap;if(!LoadMaterial(family,body,material,remap))return false;u8 rgba[1024];if(!LoadBase(name,family,base,&material,&remap,body,context,job,sex,bodyOverride,bodyStyle,raceToken,hair,rgba))return false;
 groups=ns::MaterialGroups(material);for(unsigned g=0;g<ns::GroupCount;++g)if(ns::GroupEnabled(groups,g)){const int anchor=ns::ReferenceIndex(material,g);if(anchor>=0&&(!body||g!=1||!preserveSkin))rgb[g]=(static_cast<u32>(rgba[static_cast<unsigned>(anchor)*4])<<16)|(static_cast<u32>(rgba[static_cast<unsigned>(anchor)*4+1])<<8)|rgba[static_cast<unsigned>(anchor)*4+2];}return true;
}
}
bool client::ReadResource(const char* path,void* out,unsigned size){
 u8 file[64]={};F(file,0)=0xFD3074;F(file,4)=0xffffffff;F(file,0x28)=15;
 const bool opened=N<bool(__thiscall*)(void*,const char*,int,int,int)>(0x573750)(file,path,0,1,0);
 const bool ok=opened&&N<bool(__thiscall*)(void*,void*,unsigned)>(0x573980)(file,out,size);
 N<void(__thiscall*)(void*)>(0x573060)(file);N<void(__thiscall*)(void*)>(0x4F08F0)(file+0x14);return ok;
}
void client::PaletteRecipe(const ns::Recipe& recipe){
 if(!ns::Valid(recipe)||recipe.domain!=ns::Character||!ns::CharacterID(recipe.id))return;
 Lock();StoredRecipe*& stored=recipes[recipe.id-ns::FirstRecipe];if(!stored){stored=static_cast<StoredRecipe*>(Alloc(sizeof(StoredRecipe)));if(stored)stored->bindings=nullptr;}
 if(stored){if(stored->bindings&&!Equal(&stored->recipe,&recipe,sizeof(recipe)))ClearBindings(*stored);Copy(&stored->recipe,&recipe,sizeof(recipe));}Unlock();
}
void client::LocalPalettePreview(bool active){localPalettePreview=active;}
void client::ReleaseLocalPalettes(){Lock();if(recipes[ns::CharacterPreview-ns::FirstRecipe])ClearBindings(*recipes[ns::CharacterPreview-ns::FirstRecipe]);for(unsigned i=0;i<Buckets;++i){Content** link=&contents[i];while(*link){Content* p=*link;if(!p->local){link=&p->next;continue;}*link=p->next;Content** id=&identities[p->low&(Buckets-1)];while(*id&&*id!=p)id=&(*id)->identityNext;if(*id)*id=p->identityNext;Free(p,sizeof(Content));--contentCount;}}Unlock();}
bool client::PaletteCacheFull(){Lock();const bool full=capacityFull;Unlock();return full;}
bool client::SelectRecipe(const ns::RecipePacket& p,unsigned bytes){
 if(bytes!=sizeof(p)||p.opcode!=0xD54||p.length!=sizeof(p)||p.version!=ns::Version||p.reserved[0]>1||p.reserved[1]||p.reserved[2]||!ns::Valid(p.recipe))return false;
 if(!p.recipe.id&&(!p.reserved[0]||p.recipe.flags))return false;
 if(p.reserved[0]){if(p.recipe.id||p.recipe.flags||p.recipe.domain!=ns::Character)return false;PaletteResetSession();CostumeResetSession();}if(p.recipe.id){if(p.recipe.domain==ns::Character)PaletteRecipe(p.recipe);else CostumeRecipe(p.recipe);}return true;
}
void client::PaletteResetSession(){Lock();for(unsigned i=0;i<ns::CharacterCount;++i)if(recipes[i]){ClearBindings(*recipes[i]);Free(recipes[i],sizeof(StoredRecipe));recipes[i]=nullptr;}Unlock();}
void client::RewriteCharacterPalette(NativeString* name,unsigned recipeId,bool body,void* context,int job,int sex,int bodyOverride,int bodyStyle,int hair,int raceToken){
 const bool ordinary=body&&recipeId>0&&recipeId<ns::FirstRecipe;if(!name||(!ns::CharacterID(recipeId)&&!ordinary))return;Family family;unsigned actualBase=recipeId;
 if(!FamilyOf(*name,actualBase,body,family)){if(!ordinary||recipeId!=1||!FamilyOf(*name,0,body,family))return;actualBase=0;}
 const bool sourceReady=body?BodySource(family,context,job,sex,bodyOverride,bodyStyle,raceToken):HairSource(family,context,job,sex,hair);
 if(ordinary){
  ns::Material material;ns::BodyRemap remap;u8 rgba[1024];if(!sourceReady||!LoadMaterial(family,true,material,remap)||!remap.count||!ReadOriginalBase(*name,family,actualBase,rgba))return;
  ns::RemapBody(rgba,remap);Lock();Content* content=Intern(rgba);if(content){char virtualName[16];VirtualName(*content,virtualName);Assign(name,virtualName,15);}Unlock();return;
 }
 Lock();StoredRecipe* stored=recipes[recipeId-ns::FirstRecipe];StoredRecipe fallback={};fallback.recipe.id=static_cast<u16>(recipeId);
 Content* content=sourceReady?Resolve(name,family,stored?*stored:fallback,body,context,job,sex,bodyOverride,bodyStyle,raceToken,hair):nullptr;if(!stored)ClearBindings(fallback);
 if(content){char virtualName[16];VirtualName(*content,virtualName);Assign(name,virtualName,15);}
 else {char path[PathLimit];unsigned size=0;const unsigned base=stored?(body?stored->recipe.bodyBase:stored->recipe.headBase):0;if(BasePath(*name,family,base,path,&size))Assign(name,path,size);}
 Unlock();
}
bool client::CharacterMaterials(void* actor,u16 hair,u16 headBase,u16 bodyBase,CharacterMaterialInfo* out){
 if(!out)return false;*out={};if(!actor||headBase>=ns::FirstRecipe||bodyBase>=ns::FirstRecipe)return false;
 int job=N<int(__thiscall*)(void*,int)>(0xC43230)(actor,-1);const int sex=static_cast<u8*>(actor)[0x260];if(static_cast<u8*>(actor)[0x2CC])job=N<int(__thiscall*)(void*,int)>(0xD83D60)(reinterpret_cast<void*>(0x15FA3C0),job);
 const int bodyOverride=static_cast<int>(F(actor,0x4C8));const int variant=N<int(__thiscall*)(void*)>(F(reinterpret_cast<void*>(F(actor,0)),0xDC))(actor);void* context=*reinterpret_cast<void**>(0x15C57C8);
 NativeString head,body;Init(head);Init(body);OriginalHairPalette(context,&head,hair,job,sex,ns::FirstRecipe);OriginalBodyPalette(context,&body,job,sex,ns::FirstRecipe,bodyOverride,variant,0);
 out->headReady=QueryFamily(head,false,headBase,context,job,sex,bodyOverride,variant,static_cast<int>(F(actor,0x4CC))+1,hair,out->headGroups,out->headFamily,out->sourceRGB);out->bodyReady=QueryFamily(body,true,bodyBase,context,job,sex,bodyOverride,variant,static_cast<int>(F(actor,0x4CC))+1,hair,out->bodyGroups,out->bodyFamily,out->sourceRGB,ns::GroupEnabled(out->headGroups,1));
 out->capabilities=out->headGroups|out->bodyGroups;Destroy(head);Destroy(body);return true;
}
EXPORT bool __fastcall PaletteLoad(void* self,void*,const char* path){
 u32 low=0,high=0;if(!path||!Identity(path,low,high))return originalLoadFunction(self,nullptr,path);
 Lock();const Content* found=nullptr;for(Content* c=identities[low&(Buckets-1)];c;c=c->identityNext)if(c->low==low&&c->high==high){found=c;break;}
 if(found)Copy(static_cast<u8*>(self)+0x110,found->rgba,1024);Unlock();
 if(!found)return originalLoadFunction(self,nullptr,path);
 N<void(__cdecl*)(void*,const void*,int)>(0x566770)(static_cast<u8*>(self)+0x510,static_cast<u8*>(self)+0x110,256);return true;
}
