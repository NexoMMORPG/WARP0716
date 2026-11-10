"""Build logical hairstyles from target original tables and verified physical pairs."""
from pathlib import Path
import argparse,json,struct,re,hashlib

def build(corrected,tables,data,header,doram_tables=None,include_cross_sex=False):
    corrected=Path(corrected);data=Path(data)/'native_stylist';data.mkdir(parents=True,exist_ok=True)
    target=next(r for r in json.loads(Path(tables).read_text(encoding='utf-8')) if r['label']=='target')
    result={};crosswalk={};excluded={}
    for word,sex in [('male','m'),('female','f')]:
        korean='남' if sex=='m' else '여';entries=next(t['entries'] for t in target['tables'] if t['sex']==word)
        native={e['logical_style']:int(e['filename_stem']) for e in entries};root=corrected/f'data/sprite/인간족/머리통/{korean}'
        pairs={int(p.stem.split('_')[0]) for p in root.glob('*.spr') if p.with_suffix('.act').is_file() and re.fullmatch(r'[0-9]+_'+korean,p.stem)}
        rows=[];excluded[sex]=[]
        for logical in range(1,512):
            physical=native[logical] if logical<43 else logical
            if physical in pairs:
                # This asset set adds opposite-sex copies at physical ID+200.
                # Confirm the actual sprite bytes before classifying a copy.
                opposite='여' if sex=='m' else '남'
                other=corrected/f'data/sprite/인간족/머리통/{opposite}/{physical-200}_{opposite}.spr'
                source=root/f'{physical}_{korean}.spr'
                copied=physical>=200 and other.is_file() and source.read_bytes()==other.read_bytes()
                if copied and not include_cross_sex:
                    excluded[sex].append(dict(logical=logical,physical=physical,opposite_physical=physical-200,sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
                    continue
                rows.append(dict(logical=logical,physical=physical,original_lookup=logical<43))
        ids=[r['logical'] for r in rows]
        if not 1<=len(ids)<=512:raise ValueError('Invalid paired catalogue')
        (data/('hairs_'+sex+'.bin')).write_bytes(struct.pack('<IHH',0x3148534e,len(ids),0)+struct.pack('<'+'H'*512,*(ids+[0]*(512-len(ids)))))
        result[sex]=ids;crosswalk[sex]=rows
    doram_tables=Path(doram_tables) if doram_tables else Path(__file__).with_name('hair_doram_tables_v2.json')
    doram=next(r for r in json.loads(doram_tables.read_text(encoding='utf-8')) if r['label']=='target')
    for word,sex in [('male','dm'),('female','df')]:
        korean='남' if sex=='dm' else '여';root=corrected/f'data/sprite/도람족/머리통/{korean}'
        entries=next(t['entries'] for t in doram['tables'] if t['sex']==word)
        rows=[]
        for entry in entries:
            logical=entry['logical_style'];physical=int(entry['filename_stem'])
            if not 1<=logical<=10:raise ValueError('Doram native style outside reviewed range')
            source=root/f'{physical}_{korean}.spr'
            if not source.is_file() or not source.with_suffix('.act').is_file():raise ValueError('Missing native Doram SPR/ACT pair: '+str(source))
            rows.append(dict(logical=logical,physical=physical,original_lookup=True,species='doram'))
        ids=[r['logical'] for r in rows]
        if ids!=list(range(1,11)):raise ValueError('Incomplete native Doram catalogue')
        (data/('hairs_'+sex+'.bin')).write_bytes(struct.pack('<IHH',0x3148534e,len(ids),0)+struct.pack('<'+'H'*512,*(ids+[0]*(512-len(ids)))))
        result[sex]=ids;crosswalk[sex]=rows
    (data/'hairs.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    (data/'hair_crosswalk.json').write_text(json.dumps(crosswalk,indent=2),encoding='utf-8')
    (data/'hair_sex_filter.json').write_text(json.dumps(dict(include_cross_sex=include_cross_sex,method='Physical ID+200 and exact opposite-sex SPR byte equality. ACT files may have adjusted offsets.',excluded=excluded),indent=2),encoding='utf-8')
    header=Path(header);header.parent.mkdir(parents=True,exist_ok=True)
    header.write_text('#pragma once\n// Target logical hair catalogue. SPDX-License-Identifier: GPL-3.0-or-later\nnamespace ns_hairs {\n'+''.join('static const unsigned short '+sex+'[]={'+','.join(map(str,ids))+'};\n' for sex,ids in result.items())+'}\n',encoding='utf-8')
    return {sex:dict(count=len(ids),minimum=min(ids),maximum=max(ids),logical1_physical=crosswalk[sex][0]['physical']) for sex,ids in result.items()}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--corrected',required=True);p.add_argument('--tables',required=True);p.add_argument('--doram-tables');p.add_argument('--data',required=True);p.add_argument('--header',required=True);p.add_argument('--include-cross-sex',action='store_true',help='Include confirmed opposite-sex ID+200 copies instead of the default sex-specific lists');a=p.parse_args()
    print(json.dumps(build(a.corrected,a.tables,a.data,a.header,a.doram_tables,a.include_cross_sex)))
