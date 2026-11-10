#pragma once
// Integer HSV math shared by external-art generation tests and native UI.
namespace picker {
inline unsigned RGB(unsigned hue,unsigned saturation,unsigned value){
 unsigned h=hue*1535/149,sector=h/256,t=h%256;
 unsigned low=value*(255-saturation)/255,up=low+(value-low)*t/255,down=value-(value-low)*t/255,r=0,g=0,b=0;
 switch(sector){case 0:r=value;g=up;b=low;break;case 1:r=down;g=value;b=low;break;case 2:r=low;g=value;b=up;break;case 3:r=low;g=down;b=value;break;case 4:r=up;g=low;b=value;break;default:r=value;g=low;b=down;}
 return r<<16|g<<8|b;
}
inline void HSV(unsigned rgb,unsigned& hue,unsigned& saturation,unsigned& value){
 int r=(rgb>>16)&255,g=(rgb>>8)&255,b=rgb&255,hi=r>g?r:g;hi=hi>b?hi:b;int lo=r<g?r:g;lo=lo<b?lo:b;int span=hi-lo,h=0;
 if(span){if(hi==r)h=256*(g-b)/span;else if(hi==g)h=512+256*(b-r)/span;else h=1024+256*(r-g)/span;if(h<0)h+=1536;hue=static_cast<unsigned>(h)*149/1535;}
 saturation=hi?static_cast<unsigned>(span*255/hi):0;value=static_cast<unsigned>(hi);
}
}
