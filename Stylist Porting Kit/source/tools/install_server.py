"""Apply the small upstream integration with exact source anchors and verified backups."""
import argparse,hashlib,json,shutil,datetime
from pathlib import Path
from server_storage_config import storage_header
p=argparse.ArgumentParser();p.add_argument('--server',required=True);p.add_argument('--backup',required=True);p.add_argument('--inter-config',default='conf/inter_athena.conf');a=p.parse_args()
root=Path(a.server).resolve();backup=Path(a.backup).resolve();kit=Path(__file__).resolve().parents[1]
if not (root/'rAthena.sln').is_file():raise SystemExit('Expected rAthena source root')
if backup.exists():raise SystemExit('Choose a fresh backup directory')
edits={}
def edit(relative,old,new):
 path=root/relative;data=edits.get(path,path.read_bytes());old=old.encode();new=new.encode()
 # Preserve the checked file's newline convention.
 if b'\r\n' in data:old=old.replace(b'\n',b'\r\n');new=new.replace(b'\n',b'\r\n')
 if new in data:return
 if data.count(old)!=1:raise RuntimeError('Integration anchor mismatch: '+relative+' '+repr(old[:90]))
 edits[path]=data.replace(old,new)
edit('src/common/sql.cpp','\tint32 keepalive;\n};','\tint32 keepalive;\n};\n\n#include <custom/native_stylist_sql.inc>')
edit('src/map/clif.cpp','static bool clif_session_isValid( const map_session_data* sd );','static bool clif_session_isValid( const map_session_data* sd );\n\n#include <custom/native_stylist.inc>')
# Upgrade the old pre-accept hook before installing the world-ready ordering.
auth_path=root/'src/map/clif.cpp'
auth_data=edits.get(auth_path,auth_path.read_bytes())
auth_nl=b'\r\n' if b'\r\n' in auth_data else b'\n'
auth_start=auth_data.index(b'void clif_authok( const map_session_data* sd )')
auth_end=auth_data.index(b'void clif_authrefuse',auth_start)
auth_part=auth_data[auth_start:auth_end]
auth_hook=b'\tnative_stylist::sync(const_cast<map_session_data*>(sd));'+auth_nl
auth_send=b'\tclif_send(&packet, sizeof(packet), sd, SELF);'+auth_nl
if auth_send+auth_hook not in auth_part:
 if auth_part.count(auth_hook)>1 or auth_part.count(auth_send)!=1:raise RuntimeError('Stylist auth ordering anchor mismatch')
 auth_part=auth_part.replace(auth_hook,b'')
 auth_part=auth_part.replace(auth_send,auth_send+auth_hook,1)
 edits[auth_path]=auth_data[:auth_start]+auth_part+auth_data[auth_end:]
if b'parseable_packet(0x0D50, 36, native_stylist::parse, 0);' in (root/'src/map/clif_packetdb.hpp').read_bytes():
 edit('src/map/clif_packetdb.hpp','parseable_packet(0x0D50, 36, native_stylist::parse, 0);','parseable_packet(0x0D50, 184, native_stylist::parse, 0);')
else:
 edit('src/map/clif_packetdb.hpp','#endif /* CLIF_PACKETDB_HPP */','\tparseable_packet(0x0D50, 184, native_stylist::parse, 0);\n\n#endif /* CLIF_PACKETDB_HPP */')
packet_bound='MAX_PACKET_DB  = 0xD52,' if b'MAX_PACKET_DB  = 0xD52,' in (root/'src/map/clif.hpp').read_bytes() else 'MAX_PACKET_DB  = 0xCFF,'
edit('src/map/clif.hpp',packet_bound,'MAX_PACKET_DB  = 0xD53,')
edit('src/map/clif.cpp','#include "clif_shuffle.hpp"','#include "clif_shuffle.hpp"\n\n\tif (packet_db[0x0D50].len != sizeof(ns::Request) || packet_db[0x0D50].func != native_stylist::parse) {\n\t\tShowFatalError("Native Stylist: request packet registration is invalid.\\n");\n\t\texit(EXIT_FAILURE);\n\t}\n\tShowStatus("Native Stylist: request 0x0D50 registered, %u bytes.\\n", static_cast<unsigned>(sizeof(ns::Request)));')
edit('src/custom/atcommand.inc','//ACMD_FUNC(newcommand)','ACMD_FUNC(nativestylist)\n{\n\treturn native_stylist::open(sd) ? 0 : -1;\n}\n\n//ACMD_FUNC(newcommand)')
edit('src/map/atcommand.cpp','#include "vending.hpp"','#include "vending.hpp"\n#include <custom/native_stylist.hpp>')
edit('src/custom/atcommand_def.inc','//ACMD_DEF(newcommand),','ACMD_DEF(nativestylist),\n//ACMD_DEF(newcommand),')
for file in ('src/map/pc.cpp','src/map/status.cpp'):
 edit(file,'#include "pc.hpp"','#include "pc.hpp"\n#include <custom/native_stylist.hpp>')
 for old in ('cap_value(sd->status.hair_color,MIN_HAIR_COLOR,MAX_HAIR_COLOR)','cap_value(sd->status.clothes_color,MIN_CLOTH_COLOR,MAX_CLOTH_COLOR)','cap_value(sd->status.hair_color, MIN_HAIR_COLOR, MAX_HAIR_COLOR)','cap_value(sd->status.clothes_color, MIN_CLOTH_COLOR, MAX_CLOTH_COLOR)','cap_value(sd->status.clothes_color,0,battle_config.max_cloth_color)'):
  if old.encode() in edits.get(root/file,(root/file).read_bytes()):edit(file,old,old.replace('cap_value','native_stylist::clamp_palette'))
 if file.endswith('pc.cpp'):
  edit(file,'val = cap_value(val, MIN_HAIR_COLOR, MAX_HAIR_COLOR);','val = native_stylist::clamp_palette(val, MIN_HAIR_COLOR, MAX_HAIR_COLOR);')
  edit(file,'val = cap_value(val, MIN_CLOTH_COLOR, MAX_CLOTH_COLOR);','val = native_stylist::clamp_palette(val, MIN_CLOTH_COLOR, MAX_CLOTH_COLOR);')
 for old in ('cap_value(sd->status.hair,MIN_HAIR_STYLE,MAX_HAIR_STYLE)','cap_value(sd->status.hair, MIN_HAIR_STYLE, MAX_HAIR_STYLE)'):
  if old.encode() in edits.get(root/file,(root/file).read_bytes()):edit(file,old,old.replace('cap_value(','native_stylist::clamp_hair(sd, '))
 if file.endswith('pc.cpp'):edit(file,'val = cap_value(val, MIN_HAIR_STYLE, MAX_HAIR_STYLE);','val = native_stylist::clamp_hair(sd, val, MIN_HAIR_STYLE, MAX_HAIR_STYLE);')
edit('src/char/char.cpp','#include "char.hpp"','#include "char.hpp"\n#include <custom/native_stylist_protocol.hpp>')
edit('src/char/char_clif.cpp','using namespace rathena;','using namespace rathena;\n\n#include <custom/native_stylist_select.inc>')
edit('src/char/char_clif.cpp','\tp->packetLength += char_mmo_chars_fromsql( sd, p->characters );','\tuint8 count = 0;\n\tp->packetLength += char_mmo_chars_fromsql( sd, p->characters, &count );\n\tnative_stylist_select::send(fd, p->characters, count);')
edit('src/char/char_clif.cpp','\tp->packetLength += char_mmo_chars_fromsql( sd, p->characters, &count );\n\n\tsocket_send( fd, p );\n\n\t// This is something special Gravity came up with.','\tp->packetLength += char_mmo_chars_fromsql( sd, p->characters, &count );\n\tnative_stylist_select::send(fd, p->characters, count);\n\n\tsocket_send( fd, p );\n\n\t// This is something special Gravity came up with.')
# Keep stored/base values ordinary. The char-list helper overlays transient RGB IDs.
edit('src/char/char.cpp','info.headpalette = p.hair_color;','info.headpalette = ns::Reserved(p.hair_color) ? 0 : p.hair_color;')
edit('src/char/char.cpp','info.bodypalette = p.clothes_color;','info.bodypalette = ns::Reserved(p.clothes_color) ? 0 : p.clothes_color;')
edit('src/char/char.cpp','info.hairColor = (uint8)u16min( p.hair_color, UINT8_MAX );','info.hairColor = ns::Reserved(p.hair_color) ? 0 : (uint8)u16min( p.hair_color, UINT8_MAX );')
edit('src/map/clif.cpp','\t// prevent leaking the presence of GM-hidden objects','\tif (sd) {\n\t\tval = native_stylist::view(sd, type, val);\n\t\tif (vd && (type == LOOK_HAIR_COLOR || type == LOOK_CLOTHES_COLOR || type == LOOK_HEAD_TOP || type == LOOK_HEAD_MID || type == LOOK_HEAD_BOTTOM || type == LOOK_ROBE))\n\t\t\tvd->look[type] = val;\n\t}\n\n\t// prevent leaking the presence of GM-hidden objects')
for signature in ('static void clif_set_unit_idle( const block_list* bl, bool walking, send_target target, const block_list* tbl ){','static void clif_spawn_unit( const block_list* bl, enum send_target target ){'):
 old_hook=signature+'\n\tif (auto player = BL_CAST(BL_PC, bl)) native_stylist::apply_view(player);'
 if old_hook.encode() in (root/'src/map/clif.cpp').read_bytes():
  edit('src/map/clif.cpp',old_hook,signature+'\n\tif (auto player = BL_CAST(BL_PC, bl)) native_stylist::apply_view(const_cast<map_session_data*>(player));')
 else:
  edit('src/map/clif.cpp',signature,signature+'\n\tif (auto player = BL_CAST(BL_PC, bl)) native_stylist::apply_view(const_cast<map_session_data*>(player));')
edit('src/map/pc.cpp','\tif (head_low != sd->status.head_bottom)','\tnative_stylist::refresh_views(sd);\n\n\tif (head_low != sd->status.head_bottom)')
edit('src/map/clif.cpp','\tclif_spawn(sd);\n\n\t// Party','\tclif_spawn(sd);\n\tnative_stylist::map_loaded(sd);\n\n\t// Party')
edit('src/map/status.cpp','\t\t\t\tsd->vd.look[LOOK_ROBE] = sd->status.robe;','\t\t\t\tsd->vd.look[LOOK_ROBE] = sd->status.robe;\n\t\t\t\tnative_stylist::apply_view(sd);')
edit('src/map/unit.cpp','#include "pc.hpp"','#include "pc.hpp"\n#include <custom/native_stylist.hpp>')
edit('src/map/unit.cpp','void unit_free_pc(map_session_data *sd)\n{','void unit_free_pc(map_session_data *sd)\n{\n\tnative_stylist::quit(sd);')
for name in ('native_stylist.hpp','native_stylist_hairs.hpp'):edits[root/'src/custom'/name]=(kit/'server'/name).read_bytes()
edits[root/'src/custom/native_stylist_sql.inc']=(kit/'server/native_stylist_sql.inc').read_bytes()
edits[root/'src/custom/native_stylist.inc']=(kit/'server/native_stylist_v2.inc').read_bytes()
edits[root/'src/custom/native_stylist_protocol.hpp']=(kit/'client/protocol.hpp').read_bytes()
edits[root/'src/custom/native_stylist_select.inc']=(kit/'server/native_stylist_select.inc').read_bytes()
edits[root/'src/custom/native_stylist_storage.hpp']=storage_header(root,a.inter_config)
edits[root/'src/custom/shading.hpp']=(kit/'client/shading.hpp').read_bytes()
backup.mkdir(parents=True);records=[];changes=[]
for path,data in edits.items():
 relative=path.relative_to(root);original=path.read_bytes() if path.exists() else None
 if original==data:continue
 if original is not None:
  saved=backup/relative;saved.parent.mkdir(parents=True,exist_ok=True)
  if saved.exists():raise RuntimeError('Backup exists: '+str(saved))
  saved.write_bytes(original)
  if hashlib.sha256(saved.read_bytes()).digest()!=hashlib.sha256(original).digest():raise RuntimeError('Backup failed')
 records.append(dict(path=str(relative),before=hashlib.sha256(original).hexdigest() if original is not None else None,after=hashlib.sha256(data).hexdigest()))
 changes.append((path,data,original))
(backup/'manifest.json').write_text(json.dumps(records,indent=2))
attempted=[]
try:
 for path,data,original in changes:
  attempted.append((path,original));path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
  if path.read_bytes()!=data:raise RuntimeError('Installation write verification failed: '+str(path))
except BaseException as error:
 failed=[]
 for path,original in reversed(attempted):
  try:
   if original is None:path.unlink(missing_ok=True)
   else:
    path.write_bytes(original)
    if path.read_bytes()!=original:raise RuntimeError('Recovery verification failed')
  except BaseException:failed.append(str(path))
 if failed:raise RuntimeError('Installation failed. Restore these files from '+str(backup)+': '+', '.join(failed)) from error
 raise
print(json.dumps(records,indent=2))
