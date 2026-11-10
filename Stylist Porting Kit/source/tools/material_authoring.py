"""Build explicit NSM4 materials from reviewed operator-owned SPR families."""
from pathlib import Path
import hashlib,json,re,struct
from sprite import read
from grf import GRF

MAGIC=0x344d534e
GROUP_COUNT=34
MATERIAL_BYTES=1424
BODY_MATERIAL_BYTES=MATERIAL_BYTES+520
UNEDITABLE=0xff

def sha(data):return hashlib.sha256(data).hexdigest()

def write_labels(records,profiles,out,labels_header=None):
    """Keep anatomical names attached to their reviewed source material identity."""
    names=['Hair','Skin']+[f'Palette {start}-{start+7}' for start in range(0,256,8)]
    labels={};covered=[]
    for record in records:
        if record.get('material_kind')!='body' and '\\몸통\\' not in record['source']:continue
        key=int(record['hash'],16);selected={}
        for group in record['groups']:
            slot=group['slot'];name=group.get('name',names[slot])
            if not 0<=slot<GROUP_COUNT or not name or len(name.encode('ascii'))>=40:raise ValueError('Invalid material label')
            if slot>=2 and name!=names[slot]:selected[slot]=name
        if key in labels and labels[key]!=selected:raise ValueError('Conflicting labels for one material identity')
        labels[key]=selected
        sources=[record['source']]+record.get('equivalent_sources',[])
        skin_covered=record.get('skin_status')=='covered' or all(profiles.get(source.lower(),{}).get('skin_status')=='covered' for source in sources)
        if skin_covered:
            if any(group['slot']==1 for group in record['groups']):raise ValueError('Covered body cannot also expose skin material')
            covered.append(key)
    (Path(out)/'native_stylist/group_names.bin').write_bytes(b''.join(n.encode('ascii').ljust(40,b'\0') for n in names))
    if labels_header:
        semantic=any(labels.values())
        lines=['#pragma once','// Generated material labels. SPDX-License-Identifier: GPL-3.0-or-later','namespace ns_labels {','static const char* const Names[34]={',',\n'.join(json.dumps(n) for n in names),'};','inline const char* Outfit(unsigned family,unsigned group){',' switch(family){' if semantic else ' (void)family;']
        for key,selected in sorted(labels.items()):
            if not selected:continue
            lines.append(f' case 0x{key:08x}u:switch(group){{')
            lines.extend(f'  case {slot}:return {json.dumps(name)};' for slot,name in sorted(selected.items()))
            lines.append('  default:break;}break;')
        if semantic:lines.append(' default:break;}')
        lines+=[' return group<34?Names[group]:"Material";','}','inline bool BodySkinCovered(unsigned family){',' switch(family){' if covered else ' (void)family;']
        lines.extend(f' case 0x{key:08x}u:return true;' for key in sorted(set(covered)))
        lines+=[' default:return false;}' if covered else ' return false;','}','}','']
        destination=Path(labels_header);destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text('\n'.join(lines),encoding='utf-8')

def family_hash(prefix):
    value=2166136261
    for c in prefix.encode('cp949'):
        if 65<=c<=90:c+=32
        value=((value^c)*16777619)&0xffffffff
    return value

def palette_prefix(relative):
    parts=relative.replace('/','\\').split('\\');stem=Path(parts[-1]).stem
    if '머리통' in parts:
        match=re.fullmatch(r'([0-9]+)_(남|여)',stem)
        if not match or len(parts)<2 or parts[-2]!=match[2]:return None
        parent='palette\\도람족\\머리\\' if '도람족' in parts else 'palette\\머리\\'
        return parent+'머리'+stem+'_'
    if '몸통' in parts:
        at=parts.index('몸통')
        if '인간족' not in parts or len(parts)!=at+3 or parts[at+1] not in ['남','여']:return None
        if not stem.endswith('_'+parts[at+1]):return None
        return 'palette\\몸\\'+stem+'_'
    return None

def material_blob(prefix,palette,groups):
    if len(palette)!=1024:raise ValueError('A full 1024 byte source palette is required')
    mask=bytearray([UNEDITABLE]*256);refs=bytearray(GROUP_COUNT*3);anchors=bytearray(GROUP_COUNT)
    for group in groups:
        slot=group['slot'];indices=group['indices'];anchor=group['anchor_index']
        if not 0<=slot<GROUP_COUNT or anchors[slot] or not indices or anchor not in indices:raise ValueError('Invalid or duplicate group or anchor')
        for index in indices:
            if not 1<=index<=255 or mask[index]!=UNEDITABLE:raise ValueError('Masks must be disjoint and preserve index zero')
            mask[index]=slot
        anchors[slot]=anchor
        refs[slot*3:slot*3+3]=palette[anchor*4:anchor*4+3]
        group['reference_rgb']=list(refs[slot*3:slot*3+3])
        assert any(mask[i]==slot and palette[i*4:i*4+3]==refs[slot*3:slot*3+3] for i in range(1,256))
    blob=struct.pack('<II',MAGIC,family_hash(prefix))+palette+mask+refs+anchors
    assert len(blob)==MATERIAL_BYTES and mask[0]==UNEDITABLE
    return blob

def body_blob(material,remap):
    pairs=[tuple(pair) for pair in remap if pair[0]!=pair[1]]
    if len(pairs)>256 or any(len(pair)!=2 or not 1<=pair[0]<=255 or not 1<=pair[1]<=255 for pair in pairs):
        raise ValueError('Invalid source-specific body palette remap')
    if len({pair[1] for pair in pairs})!=len(pairs):raise ValueError('Body remap targets must be unique')
    old=bytes(pair[0] for pair in pairs).ljust(256,b'\0');new=bytes(pair[1] for pair in pairs).ljust(256,b'\0')
    blob=material+struct.pack('<IHH',0x3152534e,len(pairs),0)+old+new
    assert len(blob)==BODY_MATERIAL_BYTES
    return blob,pairs

def stage_latest(latest,stage):
    if not latest:return {},[]
    latest=Path(latest).resolve()
    manifest=json.loads((latest/'GENERATED_COMPARISON.json').read_text(encoding='utf-8'))
    selected={};records=[]
    for item in manifest['files']:
        if item['status']=='identical' or not item['path'].endswith('.spr') or '\\몸통\\' not in item['path']:continue
        source=latest/'extracted-generated'/item['path'];content=source.read_bytes()
        if sha(content)!=item['sha256']:raise ValueError('Sprite differs from authenticated extraction')
        dest=stage/'visual-update'/item['path'];dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(content)
        selected[item['path'].lower()]=source
        records.append(dict(path=item['path'],sha256=item['sha256'],bytes=len(content),source=str(source),staged=str(dest)))
    (stage/'VISUAL_UPDATE.json').write_text(json.dumps({'files':records,'deployed':False},indent=2,ensure_ascii=False),encoding='utf-8')
    return selected,records

def prepare_v2(grf_path,corrected,out,profiles_path=None,latest=None,labels_header=None,resource_overlays=None):
    corrected=Path(corrected).resolve();out=Path(out).resolve();stage=out.parent
    profiles_path=Path(profiles_path) if profiles_path else Path(__file__).with_name('material_profiles_v2.json')
    authored=json.loads(profiles_path.read_text(encoding='utf-8'));profiles={r['source'].lower():r for r in authored['families']}
    previous_path=out/'native_stylist/materials.json'
    previous=json.loads(previous_path.read_text(encoding='utf-8'))['families'] if previous_path.is_file() else []
    latest_sources,updates=stage_latest(latest,stage)
    materials=out/'native_stylist/materials';materials.mkdir(parents=True,exist_ok=True)
    body_materials=out/'native_stylist/body_materials';body_materials.mkdir(parents=True,exist_ok=True)
    palettes=out/'native_stylist/palettes';palettes.mkdir(parents=True,exist_ok=True)
    records=[];seen={};unsupported=[];unmapped=[]
    overlays=[Path(p).resolve() for p in (resource_overlays or [])]
    sources={str(p.relative_to(corrected)).replace('/','\\').lower():p for p in (corrected/'data/sprite').rglob('*.spr')}
    overridden={}
    for overlay in overlays:
        if not (overlay/'data/sprite').is_dir():raise ValueError('Resource overlay requires data/sprite: '+str(overlay))
        for path in (overlay/'data/sprite').rglob('*.spr'):
            relative=str(path.relative_to(overlay)).replace('/','\\');overridden[relative.lower()]=path
            sources.setdefault(relative.lower(),path)
    for lower,source in sorted(sources.items()):
        relative=next((str(source.relative_to(folder)).replace('/','\\') for folder in [corrected]+overlays if source.is_relative_to(folder)),None)
        if relative is None:raise ValueError('Sprite is outside explicit source roots')
        profile=profiles.get(lower);body='\\몸통\\' in relative
        prefix=profile.get('palette_prefix') if profile else None
        if prefix is not None:
            if not prefix.startswith('palette\\') or not prefix.endswith('_') or '..' in prefix.split('\\') or '/' in prefix or len(prefix.encode('cp949'))>=240:
                raise ValueError('Invalid explicit native palette prefix: '+relative)
            if not profile.get('native_binding'):raise ValueError('Explicit palette prefix requires native binding metadata: '+relative)
        else:prefix=palette_prefix(relative)
        if prefix is None and not body:
            unmapped.append(dict(source=relative,reason='Native palette prefix is not proven for this layout or path. Use native fallback.'))
            continue
        selected=overridden.get(lower,latest_sources.get(lower,source));content=selected.read_bytes()
        try:frames,palette,truecolor=read(content)
        except ValueError as error:unsupported.append(dict(source=relative,reason=str(error)));continue
        if truecolor or not frames:unsupported.append(dict(source=relative,reason='Truecolor or empty indexed sprite needs another adapter'));continue
        digest=hashlib.sha256()
        for width,height,pixels in frames:digest.update(struct.pack('<II',width,height));digest.update(pixels)
        layout=digest.hexdigest();groups=[];status='unreviewed'
        if profile:
            if sha(content)!=profile['spr_sha256'] or layout!=profile['layout_sha256']:raise ValueError('Reviewed source fingerprint changed: '+relative)
            groups=[dict(g) for g in profile['groups']];status=profile['status']
        native_prefix=prefix
        if body:prefix=relative[5:] if relative.lower().startswith('data\\') else relative
        key=family_hash(prefix)
        if key in seen and seen[key]!=prefix:raise ValueError('Material hash collision')
        if prefix in seen.values():
            equivalent=next(r for r in records if r['prefix']==prefix)
            if equivalent['palette_sha256']!=sha(palette):unsupported.append(dict(source=relative,reason='Native prefix has conflicting source palettes',prefix=prefix));continue
            equivalent.setdefault('equivalent_sources',[]).append(relative);continue
        seen[key]=prefix;blob=material_blob(prefix,palette,groups);remap=[]
        if body:blob,remap=body_blob(blob,profile.get('palette_remap',[]) if profile else [])
        destination=(body_materials if body else materials)/(f'{key:08x}'+('.nsb' if body else '.nsm'));destination.write_bytes(blob)
        suffix=profile.get('palette_suffix','.pal') if profile else '.pal'
        if not re.fullmatch(r'(?:_[0-9]+)?\.pal',suffix):raise ValueError('Invalid native palette suffix: '+relative)
        record=dict(source=relative,source_file=str(selected),source_sha256=sha(content),layout_sha256=layout,palette_sha256=sha(palette),prefix=prefix,native_palette_prefix=native_prefix,palette_suffix=suffix,prefix_cp949=prefix.encode('cp949').hex(),hash=f'{key:08x}',file=str(destination.relative_to(out)),material_kind='body' if body else 'head',skin_status=profile.get('skin_status','reviewed' if any(g['slot']==1 for g in groups) else 'unreviewed') if profile else 'unreviewed',palette_remap=remap,bytes=len(blob),sha256=sha(blob),status=status,groups=groups,available_flags=sum(1<<g['slot'] for g in groups),latest_overlay=selected!=source,legacy_bases=[],legacy_sources=[])
        if profile:
            if profile.get('native_binding'):record['native_binding']=profile['native_binding']
        records.append(record)
    grf=GRF(grf_path,lambda name:name.startswith('data\\palette\\') and name.endswith('.pal'));legacy_count=0
    for r in records:
        if r['material_kind']=='body':
            r['legacy_resolution']='Exact native palette filename and current client resource priority'
            continue
        if not r['groups']:continue
        for base in range(1,256):
            native='data\\'+r['prefix']+str(base)+r['palette_suffix']
            corrected_palette=next((folder/native for folder in reversed([corrected]+overlays) if (folder/native).is_file()),corrected/native)
            if corrected_palette.is_file():
                palette=corrected_palette.read_bytes();origin=dict(source_file=str(corrected_palette))
            elif native.lower() in grf.entries:
                palette=grf.read(native);origin=dict(source_grf=str(Path(grf_path).resolve()),entry=native)
            else:continue
            if len(palette)!=1024:raise ValueError('Invalid legacy palette: '+native)
            (palettes/(r['hash']+'_'+str(base)+'.pal')).write_bytes(palette)
            r['legacy_bases'].append(base);r['legacy_sources'].append(dict(base=base,sha256=sha(palette),**origin));legacy_count+=1
    manifest=dict(schema_version=4,magic=f'{MAGIC:08x}',material_bytes=MATERIAL_BYTES,body_material_bytes=BODY_MATERIAL_BYTES,anchor_bytes=GROUP_COUNT,group_count=GROUP_COUNT,uneditable=UNEDITABLE,source_grf=str(Path(grf_path).resolve()),corrected_source=str(corrected),resource_overlays=[str(p) for p in overlays],body_identity='ASCII-case-folded normalized native SPR resource path hash',authoring_source=str(profiles_path.resolve()),authoring_sha256=sha(profiles_path.read_bytes()),families=records,unsupported=unsupported,unmapped=unmapped,latest_visual_updates=updates)
    current={r['hash']+'.nsm' for r in records}
    for old in previous:
        filename=old['hash']+'.nsm'
        if filename not in current and re.fullmatch(r'[0-9a-f]{8}\.nsm',filename):
            obsolete=materials/filename
            assert obsolete.parent.resolve()==materials.resolve()
            if obsolete.is_file():obsolete.unlink()
    (out/'native_stylist/materials.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding='utf-8')
    write_labels(records,profiles,out,labels_header)
    summary=dict(material_families=len(records),body_families=sum(r['material_kind']=='body' for r in records),reviewed_primary_hair=sum(any(g['slot']==0 for g in r['groups']) for r in records),reviewed_skin=sum(any(g['slot']==1 for g in r['groups']) for r in records),reviewed_outfit=sum(any(g['slot']>=2 for g in r['groups']) for r in records),covered_bodies=sum(r['material_kind']=='body' and r['skin_status']=='covered' for r in records),remapped_bodies=sum(bool(r['palette_remap']) for r in records),original_only=sum(not r['groups'] for r in records),unsupported=len(unsupported),unmapped_sources=len(unmapped),legacy_palettes=legacy_count,latest_body_sprites=len(updates),material_bytes=MATERIAL_BYTES,body_material_bytes=BODY_MATERIAL_BYTES)
    (stage/'MATERIAL_SUMMARY.json').write_text(json.dumps(summary,indent=2),encoding='utf-8')
    return summary
