#pragma once
// Material palette shading and color-group interpolation.
// SPDX-License-Identifier: GPL-3.0-or-later
// Preserve palette indices when recoloring material groups.
namespace ns {
struct GroupRecipe {
 unsigned short enabled;
 unsigned char reference[13][3];
 unsigned char target[13][3];
};
inline int Smaller(int a,int b){return a<b?a:b;}
inline int Larger(int a,int b){return a>b?a:b;}
inline int Bound(int value,int lo,int hi){return value<lo?lo:value>hi?hi:value;}
inline int Minimum(const unsigned char* rgb){return Smaller(rgb[0],Smaller(rgb[1],rgb[2]));}
inline int Maximum(const unsigned char* rgb){return Larger(rgb[0],Larger(rgb[1],rgb[2]));}
inline void Shade(const GroupRecipe& recipe,unsigned group,unsigned char* rgb){
 if(group>=13||!(recipe.enabled&(1u<<group)))return;
 const auto base=recipe.reference[group];const auto target=recipe.target[group];
 if(base[0]==target[0]&&base[1]==target[1]&&base[2]==target[2])return;
 const int sourceLight=Minimum(rgb)+Maximum(rgb);
 const int referenceLight=Bound(Minimum(base)+Maximum(base),1,509);
 const int targetMin=Minimum(target),targetMax=Maximum(target);
 const int targetLight=targetMin+targetMax;
 int light=Bound(targetLight,16,494);
 if(sourceLight>referenceLight){
  const int remaining=510-referenceLight;
  light+=(remaining/2+(sourceLight-referenceLight)*(510-light))/remaining;
 }else light=(referenceLight/2+light*sourceLight)/referenceLight;
 const int saturationDenominator=Smaller(targetLight,510-targetLight);
 const int span=saturationDenominator?(saturationDenominator/2+Smaller(light,510-light)*(targetMax-targetMin))/saturationDenominator:0;
 const int low=(light-span)/2;
 for(unsigned c=0;c<3;++c){
  const int shifted=targetMax==targetMin?0:((targetMax-targetMin)/2+(target[c]-targetMin)*span)/(targetMax-targetMin);
  rgb[c]=static_cast<unsigned char>(Bound(low+shifted,0,255));
 }
}
inline unsigned ColorGroup(const unsigned char* rgb){
 const int maximum=Maximum(rgb),minimum=Minimum(rgb),span=maximum-minimum;
 const int light=maximum+minimum;
 if(span*255<=20*Smaller(light,510-light))return 12;
 int hue;
 if(maximum==rgb[0])hue=60*(static_cast<int>(rgb[1])-rgb[2])/span;
 else if(maximum==rgb[1])hue=120+60*(static_cast<int>(rgb[2])-rgb[0])/span;
 else hue=240+60*(static_cast<int>(rgb[0])-rgb[1])/span;
 return static_cast<unsigned>((hue+375)%360/30);
}
inline void ShadeClassified(const GroupRecipe& recipe,unsigned char* rgb){Shade(recipe,ColorGroup(rgb),rgb);}
}
