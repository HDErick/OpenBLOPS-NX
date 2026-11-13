"""Prepare pinned Sarek sources after running prepare-dxvk.py."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT/'build-nx/deps/dxvk-sarek'
COMMIT = '37f397e142b977a343e920dbc4c7bf7ed2c63a81'
URL = 'https://github.com/pythonlover02/dxvk-sarek.git'
SUBMODULES = {
    'include/spirv': 'ce9dfb01496073a02d74581ae909384763b41ff8',
    'include/vulkan': 'afe9eb980aa928a66d1c9c06f38c55dd59868720',
}
def git(*args):
    return subprocess.check_output(['git', '-C', str(SOURCE), *args], text=True).strip()

if not SOURCE.exists():
    SOURCE.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git','clone','--depth','1','--branch','v1.13.0',
                    '--recurse-submodules',URL,str(SOURCE)],check=True)
assert (SOURCE/'.git').exists(), 'Expected the Sarek dependency checkout, not the parent repository'
assert git('rev-parse','HEAD') == COMMIT, 'Unexpected Sarek revision; refusing to overwrite it'
for name, commit in SUBMODULES.items():
    actual=subprocess.check_output(['git','-C',str(SOURCE/name),'rev-parse','HEAD'],text=True).strip()
    assert actual == commit, f'Unexpected {name} revision'
base=json.loads((SOURCE.parent/'renderer-lock.json').read_text())
assert base['dxvk']=='b1a1c99ab52b687cf950d62c88bc2fa316b41663'
assert (SOURCE.parent/'dxvk/include/native/directx/d3d9.h').is_file()
patch=ROOT/'tools/nx/patch-dxvk-sarek.py'
subprocess.run([sys.executable,str(patch),str(SOURCE)],check=True)
record={
    'renderer':'DXVK Sarek 1.13.0', 'repository':URL, 'commit':COMMIT,
    'submodules':SUBMODULES, 'mesa_sha256':base['mesa_sha256'],
    'portable_com_declarations':{'dxvk_commit':base['dxvk'],
                                'directx_headers_commit':'9df86f2341616ef1888ae59919feaa6d4fad693d'},
    'patch_script_sha256':hashlib.sha256(patch.read_bytes()).hexdigest(),
}
(SOURCE.parent/'sarek-renderer-lock.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps(record,indent=2))
