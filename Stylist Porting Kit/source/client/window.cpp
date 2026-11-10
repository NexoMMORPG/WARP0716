#include "native.hpp"
#include "palette_hooks.hpp"
#include "costume.hpp"
#include "material_labels.hpp"
#include "picker.hpp"
extern "C" { __declspec(dllexport) volatile u32 NativeHairLimits[2]={65535,65535}; }
namespace {
using Void=void(__thiscall*)(void*);using Add=void(__thiscall*)(void*,void*);using Two=void(__thiscall*)(void*,int,int);
void* frame;u32 table[55];bool ready,pending,rendered,drawing,previewBusy,discardPreview,closeDeferred,closeNotify;ns::State state{};ns::Recipe chosen{},characterChosen{};ns::ItemsPacket equipped{};
client::CharacterMaterialInfo materials{};__declspec(align(16)) u8 renderer[0x98];int page,part,rotation,zoom,materialPage,px=-1,py=-1,pressed=-1,slot,selectedItem;
bool pickerOpen,pickerDirty;int hoverPart=-1,pickerDrag,pickerField;unsigned pickerHue,pickerSat,pickerValue,pickerRGB,pickerApplied,pickerStamp;
bool editing,lastCacheFull,highlightArea;int lastCostumeStatus=-1;char hex[7];unsigned hexLength;const char* notice="";u32 nativeCompose;
const char* pages[]={"Hair and skin","Outfit","Costumes","Looks"};
ns::u64 Available();
void Number(char*,unsigned);
struct HairCatalog {u32 magic;u16 count,reserved;u16 ids[512];};HairCatalog hairs;
void* Actor(){void* mode=N<void*(__thiscall*)(void*)>(0xA75340)(reinterpret_cast<void*>(0x1213338));void* world=mode?reinterpret_cast<void*>(F(mode,0xCC)):nullptr;return world?reinterpret_cast<void*>(F(world,0x2C)):nullptr;}
struct ActorContext {void* actor;int job,style;u32 original,body,options;u8 sex,transformed;};
ActorContext actorContext{};bool contextReady;
constexpr int PartWidth=108,PartHeight=20,PartColumnStep=112,PartRowStep=22;
unsigned PartsPerPage(){return page==2?18u:24u;}
int PartX(unsigned i){return 512+static_cast<int>(i%3)*PartColumnStep;}
int PartY(unsigned i){return (page==2?124:88)+static_cast<int>(i/3)*PartRowStep;}
const char* PartName(unsigned group){if(page!=2)return ns_labels::Outfit(materials.bodyFamily,group);static char name[24];const char* prefix="Color area ";unsigned i=0;while(prefix[i]){name[i]=prefix[i];++i;}unsigned number=0;auto visible=Available()|chosen.flags;for(unsigned g=0;g<=group;++g)if(ns::GroupEnabled(visible,g))++number;Number(name+i,number);return name;}
const char* PartButtonLabel(const char* label,char* compact){unsigned n=0;while(label[n])++n;if(n<=17)return label;for(unsigned i=0;i<14;++i)compact[i]=label[i];compact[14]=compact[15]=compact[16]='.';compact[17]=0;return compact;}
bool ReadContext(ActorContext& value){
 value={};value.actor=Actor();if(!value.actor)return false;void* actor=value.actor;
 value.job=N<int(__thiscall*)(void*,int)>(0xC43230)(actor,-1);value.original=F(actor,0x4CC);value.body=F(actor,0x4C8);value.sex=static_cast<u8*>(actor)[0x260];value.transformed=static_cast<u8*>(actor)[0x2CC];
 value.style=N<int(__thiscall*)(void*)>(F(reinterpret_cast<void*>(F(actor,0)),0xDC))(actor);
 // The native self-only ZC_STATE_CHANGE branch stores its full option DWORD here.
 value.options=*reinterpret_cast<const u32*>(0x15FB2B4);return true;
}
bool SameContext(const ActorContext& a,const ActorContext& c){return a.actor==c.actor&&a.job==c.job&&a.original==c.original&&a.body==c.body&&a.style==c.style&&a.sex==c.sex&&a.transformed==c.transformed&&a.options==c.options;}
bool ObserveActor(){
 if(!frame||closeDeferred)return false;ActorContext current{};
 if(!ReadContext(current)||(contextReady&&!SameContext(actorContext,current))){client::Close(true);return false;}
 if(!contextReady){actorContext=current;contextReady=true;}return true;
}
void* Manager(){return reinterpret_cast<void*>(0x131F4E8);}
void Dirty(){if(frame)N<Void>(0xA23340)(frame);}
bool PickerInputValid();void PickerPaint(void*);void PickerFlush(bool);void PickerCancel();void PickerPoint(int,int);
unsigned Clock(){return (*reinterpret_cast<unsigned(__stdcall**)()>(0xfc120c))();}
bool LeftHeld(){return ((*reinterpret_cast<short(__stdcall**)(int)>(0xfc1658))(1)&0x8000)!=0;}
void DestroyPreview(){if(previewBusy){discardPreview=true;return;}if(rendered){N<Void>(0x79A6A0)(renderer);rendered=false;client::ReleaseLocalPalettes();client::ReleaseLocalCostumes();}}
bool Hit(int x,int y,int l,int t,int w,int h){return x>=l&&x<l+w&&y>=t&&y<t+h;}
void Copy(char* d,const char* s){while((*d++=*s++)){} }
void Number(char* out,unsigned value){char reverse[12];unsigned n=0;do{reverse[n++]=static_cast<char>('0'+value%10);value/=10;}while(value);while(n)*out++=reverse[--n];*out=0;}
void Text(void* self,int x,int y,const char* s,u32 color=0x3d507d,int width=0,int size=12){N<void(__thiscall*)(void*,int,int,const char*,int,int,int,u32,int)>(0xA25ED0)(self,x,y,s,width,0,size,((color&255u)<<16)|(color&0xff00u)|((color>>16)&255u),0);}
void Bitmap(void* self,int x,int y,const char* name){char path[128];Copy(path,"native_stylist\\");Copy(path+15,name);for(unsigned i=0;path[i];++i)if(path[i]=='/')path[i]='\\';void* bmp=N<void*(__thiscall*)(void*,const char*)>(0xA8D4A0)(N<void*(__cdecl*)()>(0xA90350)(),path);if(bmp)N<void(__thiscall*)(void*,int,int,void*,bool)>(0xA1D260)(self,x,y,bmp,true);}
void Button(void* self,int id,int x,int y,int w,int h,const char* label,const char* asset="button",bool enabled=true,bool selected=false){
 char path[64];Copy(path,asset);unsigned n=0;while(path[n])++n;Copy(path+n,!enabled?"_disabled.bmp":selected?"_selected.bmp":pressed==id&&Hit(px,py,x,y,w,h)?"_press.bmp":Hit(px,py,x,y,w,h)?"_over.bmp":"_normal.bmp");Bitmap(self,x,y,path);Text(self,x,y+(h-12)/2,label,enabled?0x3d507d:0x8993a8,w);
}
void ToHex(char* out,u32 rgb){const char* digits="0123456789ABCDEF";for(int i=5;i>=0;--i){out[i]=digits[rgb&15];rgb>>=4;}out[6]=0;}
u32 Swatch(int index){
 if(index<13){unsigned v=255u*index/12;return v*0x010101u;}int row=index/13-1,col=index%13;unsigned h=static_cast<unsigned>(col)*1536/13,sector=h/256,t=h%256,v=static_cast<unsigned>(row<3?255:240-(row-3)*42);
 unsigned s=static_cast<unsigned>(row<3?80+row*80:230),lo=v*(255-s)/255,up=lo+(v-lo)*t/255,down=v-(v-lo)*t/255,r=0,g=0,b=0;
 switch(sector){case 0:r=v;g=up;b=lo;break;case 1:r=down;g=v;b=lo;break;case 2:r=lo;g=v;b=up;break;case 3:r=lo;g=down;b=v;break;case 4:r=up;g=lo;b=v;break;default:r=v;g=lo;b=down;}return (r<<16)|(g<<8)|b;
}
int CurrentItem(){for(unsigned i=0;i<equipped.count;++i)if(equipped.items[i].index==state.target&&equipped.items[i].instance==state.instance)return static_cast<int>(i);return -1;}
ns::u64 Available(){
 if(page==2)return state.target&&CurrentItem()>=0&&client::CostumeRecipeReady(chosen.id)?state.capabilities&ns::CostumeGroups&client::CostumeCapabilities(chosen.id):0;
 if(state.target)return 0;
 ns::u64 result=materials.capabilities&state.capabilities;
 if(!ns::GroupEnabled(materials.headGroups,1)||(!ns::GroupEnabled(materials.bodyGroups,1)&&!(materials.bodyReady&&ns_labels::BodySkinCovered(materials.bodyFamily))))result&=~ns::GroupFlag(1);
 return result;
}
unsigned PartList(unsigned* out){unsigned count=0;auto available=Available()|chosen.flags;for(unsigned g=page==1?2:0;g<(page==2?13:ns::GroupCount);++g)if(ns::GroupEnabled(available,g))out[count++]=g;return count;}
bool CanConfirm(){if(!chosen.id||client::PaletteCacheFull())return false;if(page==2)return state.target&&CurrentItem()>=0&&client::CostumeRecipeReady(chosen.id)&&(!chosen.flags||client::CostumeRenderStatus(chosen.id)==1);return !state.target&&!(chosen.flags&~Available());}
void RefreshMaterials(){
 materials={};const auto& colors=state.target?characterChosen:chosen;client::CharacterMaterials(Actor(),state.hair,colors.headBase,colors.bodyBase,&materials);
 unsigned list[ns::GroupCount];unsigned count=PartList(list);if((page==1||page==2)&&count){bool present=false;for(unsigned i=0;i<count;++i)if(list[i]==static_cast<unsigned>(part))present=true;if(!present)part=static_cast<int>(list[0]);if(static_cast<unsigned>(materialPage)*PartsPerPage()>=count)materialPage=0;}
}
u32 CurrentRGB(){return ns::GroupEnabled(chosen.flags,static_cast<unsigned>(part))||state.target?chosen.rgb[part]:materials.sourceRGB[part];}
bool Send(unsigned action,int item=-1){
 if(!state.session||(pending&&action!=1))return false;if((action==2||action==3||action>=6)&&highlightArea){highlightArea=false;DestroyPreview();}ns::Request r{};r.opcode=0xD50;r.version=ns::Version;r.action=static_cast<u8>(action);r.session=state.session;r.revision=state.revision;r.hair=state.hair;r.slot=static_cast<u16>(slot);r.target=state.target;r.instance=state.instance;r.recipe=chosen;
 if(action==7){if(item<0||static_cast<unsigned>(item)>=equipped.count)return false;r.target=equipped.items[item].index;r.instance=equipped.items[item].instance;selectedItem=item;}
 if(action==8){r.target=0;r.instance=0;}
 bool ok=N<bool(__thiscall*)(void*,int,void*)>(0xC14920)(N<void*(__cdecl*)()>(0xC14D60)(),sizeof(r),&r);
 if(action!=1){pending=ok;notice=ok?"Updating preview...":"Unable to send. Please try again.";}Dirty();return ok;
}
void Preview(void* self){
 if(!ObserveActor())return;void* actor=actorContext.actor;previewBusy=true;
 const bool local=pickerOpen||(!pending&&hoverPart>=0);ns::Recipe draft=chosen;
 if(pickerOpen){draft.flags|=ns::GroupFlag(static_cast<unsigned>(part));draft.rgb[part]=pickerApplied;}
 else if(local){draft.flags|=ns::GroupFlag(static_cast<unsigned>(hoverPart));draft.rgb[hoverPart]=0xff40ff;}
 client::LocalPalettePreview(local);client::LocalCostumePreview(local);
 if(local){if(state.target)client::CostumeRecipe(draft);else client::PaletteRecipe(draft);}
 client::CostumePreviewHighlight(page==2&&!pickerOpen?(hoverPart>=0?chosen.id:highlightArea?chosen.id:0):0,hoverPart>=0?hoverPart:part);
 if(!rendered){
  int job=N<int(__thiscall*)(void*,int)>(0xC43230)(actor,-1);int variant=N<int(__thiscall*)(void*)>(F(reinterpret_cast<void*>(F(actor,0)),0xDC))(actor);
  const auto& colors=state.target?characterChosen:(local?draft:chosen);unsigned head=colors.flags&3?colors.id:colors.headBase,body=colors.flags&~1ull?colors.id:colors.bodyBase;
  unsigned first=F(actor,0x450),second=F(actor,0x44C),third=F(actor,0x448),robe=F(actor,0x494);
  // CPc RTTI vtable and constructor arguments: mid, top, bottom, robe.
  int item=CurrentItem();if(state.target&&item>=0&&client::CostumeRecipeReady(chosen.id)){
   unsigned view=chosen.id;
   switch(equipped.items[item].slot){case 0:second=view;break;case 1:first=view;break;case 2:third=view;break;case 3:robe=view;break;}
  }
  using Ctor=void*(__thiscall*)(void*,void*,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,int,u32);
  N<Ctor>(0x7AC210)(renderer,self,340,248,static_cast<u8*>(actor)[0x260],job,F(actor,0x4CC),F(actor,0x4C8),state.hair,first,second,third,robe,static_cast<u8*>(actor)[0x2CC],variant,body,head,rotation,0,0);rendered=true;
 }
 F(renderer,0x38)=static_cast<u32>(rotation);renderer[0x45]=N<u8(__thiscall*)(void*)>(0x7AD9E0)(renderer);drawing=true;N<void(__thiscall*)(void*,int)>(0x7AC820)(renderer,1);drawing=false;client::CostumePreviewHighlight(0,-1);client::LocalPalettePreview(false);client::LocalCostumePreview(false);if(local){if(state.target)client::CostumeRecipe(chosen);else client::PaletteRecipe(chosen);}previewBusy=false;
 if(closeDeferred){const bool notify=closeNotify;closeDeferred=closeNotify=false;client::Close(notify);return;}
 if(discardPreview){discardPreview=false;DestroyPreview();Dirty();return;}
 if(!ObserveActor())return;
 int status=state.target?client::CostumeRenderStatus(chosen.id):0;bool full=client::PaletteCacheFull();if(status!=lastCostumeStatus||full!=lastCacheFull){lastCostumeStatus=status;lastCacheFull=full;if(page==2)RefreshMaterials();if(status==2)notice="This costume could not be recolored. Original appearance preserved.";if(full)notice="Client color cache is full. Restart the game.";Dirty();}
}
int At(int x,int y){
 if(pickerOpen){if(Hit(x,y,190,340,200,150))return 500;if(Hit(x,y,406,340,20,150))return 501;if(PickerInputValid()&&Hit(x,y,696,493,144,36))return 502;if(Hit(x,y,536,493,144,36))return 503;if(Hit(x,y,450,366,144,28))return 510;for(int i=0;i<3;++i)if(Hit(x,y,450+i*70,416,64,28))return 511+i;return -1;}
 if(Hit(x,y,848,0,24,17))return 1;for(int i=0;i<4;++i)if(Hit(x,y,12,60+i*42,144,36))return 10+i;
 if(Hit(x,y,184,274,40,28))return 20;if(Hit(x,y,412,274,40,28))return 21;if(Hit(x,y,260,274,40,28))return 22;if(Hit(x,y,336,274,40,28))return 23;
 if(pending)return -1;if(CanConfirm()&&Hit(x,y,732,552,128,32))return 30;if(Hit(x,y,592,552,128,32))return 31;
 if(page==0){if(Hit(x,y,512,88,144,36))return 40;if(Hit(x,y,512,132,144,36))return 41;if(hairs.count){if(Hit(x,y,512,184,40,28))return 24;if(Hit(x,y,696,184,40,28))return 25;}}
 if(page==2&&Available()&&Hit(x,y,512,240,144,24))return 45;
 if(page==1||page==2){unsigned list[ns::GroupCount],count=PartList(list),first=static_cast<unsigned>(materialPage)*PartsPerPage();for(unsigned i=0;i<PartsPerPage()&&first+i<count;++i){if(Hit(x,y,PartX(i),PartY(i),PartWidth,PartHeight))return 400+static_cast<int>(list[first+i]);}
  if(Hit(x,y,512,274,40,28))return 26;if(Hit(x,y,808,274,40,28))return 27;if(page==2&&equipped.count){if(Hit(x,y,512,88,40,28))return 28;if(Hit(x,y,808,88,40,28))return 29;}
 }
 if(page<3){if(Hit(x,y,696,493,144,36))return 42;if(ns::GroupEnabled(Available(),static_cast<unsigned>(part))&&Hit(x,y,184,340,507,144))return 100+(y-340)/18*13+(x-184)/39;if(Hit(x,y,696,340,144,36))return 43;if(Hit(x,y,696,386,144,36))return 44;}
 if(page==3){for(int i=0;i<8;++i)if(Hit(x,y,184+(i%4)*160,344+(i/4)*64,144,36))return 300+i;if(Hit(x,y,184,472,144,36))return 310;}
 return -1;
}
unsigned AreaRGB(unsigned group){if(pickerOpen&&group==static_cast<unsigned>(part))return pickerRGB;if(ns::GroupEnabled(chosen.flags,group))return chosen.rgb[group];return page==2?client::CostumeSourceRGB(chosen.id,group):materials.sourceRGB[group];}
void AreaSwatch(void* self,int x,int y,unsigned rgb){const char* digits="0123456789abcdef";char path[]="chips/c000.bmp";path[7]=digits[(((rgb>>16)&255)+8)/17];path[8]=digits[(((rgb>>8)&255)+8)/17];path[9]=digits[((rgb&255)+8)/17];Bitmap(self,x,y,path);}
void PaintParts(void* self){
 if(page==0){Button(self,40,512,88,144,36,"Hair","tab",!pending&&ns::GroupEnabled(Available()|chosen.flags,0),part==0);Button(self,41,512,132,144,36,"Skin","tab",!pending&&ns::GroupEnabled(Available()|chosen.flags,1),part==1);Button(self,24,512,184,40,28,"<","small",hairs.count&&!pending);Button(self,25,696,184,40,28,">","small",hairs.count&&!pending);char label[32]="Hair style ";Number(label+11,state.hair);Text(self,552,192,label,0x3d507d,144);return;}
 if(page!=1&&page!=2)return;
 if(page==2){Button(self,28,512,88,40,28,"<","small",equipped.count&&!pending);Button(self,29,808,88,40,28,">","small",equipped.count&&!pending);int item=CurrentItem();Text(self,552,96,item>=0?equipped.items[item].name:equipped.count?"Select a costume":"Equip a costume to recolor",0x3d507d,256);}
 unsigned list[ns::GroupCount],count=PartList(list),first=static_cast<unsigned>(materialPage)*PartsPerPage();for(unsigned i=0;i<PartsPerPage()&&first+i<count;++i){unsigned group=list[first+i];char compact[18];const char* label=PartButtonLabel(PartName(group),compact);Button(self,400+static_cast<int>(group),PartX(i),PartY(i),PartWidth,PartHeight,"","material",!pending,part==static_cast<int>(group));AreaSwatch(self,PartX(i)+4,PartY(i)+4,AreaRGB(group));Text(self,PartX(i)+18,PartY(i)+4,label,pending?0x8993a8:0x3d507d,90);}
 if(page==2)Button(self,45,512,240,144,24,"Highlight area","area",!pending&&Available(),highlightArea);
 Button(self,26,512,274,40,28,"<","small",!pending&&materialPage>0);Button(self,27,808,274,40,28,">","small",!pending&&static_cast<unsigned>(materialPage+1)*PartsPerPage()<count);
 char label[64]="Page ";Number(label+5,static_cast<unsigned>(materialPage+1));unsigned end=0;while(label[end])++end;Copy(label+end," / ");Number(label+end+3,count?(count+PartsPerPage()-1)/PartsPerPage():1);while(label[end])++end;if(count){Copy(label+end,"  ");int hover=At(px,py);unsigned group=hover>=400&&hover<400+static_cast<int>(ns::GroupCount)?static_cast<unsigned>(hover-400):static_cast<unsigned>(part);Copy(label+end+2,PartName(group));if(page==1||page==2){while(label[end])++end;Copy(label+end,"  #");ToHex(label+end+3,AreaRGB(group));}}Text(self,552,282,label,0x63769c,256);
 if(!count)Text(self,512,page==2?148:132,page==2?(CurrentItem()<0?"Select an equipped costume.":client::CostumeRenderStatus(chosen.id)==0?"Loading costume colors...":client::CostumeRenderStatus(chosen.id)==2?"Original appearance preserved.":"No editable colors in this costume."):"No reviewed outfit profile for this body.",0x7484a5);
}
void PaintBody(void* self){
 Bitmap(self,0,0,"window.bmp");Text(self,20,3,"Native Stylist",0x182c57,0,12);for(int i=0;i<4;++i)Button(self,10+i,12,60+i*42,144,36,pages[i],"sidebar",!pending,page==i);Button(self,1,848,0,24,17,"","close");
 Text(self,190,58,"Appearance preview",0x3153a6);Text(self,512,58,page==2?"Costume and colors":"Parts",0x3153a6);Button(self,20,184,274,40,28,"<","small");Button(self,21,412,274,40,28,">","small");Button(self,22,260,274,40,28,"-","small",zoom>0);Button(self,23,336,274,40,28,"+","small",zoom<6);
 const char* scales[]={"100%","150%","200%","300%","400%","600%","800%"};Text(self,300,282,scales[zoom],0x3d507d,36);PaintParts(self);
 if(page<3){
  Text(self,184,316,"Choose a dye or enter an RGB color",0x3153a6);Bitmap(self,184,340,"dyes.bmp");int index=At(px,py);if(index>=100&&index<204)Bitmap(self,184+(index-100)%13*39,340+(index-100)/13*18,pressed==index?"dye_press.bmp":"dye_over.bmp");
  char color[7];ToHex(color,CurrentRGB());Text(self,184,498,"RGB",0x7484a5);bool available=ns::GroupEnabled(Available(),static_cast<unsigned>(part));Button(self,42,696,493,144,36,editing?hex:ns::GroupEnabled(chosen.flags,static_cast<unsigned>(part))?color:"Original","tab",!pending&&available,editing);Button(self,43,696,340,144,36,"Original","tab",!pending&&(available||ns::GroupEnabled(chosen.flags,static_cast<unsigned>(part))));Button(self,44,696,386,144,36,"Color picker","tab",!pending&&available);
  Text(self,184,522,available?(page==2?"Colors belong to this costume item.":"Only the selected material group changes."):(part==1?"Skin colors are unavailable for this body.":"This color is unavailable for this appearance."),0x7484a5);
 }else{
  Text(self,184,316,"Saved Looks",0x3153a6);const char* names[]={"Look 1","Look 2","Look 3","Look 4","Look 5","Look 6","Look 7","Look 8"};for(int i=0;i<8;++i)Button(self,300+i,184+(i%4)*160,344+(i/4)*64,144,36,names[i],"tab",!pending,slot==i);Button(self,310,184,472,144,36,"Save selected slot","tab",!pending);
 }
 Text(self,184,560,notice,0x63769c,390,11);Button(self,31,592,552,128,32,"Reset","button",!pending);Button(self,30,732,552,128,32,"Confirm all","confirm",!pending&&CanConfirm());if(pickerOpen)PickerPaint(self);
}
void Choose(){editing=false;chosen.flags|=ns::GroupFlag(static_cast<unsigned>(part));Send(2);}

bool PickerInputValid(){if(!hexLength)return true;if(pickerField==0)return hexLength==6;unsigned value=0;for(unsigned i=0;i<hexLength;++i)value=value*10+static_cast<unsigned>(hex[i]-'0');return value<=255;}
unsigned Channel(unsigned rgb,int c){return (rgb>>(16-8*c))&255;}
void FieldText(){if(pickerField==0){ToHex(hex,pickerRGB);}else Number(hex,Channel(pickerRGB,pickerField-1));hexLength=0;}
void PickerOpen(){pickerOpen=editing=true;pickerField=pickerDrag=0;hoverPart=-1;highlightArea=false;pickerRGB=pickerApplied=ns::GroupEnabled(chosen.flags,static_cast<unsigned>(part))?chosen.rgb[part]:state.target?client::CostumeSourceRGB(chosen.id,static_cast<unsigned>(part)):materials.sourceRGB[part];picker::HSV(pickerRGB,pickerHue,pickerSat,pickerValue);pickerStamp=Clock();pickerDirty=false;FieldText();DestroyPreview();Dirty();}
void PickerCancel(){pickerOpen=editing=pickerDirty=false;pickerDrag=0;DestroyPreview();Dirty();}
void PickerFlush(bool force){if(!pickerDirty)return;unsigned now=Clock();if(!force&&now-pickerStamp<33)return;pickerStamp=now;pickerDirty=false;bool change=pickerApplied!=pickerRGB;pickerApplied=pickerRGB;if(change)DestroyPreview();Dirty();}
void PickerPoint(int x,int y){unsigned before=pickerRGB,oldHue=pickerHue,oldSat=pickerSat,oldValue=pickerValue;
 if(pickerDrag==1){x=x<190?190:x>389?389:x;y=y<340?340:y>489?489:y;pickerSat=static_cast<unsigned>(x-190)*255/199;pickerValue=255-static_cast<unsigned>(y-340)*255/149;}
 if(pickerDrag==2){y=y<340?340:y>489?489:y;pickerHue=static_cast<unsigned>(y-340);}
 pickerRGB=picker::RGB(pickerHue,pickerSat,pickerValue);if(before!=pickerRGB||oldHue!=pickerHue||oldSat!=pickerSat||oldValue!=pickerValue){pickerDirty=true;FieldText();}
}
void PickerPaint(void* self){Bitmap(self,174,306,"picker/panel.bmp");Text(self,190,316,"Choose a color",0x3d507d);
 char plane[]="picker/h000.bmp";plane[8]=static_cast<char>('0'+pickerHue/100);plane[9]=static_cast<char>('0'+pickerHue/10%10);plane[10]=static_cast<char>('0'+pickerHue%10);Bitmap(self,190,340,plane);Bitmap(self,406,340,"picker/hue.bmp");
 Bitmap(self,190+static_cast<int>(pickerSat)*199/255-4,340+(255-static_cast<int>(pickerValue))*149/255-4,"picker/point.bmp");Bitmap(self,402,340+static_cast<int>(pickerHue)-3,"picker/hue_point.bmp");
 Text(self,450,344,"HEX",0x3153a6);Button(self,510,450,366,144,28,"","picker/hex",true,pickerField==0);char value[8];ToHex(value,pickerRGB);Text(self,450,374,pickerField==0&&hexLength?hex:value,0x3d507d,144);
 const char* labels[]={"R","G","B"};for(int i=0;i<3;++i){Text(self,450+i*70,400,labels[i],0x3153a6);Button(self,511+i,450+i*70,416,64,28,"","picker/channel",true,pickerField==i+1);Number(value,Channel(pickerRGB,i));Text(self,450+i*70,424,pickerField==i+1&&hexLength?hex:value,0x3d507d,64);}
 Text(self,450,460,"Drag to preview. Escape restores the previous color.",0x63769c,390,11);Button(self,503,536,493,144,36,"Cancel","tab");Button(self,502,696,493,144,36,"OK","tab",PickerInputValid());
}
void PickerAccept(){if(!ObserveActor()||!PickerInputValid())return;unsigned rgb=pickerRGB;pickerOpen=editing=pickerDirty=false;pickerDrag=0;DestroyPreview();chosen.rgb[part]=rgb;Choose();}
void HoverArea(){int id=At(px,py);int group=!pending&&!pickerOpen?(id==40?0:id==41?1:id>=400&&id<434?id-400:-1):-1;if(group>=0&&!ns::GroupEnabled(Available(),static_cast<unsigned>(group)))group=-1;if(group!=hoverPart){hoverPart=group;DestroyPreview();Dirty();}}
void Action(int id){
 if(pickerOpen){if(id==502)PickerAccept();else if(id==503)PickerCancel();else if(id>=510&&id<=513){pickerField=id-510;FieldText();Dirty();}return;}
 if(id==1){client::Close(true);return;}if(id==45&&!pending&&page==2&&Available()){highlightArea=!highlightArea;DestroyPreview();Dirty();return;}if(!ObserveActor()||pending)return;
 if(id>=10&&id<14){highlightArea=false;page=id-10;materialPage=0;part=page==1?5:0;editing=false;if(page==2){if(equipped.count)Send(7,selectedItem<static_cast<int>(equipped.count)?selectedItem:0);else Send(8);}else if(state.target)Send(8);RefreshMaterials();Dirty();return;}
 if(id==20||id==21){rotation=(rotation+(id==20?7:1))%8;DestroyPreview();Dirty();return;}if(id==22||id==23){if(id==22&&zoom>0)--zoom;if(id==23&&zoom<6)++zoom;Dirty();return;}
 if((id==24||id==25)&&hairs.count){unsigned i=0;while(i<hairs.count&&hairs.ids[i]!=state.hair)++i;if(i==hairs.count)i=0;i=(i+(id==24?hairs.count-1:1))%hairs.count;state.hair=hairs.ids[i];RefreshMaterials();Send(2);return;}
 if(id==26||id==27){unsigned list[ns::GroupCount],count=PartList(list);if(id==26&&materialPage>0)--materialPage;if(id==27&&static_cast<unsigned>(materialPage+1)*PartsPerPage()<count)++materialPage;Dirty();return;}
 if((id==28||id==29)&&equipped.count){int current=CurrentItem();if(current<0)current=0;current=(current+(id==28?equipped.count-1:1))%equipped.count;materialPage=0;part=0;Send(7,current);return;}
 if(id==40||id==41){part=id-40;editing=false;Dirty();return;}if(id>=400&&id<400+static_cast<int>(ns::GroupCount)){part=id-400;editing=false;if(highlightArea)DestroyPreview();Dirty();return;}
 if(id>=100&&id<204){chosen.rgb[part]=Swatch(id-100);Choose();return;}
 if(id==42&&ns::GroupEnabled(Available(),static_cast<unsigned>(part))){PickerOpen();return;}
 if(id==44&&!editing&&ns::GroupEnabled(Available(),static_cast<unsigned>(part))){PickerOpen();return;}
 if(id==44&&editing&&hexLength==6){u32 rgb=0;for(unsigned i=0;i<6;++i)rgb=(rgb<<4)|(hex[i]<='9'?hex[i]-'0':hex[i]-'A'+10);chosen.rgb[part]=rgb;Choose();return;}
 if(id==43&&((page==2)==(state.target!=0))&&ns::GroupEnabled(Available()|chosen.flags,static_cast<unsigned>(part))){chosen.flags&=~ns::GroupFlag(static_cast<unsigned>(part));Send(2);return;}
 if(id==30&&CanConfirm()){notice="Applying all appearance changes...";Send(3);return;}if(id==31){Send(6);return;}if(id>=300&&id<308){slot=id-300;Send(5);return;}if(id==310){Send(4);return;}
}
void __fastcall Paint(void* self,void*){if(self==frame)PaintBody(self);}
void __fastcall Tick(void* self,void*){if(self==frame&&ObserveActor()){if(pickerOpen){if(pickerDrag&&!LeftHeld()){pickerDrag=0;PickerFlush(true);}else PickerFlush(false);}else HoverArea();}}
bool __fastcall Compose(void* self,void*,int force){if(self==frame&&F(self,0x28)){Preview(self);if(self!=frame)return false;}return N<bool(__thiscall*)(void*,int)>(nativeCompose)(self,force);}
void __fastcall Down(void* self,void*,int x,int y){px=x;py=y;pressed=At(x,y);if(pickerOpen&&(pressed==500||pressed==501)){pickerDrag=pressed==500?1:2;PickerPoint(x,y);PickerFlush(false);return;}if(y<17&&pressed<0)N<Two>(0x880AD0)(self,x,y);else N<Add>(0xA39130)(Manager(),self);Dirty();}
void __fastcall Up(void* self,void*,int x,int y){int old=pressed;pressed=-1;px=x;py=y;if(pickerOpen&&pickerDrag){PickerPoint(x,y);pickerDrag=0;PickerFlush(true);return;}N<Two>(*reinterpret_cast<u32*>(0x102FE08+0x7C))(self,x,y);if(old>=0&&old==At(x,y))Action(old);Dirty();}
void Motion(void* self,int x,int y,u32 native){int before=At(px,py);px=x;py=y;N<Two>(native)(self,x,y);if(pickerOpen&&pickerDrag){PickerPoint(x,y);PickerFlush(false);}else{HoverArea();if(before!=At(x,y))Dirty();}}
void __fastcall Move(void* self,void*,int x,int y){Motion(self,x,y,0x880E00);}
void __fastcall Hover(void* self,void*,int x,int y){Motion(self,x,y,0x880CB0);}
void __fastcall Cursor(void*,void*,int x,int y){void* mode=*reinterpret_cast<void**>(0x121333C);if(mode)N<void(__thiscall*)(void*,int)>(0xA764A0)(mode,At(x,y)>=0?2:0);}
void __fastcall Nothing(void*,void*,int,int){}
void __fastcall Wheel(void* self,void*,int delta){if(self!=frame||!delta||!Hit(px,py,184,82,300,184))return;int next=zoom+(delta>0?1:-1);if(next<0)next=0;if(next>6)next=6;if(next!=zoom){zoom=next;Dirty();}}
int __fastcall Message(void* self,void*,void* sender,int msg,int cmd,int a,int b,int c){if(msg==1){client::Close(true);return 0;}return N<int(__thiscall*)(void*,void*,int,int,int,int,int)>(0x8841D0)(self,sender,msg,cmd,a,b,c);}
void* __fastcall Delete(void* self,void*,unsigned flags){if(frame==self){frame=nullptr;DestroyPreview();pickerOpen=editing=pickerDirty=false;pickerDrag=0;hoverPart=-1;state={};pending=false;contextReady=false;actorContext={};}return N<void*(__thiscall*)(void*,unsigned)>(0x86E240)(self,flags);}
__declspec(naked) void __fastcall NativeDirty(void*,void*){__asm{mov eax,[esp]}__asm{cmp eax,0A470DEh}__asm{je skip}__asm{cmp eax,0A470FFh}__asm{je skip}__asm{push 0A23340h}__asm{ret}__asm{skip:ret}}
void Init(){if(ready)return;for(unsigned i=0;i<55;++i)table[i]=reinterpret_cast<u32*>(0x102FE04)[i];nativeCompose=table[1+0xA4/4];
#define SLOT(o,f) table[1+(o)/4]=reinterpret_cast<u32>(&f)
 SLOT(0,Delete);SLOT(0x50,Paint);SLOT(0x4C,Tick);SLOT(0x64,Down);SLOT(0x7C,Up);SLOT(0x6C,Move);SLOT(0x70,Hover);SLOT(0x78,Cursor);SLOT(0x80,Nothing);SLOT(0x68,Down);SLOT(0x8C,Wheel);SLOT(0x94,Message);SLOT(0x98,NativeDirty);SLOT(0xA4,Compose);
#undef SLOT
 ready=true;}
void Open(){Init();ActorContext current{};if(!ReadContext(current)){client::Close(true);return;}actorContext=current;contextReady=true;void* actor=current.actor;bool male=actor&&static_cast<u8*>(actor)[0x260]!=0;const char* catalogue=state.species?(male?"native_stylist\\hairs_dm.bin":"native_stylist\\hairs_df.bin"):(male?"native_stylist\\hairs_m.bin":"native_stylist\\hairs_f.bin");if(!client::ReadResource(catalogue,&hairs,sizeof(hairs))||hairs.magic!=0x3148534e||hairs.reserved||hairs.count>512)hairs.count=0;
 unsigned visible=0;const u32 limit=NativeHairLimits[state.species?1:0];for(unsigned i=0;i<hairs.count;++i)if(hairs.ids[i]&&hairs.ids[i]<=limit)hairs.ids[visible++]=hairs.ids[i];hairs.count=static_cast<u16>(visible);
 frame=N<void*(__cdecl*)(unsigned)>(0xDBBC4F)(0xB4);if(!frame)return;N<void*(__thiscall*)(void*,int)>(0x86B950)(frame,0);F(frame,0)=reinterpret_cast<u32>(table+1);F(frame,0x2C)=0x4e535459;
 N<Two>(0xA1CB70)(frame,880,600);N<Two>(0xA23450)(frame,40,40);N<Add>(0xA2D220)(Manager(),frame);N<void(__thiscall*)(void*,int)>(0x5AAD80)(frame,1);N<Add>(0xA39130)(Manager(),frame);RefreshMaterials();
}
}
void client::Close(bool notify){if(previewBusy){closeDeferred=true;closeNotify=closeNotify||notify;return;}if(notify)Send(1);if(frame){void* old=frame;frame=nullptr;if(N<void*(__thiscall*)(void*)>(0xA33100)(Manager())==old)N<Add>(0xA4B760)(Manager(),nullptr);if(N<void*(__thiscall*)(void*)>(0xA32E00)(Manager())==old)N<Void>(0xA482E0)(Manager());N<void(__thiscall*)(void*,int)>(0x5AAD80)(old,0);N<Add>(0xA447D0)(Manager(),old);}DestroyPreview();state={};equipped={};chosen={};characterChosen={};pending=editing=highlightArea=pickerOpen=pickerDirty=false;pickerDrag=0;hoverPart=-1;client::LocalPalettePreview(false);client::LocalCostumePreview(false);client::CostumePreviewHighlight(0,-1);pressed=-1;px=py=-1;contextReady=false;actorContext={};discardPreview=closeDeferred=closeNotify=false;}
bool client::Owns(void* w){return w&&w==frame;}
bool client::Key(void* front,unsigned message,unsigned key){
 if(front!=frame||!frame||!editing||pending)return false;
 if(pickerOpen){if(message==0x102)return true;if(message!=0x100)return false;if(key==27){PickerCancel();return true;}if(key==13){PickerAccept();return true;}if(key==9){pickerField=(pickerField+1)%4;FieldText();Dirty();return true;}if(key==8){if(hexLength)--hexLength;hex[hexLength]=0;}else if((key>=48&&key<=57)||(pickerField==0&&key>=65&&key<=70)){unsigned max=pickerField==0?6u:3u;if(hexLength<max){hex[hexLength++]=static_cast<char>(key);hex[hexLength]=0;}}else return true;
  if(hexLength&&(pickerField!=0||hexLength==6)){unsigned rgb=0;for(unsigned i=0;i<hexLength;++i)rgb=pickerField==0?(rgb<<4)|(hex[i]<='9'?hex[i]-'0':hex[i]-'A'+10):rgb*10+static_cast<unsigned>(hex[i]-'0');if(pickerField==0)pickerRGB=rgb;else if(rgb<=255){unsigned shift=static_cast<unsigned>(3-pickerField)*8;pickerRGB=(pickerRGB&~(255u<<shift))|(rgb<<shift);}picker::HSV(pickerRGB,pickerHue,pickerSat,pickerValue);pickerDirty=true;PickerFlush(true);}else Dirty();return true;
 }
 if(front!=frame||!frame||!editing||pending)return false;if(message==0x102)return true;if(message!=0x100)return false;if(key==27){editing=false;Dirty();return true;}if(key==13){Action(44);return true;}if(key==8){if(hexLength)--hexLength;hex[hexLength]=0;Dirty();return true;}if((key>=48&&key<=57)||(key>=65&&key<=70)){if(hexLength<6){hex[hexLength++]=static_cast<char>(key);hex[hexLength]=0;}Dirty();return true;}return true;
}
void client::ReceiveItems(const ns::ItemsPacket& p){
 if(frame&&!ObserveActor())return;
 if(p.version!=ns::Version||p.length!=sizeof(p)||p.reserved||p.count>4||p.session!=state.session)return;
 for(unsigned i=0;i<p.count;++i)if(!p.items[i].index||!p.items[i].instance||!p.items[i].view||p.items[i].view>=20000||p.items[i].domain<1||p.items[i].domain>2||p.items[i].slot>3)return;
 equipped=p;for(unsigned i=0;i<equipped.count;++i)equipped.items[i].name[63]=0;int current=CurrentItem();if(current>=0)selectedItem=current;DestroyPreview();RefreshMaterials();Dirty();
}
void client::Receive(const ns::State& h){
 if(h.version!=ns::Version||h.length!=sizeof(h)||h.event>3||h.species>1||!h.session||!ns::Valid(h.recipe)||(h.target?(h.recipe.domain==ns::Character||!h.instance):(h.recipe.domain!=ns::Character||h.instance)))return;
 if(h.event==3){if(h.session==state.session)Close(false);return;}if(h.event!=0&&h.session!=state.session)return;if(h.event!=0&&frame&&!ObserveActor())return;
 pickerOpen=editing=pickerDirty=false;pickerDrag=0;hoverPart=-1;
 if(h.event==0){Close(false);page=part=rotation=zoom=slot=materialPage=selectedItem=0;state=h;chosen=characterChosen=h.recipe;Open();}else{state=h;chosen=h.recipe;if(!h.target)characterChosen=h.recipe;}
 if(h.recipe.domain==ns::Character)client::PaletteRecipe(h.recipe);else client::CostumeRecipe(h.recipe);DestroyPreview();lastCostumeStatus=-1;pending=false;RefreshMaterials();
 const char* errors[]={"Preview ready. Confirm to apply.","All appearance changes saved.","Finish your current action first.","Selection expired. Reopen the Stylist.","This appearance has no reviewed material profile.","Storage unavailable. Nothing was applied.","No Look saved in this slot.","Look saved.","Appearance cache is full. Nothing was applied.","An edited costume changed. Reopen the Stylist.","Saving item identity. Please confirm again.","This item identity cannot be verified. Original appearance kept."};notice=h.result<12?errors[h.result]:"Request rejected.";if(client::PaletteCacheFull())notice="Client color cache is full. Close and restart the game.";Dirty();
}
EXPORT double __fastcall StylistScale(void* self,void*,int job){double base=N<double(__thiscall*)(void*,int)>(0xD7FD30)(self,job);const double scales[]={1,1.5,2,3,4,6,8};return drawing?base*scales[zoom]:base;}
