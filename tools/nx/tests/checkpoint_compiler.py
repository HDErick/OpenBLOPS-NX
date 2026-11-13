"""Locate the native x86 compiler environment without exposing its variables."""
from pathlib import Path
import os,subprocess

def msvc32():
    vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
    install=subprocess.check_output([str(vswhere),'-latest','-products','*','-property','installationPath'],text=True).strip()
    vcvars=Path(install)/'VC/Auxiliary/Build/vcvars32.bat'
    output=subprocess.check_output(f'call "{vcvars}" >nul && set',shell=True,text=True,errors='replace')
    env={}
    for line in output.splitlines():
     if '=' in line:
      key,value=line.split('=',1)
      if key:env[key]=value
    native_path=next(v for k,v in env.items() if k.upper()=='PATH')
    cl=next((str(Path(d.replace('\\','/'))/'cl.exe') for d in native_path.split(';') if (Path(d.replace('\\','/'))/'cl.exe').is_file()),None)
    assert cl,'MSVC x86 compiler unavailable'
    return cl,env
