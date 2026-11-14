"""Prepare pinned DXVK sources and the Mesa SDK.

Usage: python3 tools/nx/prepare-dxvk.py --mesa-zip <sdk.zip>
       --dxvk-source <clean b1a1c99 DXVK checkout with initialized submodules>
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import zipfile

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mesa-zip',type=Path,required=True)
parser.add_argument('--dxvk-source',type=Path,required=True)
args=parser.parse_args()
root=Path(__file__).resolve().parents[2]
deps=root/'build-nx/deps'
commit='b1a1c99ab52b687cf950d62c88bc2fa316b41663'
sdk_hash='6c919eb1bbb7df5729940b5b4ecaa5e0daaa05bd06ebde9f5b676ce6c3eff04b'
mesa=deps/'mesa-26.2.7'
vulkan_archive=mesa/'opt/devkitpro/portlibs/switch/lib/libvulkan.a'
def git(*params):
    return subprocess.check_output(['git','-C',str(args.dxvk_source),*params],text=True).strip()
assert git('rev-parse','HEAD')==commit, 'DXVK checkout is not the pinned version'
assert not git('status','--porcelain','--untracked-files=no'), 'Use a clean dependency checkout'
modules=git('submodule','status','--recursive').splitlines()
assert modules and all(not row.startswith(('-', '+', 'U')) for row in modules), 'Initialize pinned DXVK submodules'
with args.mesa_zip.open('rb') as stream:
    assert hashlib.file_digest(stream,'sha256').hexdigest()==sdk_hash, 'Unexpected Mesa SDK archive'
deps.mkdir(parents=True,exist_ok=True)
if not (deps/'dxvk').exists():
    shutil.copytree(args.dxvk_source,deps/'dxvk',ignore=shutil.ignore_patterns('.git','__pycache__'))
if not vulkan_archive.is_file():
    target=mesa.resolve()
    with zipfile.ZipFile(args.mesa_zip) as archive:
        for info in archive.infolist():
            path=(target/info.filename).resolve()
            assert path.is_relative_to(target), 'Unsafe ZIP member'
        archive.extractall(target)
assert vulkan_archive.is_file(), 'Unexpected Mesa SDK layout'
(deps/'renderer-lock.json').write_text(json.dumps({'dxvk':commit,'submodules':modules,'mesa_sha256':sdk_hash},indent=2)+'\n')
print('Pinned local DXVK source and Mesa SDK ready')
