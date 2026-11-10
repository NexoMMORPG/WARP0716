"""Build a freestanding x86 payload with the installed Visual Studio tools."""
from pathlib import Path
import argparse,subprocess,os
p=argparse.ArgumentParser();p.add_argument('--out',required=True);p.add_argument('--base',default='0x2000000');p.add_argument('--include',action='append',default=[]);p.add_argument('--relocatable',action='store_true');p.add_argument('sources',nargs='+');a=p.parse_args()
out=Path(a.out).resolve();out.parent.mkdir(parents=True,exist_ok=True)
vs=Path(subprocess.check_output([r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe','-latest','-products','*','-requires','Microsoft.VisualStudio.Component.VC.Tools.x86.x64','-property','installationPath'],text=True).strip())
vc=max((vs/'VC/Tools/MSVC').iterdir(),key=lambda v:tuple(map(int,v.name.split('.'))));tool=vc/'bin/Hostx64/x86'
env={k:v for k,v in os.environ.items() if k.lower() not in ('path','temp','tmp','include')};env['Path']=os.environ.get('Path') or os.environ.get('PATH','');env['TEMP']=env['TMP']=str(out.parent)
objects=[]
for name in a.sources:
 source=Path(name).resolve();obj=out.parent/(source.stem+'.obj');objects.append(str(obj))
 command=[str(tool/'cl.exe'),'/nologo','/c','/O2','/Oi','/Gy','/GS-','/GR-','/Zl','/std:c++17','/W4','/WX','/wd4505','/arch:SSE2',*['/I'+str(Path(i).resolve()) for i in a.include],'/Fo'+str(obj),str(source)]
 r=subprocess.run(command,cwd=out.parent,env=env,capture_output=True,text=True);(out.parent/(source.stem+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode:raise SystemExit(r.stdout+r.stderr)
command=[str(tool/'link.exe'),'/nologo','/DLL','/NOENTRY','/NODEFAULTLIB','/MACHINE:X86','/FIXED:NO' if a.relocatable else '/FIXED','/DYNAMICBASE:NO','/SAFESEH:NO','/BASE:'+a.base,'/MERGE:.rdata=.text','/OPT:REF','/OPT:ICF','/OUT:'+str(out),'/MAP:'+str(out.with_suffix('.map')),*objects]
r=subprocess.run(command,cwd=out.parent,env=env,capture_output=True,text=True);out.with_suffix('.log').write_text(r.stdout+r.stderr)
if r.returncode:raise SystemExit(r.stdout+r.stderr)
print('MSVC x86 /W4 /WX build passed:',out)
