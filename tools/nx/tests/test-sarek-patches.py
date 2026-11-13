"""Check that the Horizon patches reproduce and apply idempotently."""
from pathlib import Path
import hashlib
import subprocess
import sys

root=Path(__file__).resolve().parents[3]
source=root/'build-nx/deps/dxvk-sarek-1.14.1'
replay=root/'build-nx/sarek-work/patch-replay'
patch=root/'tools/nx/patch-dxvk-sarek.py'
assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()=='0c0e8f6f62c7cb37dbf5d38413517efa00c08fef'
paths=subprocess.check_output(['git','-c','core.autocrlf=true','-c','core.safecrlf=false',
    '-c','core.filemode=false','-C',str(source),'diff','--ignore-submodules=all','--name-only'],text=True).splitlines()
for name in ['meson.build',*paths]:
    path=Path(name)
    assert not path.is_absolute() and '..' not in path.parts
    target=replay/path
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(subprocess.check_output(['git','-C',str(source),'show',f'HEAD:{name}']))
subprocess.run([sys.executable,str(patch),str(replay)],check=True)
swapchain=(replay/'src/d3d9/d3d9_swapchain.cpp').read_text()
recreate=swapchain.split('void D3D9SwapChainEx::RecreateSwapChain(',1)[1].split('void D3D9SwapChainEx::CreatePresenter(',1)[0]
assert 'PickPresentModes(false, presenterDesc.presentModes)' in recreate
assert 'vsync != m_vsync' not in swapchain
for name in paths:
    assert (source/name).read_text()==(replay/name).read_text(), f'Unreproducible patch: {name}'
first={name:hashlib.sha256((replay/name).read_bytes()).hexdigest() for name in paths}
subprocess.run([sys.executable,str(patch),str(replay)],check=True)
assert first=={name:hashlib.sha256((replay/name).read_bytes()).hexdigest() for name in paths}
print(f'PASS: {len(paths)} modified files reproduce from the pinned release; second application is unchanged')
