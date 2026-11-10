"""Generate external costume swatches and a compact native-style toggle."""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw
p=argparse.ArgumentParser();p.add_argument('--texture',required=True);a=p.parse_args();root=Path(a.texture)
(root/'swatches').mkdir(exist_ok=True)
for r in range(6):
 for g in range(6):
  for b in range(6):
   rgb=(51*r,51*g,51*b)
   if rgb==(255,0,255):rgb=(255,16,255)
   im=Image.new('RGB',(12,12),(80,80,88));ImageDraw.Draw(im).rectangle((1,1,10,10),fill=rgb);im.save(root/'swatches'/f'c{r}{g}{b}.bmp')
for state in ['normal','over','press','selected','disabled']:
 im=Image.open(root/f'tab_{state}.bmp').convert('RGB');assert im.size==(144,36)
 out=Image.new('RGB',(144,24));out.paste(im.crop((0,0,144,12)),(0,0));out.paste(im.crop((0,24,144,36)),(0,12));out.save(root/f'area_{state}.bmp')
print('Prepared216 external12x12 swatches and5 external144x24 toggle states')
