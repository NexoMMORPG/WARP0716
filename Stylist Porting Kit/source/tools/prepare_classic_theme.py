"""Prepare classic RO Stylist artwork from operator-owned Achievement/native UI assets.
Run after all other Stylist artwork generators. No game or renderer is started.
"""
from pathlib import Path
import argparse,io,json,hashlib
from PIL import Image,ImageDraw,ImageEnhance
from grf import GRF
p=argparse.ArgumentParser();p.add_argument('--grf',required=True);p.add_argument('--texture',required=True);p.add_argument('--out',required=True);a=p.parse_args()
src=Path(a.texture);out=Path(a.out);out.mkdir(parents=True,exist_ok=True)
g=GRF(a.grf,lambda n:n.startswith('data\\texture\\') and n.endswith('.bmp'));sources={}
def native(folder,name):
 suffix='\\'+folder+'\\'+name;key=next(n for n in g.entries if n.endswith(suffix));raw=g.read(key);sources[key]=hashlib.sha256(raw).hexdigest();return Image.open(io.BytesIO(raw)).convert('RGB')
def save(im,name):
 p=out/name;p.parent.mkdir(parents=True,exist_ok=True);im.save(p)
def sliced(im,size,edge=3):
 w,h=im.size;W,H=size;e=min(edge,w//3,h//3,W//3,H//3);result=Image.new('RGB',size)
 for x0,x1,X0,X1 in [(0,e,0,e),(e,w-e,e,W-e),(w-e,w,W-e,W)]:
  for y0,y1,Y0,Y1 in [(0,e,0,e),(e,h-e,e,H-e),(h-e,h,H-e,H)]:result.paste(im.crop((x0,y0,x1,y1)).resize((X1-X0,Y1-Y0),Image.Resampling.NEAREST),(X0,Y0))
 return result
blank=native('achievement_re','tab_2.bmp');blue=native('achievement_re','tab_1.bmp')
side={s:native('achievement_re','tab_'+n+'.bmp') for s,n in [('normal','out'),('over','over'),('selected','press')]}
sub={s:native('achievement_re','tab_sub_'+n+'.bmp') for s,n in [('normal','out'),('over','over'),('selected','press')]}
def keyed(before,after):
 a=before.load();b=after.load()
 for y in range(before.height):
  for x in range(before.width):
   if a[x,y]==(255,0,255):b[x,y]=(255,0,255)
 return after
def wash(im,fraction):
 return keyed(im,Image.blend(im,Image.new('RGB',im.size,'white'),fraction))
def button(size,state,kind='button'):
 if kind=='sidebar':im=Image.new('RGB',size,{'normal':'#ffffff','over':'#eff7ff','selected':'#d6e7ff','press':'#c6d7f7','disabled':'#ffffff'}[state])
 elif kind=='material':
  im=sliced(sub['normal' if state=='disabled' else 'selected' if state=='press' else state],size);ImageDraw.Draw(im).rectangle((0,0,size[0]-1,size[1]-1),outline='#809ccc' if state in ['selected','press'] else '#c2cde7')
 else:
  base=blue if state in ['over','press','selected'] or (kind=='confirm' and state=='normal') else blank
  im=sliced(base,size);im=keyed(im,ImageEnhance.Brightness(im).enhance(.94)) if state=='press' else im
 if state=='disabled':im=wash(im,.65)
 if kind=='sidebar' and state in ['selected','press']:ImageDraw.Draw(im).line((0,0,0,size[1]-1),fill='#4b70bc',width=2)
 return im
states=['normal','over','press','selected','disabled']
for kind,size in [('button',(128,32)),('confirm',(128,32)),('tab',(144,36)),('small',(40,28)),('material',(108,20)),('area',(144,24)),('sidebar',(144,36)),('picker/hex',(144,28)),('picker/channel',(64,28))]:
 for state in states:save(button(size,state,kind),f'{kind}_{state}.bmp')
close={s:native('cashemotion','btn_close_'+s+'.bmp') for s in ['normal','over','press']}
for state in states:
 im=Image.new('RGB',(24,24),(255,0,255));glyph=close['normal' if state=='disabled' else 'press' if state in ['press','selected'] else state];im.paste(glyph,(6,3));save(im,f'close_{state}.bmp')
# The fixed background uses classic native stripe art and an offline-sized title bar.
im=Image.new('RGB',(880,600),'white');d=ImageDraw.Draw(im);d.rectangle((0,0,879,599),outline='#98a8c8')
# Plain navigation background. Selection is indicated by the tab fill/accent.
d.rectangle((1,40,167,545),fill='white')
d.line((168,40,168,545),fill='#bcc7df');d.line((178,540,866,540),fill='#c7d2e9')
bar=native('basic_interface','titlebar_fix.bmp');im.paste(sliced(bar,(878,17),3),(1,0))
summary=native('achievement_re','bg_summary.bmp')
if summary.size!=(648,404):raise ValueError('Revalidate the clean native Achievement panel sample for this client')
# Preserve the native two-column pale-blue stripe, including its 16-bit-safe colors.
panel_tile=summary.crop((400,170,402,171))
for box in [(184,82,480,264),(500,82,860,264)]:
 d.rectangle(box,outline='#becafa')
 fill=Image.new('RGB',(box[2]-box[0]-1,box[3]-box[1]-1))
 for x in range(fill.width):ImageDraw.Draw(fill).line((x,0,x,fill.height-1),fill=panel_tile.getpixel((x%2,0)))
 im.paste(fill,(box[0]+1,box[1]+1));d.rectangle((box[0]+2,box[1]+2,box[2]-2,box[3]-2),outline='white')
d.line((178,306,866,306),fill='#d9e2f4')
# The existing native bitmap color key makes only the top corners transparent.
radius=8;mask=Image.new('L',im.size,255);top=Image.new('L',(880,17),0);ImageDraw.Draw(top).rounded_rectangle((0,0,879,16),radius=radius,fill=255);mask.paste(top.crop((0,0,880,radius)),(0,0))
d.arc((0,0,16,16),180,270,fill='#98a8c8');d.arc((863,0,879,16),270,360,fill='#98a8c8')
im.paste((255,0,255),(0,0,880,600),Image.eval(mask,lambda value:255-value));save(im,'window.bmp')
for state,color in [('normal','#c4cfe6'),('over','#4d77ce'),('press','#244e9a'),('selected','#244e9a'),('disabled','#d9dfea')]:
 im=Image.new('RGB',(39,18),(255,0,255));ImageDraw.Draw(im).rectangle((0,0,38,17),outline=color,width=2);save(im,f'dye_{state}.bmp')
# An externally authored raised dialog frame and title strip, within the same footprint.
im=Image.new('RGB',(686,238),(255,0,255));dialog=ImageDraw.Draw(im)
dialog.rounded_rectangle((4,4,685,237),radius=6,fill='#a5b5ce')
dialog.rounded_rectangle((0,0,681,233),radius=6,fill='white',outline='#6b84ad',width=2)
dialog.rounded_rectangle((2,2,679,231),radius=4,outline='#dee7f7',width=1)
strip=sliced(bar,(674,24),3);pixels=strip.load()
for y in range(strip.height):
 for x in range(strip.width):
  if pixels[x,y]==(255,0,255):pixels[x,y]=(255,255,255)
im.paste(strip,(4,4));dialog.line((4,28,677,28),fill='#8c9fc6');save(im,'picker/panel.bmp')
for name in ['point','hue_point']:
 im=Image.open(src/'picker'/f'{name}.bmp').convert('RGB');pixels=im.load()
 for y in range(im.height):
  for x in range(im.width):
   if pixels[x,y]==(0,0,0):pixels[x,y]=(43,71,123)
 save(im,f'picker/{name}.bmp')
# Change only the chip borders. RGB interiors and all picker/preset colors remain exact.
for path in sorted((src/'chips').glob('*.bmp')):
 im=Image.open(path).convert('RGB');ImageDraw.Draw(im).rectangle((0,0,11,11),outline='#9eafd1');save(im,'chips/'+path.name)
(out/'CLASSIC_THEME_SOURCES.json').write_bytes(json.dumps(dict(native_sources=sources,theme='classic white/blue, rounded top corners, plain navigation, raised picker',runtime_art_generation=False),indent=2).encode())
print('Prepared classic native-style theme:',len(list(out.rglob('*.bmp'))),'BMPs')
