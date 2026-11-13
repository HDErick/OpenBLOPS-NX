"""Check packaged Switch binaries, native renderer linkage and build hashes."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('build',type=Path)
parser.add_argument('--name',default='KisakBlack')
parser.add_argument('--nm',default='/opt/devkitpro/devkitA64/bin/aarch64-none-elf-nm.exe')
args=parser.parse_args()
elf=args.build/'KisakBlack.elf'
nro=args.build/(args.name+'.nro')
elf_data=elf.read_bytes()
assert elf_data[:5]==b'\x7fELF\x02', 'Not a 64-bit ELF'
assert struct.unpack_from('<H',elf_data,18)[0]==183, 'Not AArch64'
binary=nro.read_bytes()
assert binary[16:20]==b'NRO0', 'Missing NRO header'
image_size=struct.unpack_from('<I',binary,24)[0]
assert 0x80<=image_size<=len(binary), 'Invalid NRO size'
segments=[struct.unpack_from('<II',binary,offset) for offset in (32,40,48)]
for offset,size in segments:
    assert offset%4096==0 and size%4096==0 and offset+size<=image_size
assert binary[image_size:image_size+4]==b'ASET', 'Missing NACP/icon asset header'
symbols=subprocess.check_output([args.nm,'--defined-only','-C',str(elf)],text=True)
required=['nvk_loaderless_GetInstanceProcAddr','dxvk::D3D9DeviceEx::DrawIndexedPrimitive',
          'kisak_d3d9::IDirect3DDevice9::DrawIndexedPrimitive','NX_DxvkConfigure',
          '__wrap_pthread_create','NX_DxvkReportCounters']
required += ['NX_SarekCreateSurface', 'dxvk::DxsoModule::compile']
for symbol in required:
    assert symbol in symbols, f'Missing native renderer symbol: {symbol}'
for forbidden in ('eglCreateContext','eglSwapBuffers','glDrawElements','zink_create_screen','NX_GlRelease'):
    assert not re.search(r'\b'+forbidden+r'\b',symbols), f'Unexpected GL renderer linked: {forbidden}'
label = 'DXVK Sarek 1.13.0 37f397e'
assert label.encode() in binary, 'Renderer build label missing from NRO'
assert b'dxvk-sarek.conf' in binary and b'dxvk-sarek-1.13.0' in binary
assert not b'DXVK 3.1.1 b1a1c99' in binary, 'Wrong native backend linked'
report={'name':args.name,'architecture':'AArch64','renderer':f'Native {label} -> NVK 26.2.3',
        'nro_bytes':len(binary),'nro_sha256':hashlib.sha256(binary).hexdigest(),
        'elf_sha256':hashlib.sha256(elf_data).hexdigest(),'nro_segments':segments,
        'native_linkage_checked':True,'hardware_tested':False}
(args.build/'dxvk-build-verification.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
