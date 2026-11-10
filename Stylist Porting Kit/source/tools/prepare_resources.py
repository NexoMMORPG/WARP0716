"""Prepare current NSM4 materials and base artwork from matching game resources.

Run the picker and classic-theme generators afterwards for the complete UI.
"""
from pathlib import Path
import argparse,json
from PIL import Image,ImageDraw
p=argparse.ArgumentParser();p.add_argument('--grf',required=True);p.add_argument('--out',required=True);p.add_argument('--overlay',action='append',default=[]);p.add_argument('--v2',action='store_true');p.add_argument('--corrected');p.add_argument('--profiles');p.add_argument('--latest');p.add_argument('--labels-header');a=p.parse_args()
if not a.v2:p.error('This release requires --v2 for its current NSM4 material format')
if not a.corrected:p.error('--v2 requires --corrected with matching fingerprinted sprites')
out=Path(a.out).resolve();out.mkdir(parents=True,exist_ok=True);art=out/'texture/native_stylist';art.mkdir(parents=True,exist_ok=True)
materials=out/'native_stylist/materials';materials.mkdir(parents=True,exist_ok=True)
colors={'normal':'#30303a','over':'#46415b','press':'#262232','selected':'#665399','disabled':'#27272d'}
def image(name,w,h,fill,border='#585363',radius=3):
 im=Image.new('RGB',(w,h),(255,0,255));d=ImageDraw.Draw(im);d.rounded_rectangle((0,0,w-1,h-1),radius,fill=fill,outline=border);im.save(art/(name+'.bmp'));return im
im=image('window',880,600,'#18181c','#716282',6);d=ImageDraw.Draw(im)
d.rectangle((1,1,878,40),fill='#121214');d.line((168,42,168,545),fill='#39333f');d.line((178,540,866,540),fill='#39333f')
d.rounded_rectangle((184,82,480,264),5,fill='#202027',outline='#494350');d.rounded_rectangle((500,82,860,264),5,fill='#202027',outline='#494350');im.save(art/'window.bmp')
for name,w,h in [('button',128,32),('confirm',128,32),('tab',144,36),('small',40,28),('close',24,24)]+([('material',108,20)] if a.v2 else []):
 for state,c in colors.items():
  fill=('#b72d80' if state=='normal' else '#d23d98' if state=='over' else c) if name=='confirm' else c
  im=image(name+'_'+state,w,h,fill)
  if name=='close':
   d=ImageDraw.Draw(im);d.line((7,7,16,16),fill='#f0f0f4',width=2);d.line((16,7,7,16),fill='#f0f0f4',width=2);im.save(art/(name+'_'+state+'.bmp'))
for state,border in [('normal','#56515f'),('over','#c8bbff'),('press','#ef7fc3'),('selected','#ef7fc3'),('disabled','#28282c')]:
 im=Image.new('RGB',(39,18),(255,0,255));d=ImageDraw.Draw(im);d.rectangle((0,0,38,17),outline=border,width=2);im.save(art/('dye_'+state+'.bmp'))
def swatch(index):
 if index<13:return (255*index//12,)*3
 row=index//13-1;col=index%13;h=col*1536//13;sector=h//256;t=h%256
 v=255 if row<3 else 240-(row-3)*42;s=80+row*80 if row<3 else 230
 lo=v*(255-s)//255;up=lo+(v-lo)*t//255;down=v-(v-lo)*t//255
 return [(v,up,lo),(down,v,lo),(lo,v,up),(lo,down,v),(up,lo,v),(v,lo,down)][sector]
im=Image.new('RGB',(507,144),(255,0,255));d=ImageDraw.Draw(im)
for i in range(104):
 x=i%13*39;y=i//13*18;d.rectangle((x+2,y+2,x+36,y+15),fill=swatch(i))
im.save(art/'dyes.bmp')
from material_authoring import prepare_v2
(out/'native_stylist/dyes.json').write_text(json.dumps([swatch(i) for i in range(104)]),encoding='utf-8')
print(json.dumps(prepare_v2(a.grf,a.corrected,out,a.profiles,a.latest,a.labels_header,a.overlay),ensure_ascii=True))
