"""Test KBZ image ownership and delayed overrides using retail fixtures.

Run in MSYS2: python3 tools/nx/tests/test-kbz-image-overrides.py <kbz-dir> <output-dir>
"""
import argparse
from pathlib import Path
import mmap
import re
import struct
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('zones', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--loader', type=Path)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
loader = (args.loader or root/'src/nx/nx_kbz.cpp').resolve()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)


def retail_image(zone, wanted):
    with (args.zones/(zone+'.kbz')).open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as data:
        assert data[:4] == b'KBZ1'
        nblocks, = struct.unpack_from('<I', data, 8)
        sizes = struct.unpack_from(f'<{nblocks}I', data, 12)
        nreloc, nasset = struct.unpack_from('<II', data, 12+4*nblocks)
        pos = 20+4*nblocks
        bases = []
        for i, size in enumerate(sizes):
            bases.append(None if i == 1 else pos)
            if i != 1:
                pos += size
        reloc = {}
        for sb, so, tb, to in struct.iter_unpack('<BIBI', data[pos:pos+10*nreloc]):
            if bases[sb] is not None and bases[tb] is not None:
                reloc[bases[sb]+so] = bases[tb]+to
        pos += 10*nreloc
        for kind, block, offset in struct.iter_unpack('<IBI', data[pos:pos+9*nasset]):
            if kind != 8:
                continue
            image = bases[block]+offset
            name = reloc[image+56]
            if data[name:data.find(b'\0', name)].decode() != wanted:
                continue
            definition = reloc.get(image)
            pixels = None
            if definition is not None:
                size, = struct.unpack_from('<I', data, definition+8)
                pixels = data[definition:definition+12+size]
                assert len(pixels) == 12+size
            return data[image:image+72], pixels
    raise AssertionError(f'{zone}: missing {wanted}')


def fixture(name, entries):
    block = bytearray()
    relocs, assets, definitions = [], [], {}
    for label, image, pixels, alias in entries:
        block.extend(b'\0' * (-len(block) % 8))
        offset = len(block)
        record = bytearray(image)
        for field in (0, 40, 56):
            struct.pack_into('<Q', record, field, 0)
        block.extend(record)
        assets.append((8, 0, offset))
        relocs.append((0, offset+56, 0, len(block)))
        block.extend(label.encode()+b'\0')
        if pixels is not None:
            if alias not in definitions:
                block.extend(b'\0' * (-len(block) % 4))
                definitions[alias] = len(block)
                block.extend(pixels)
            relocs.append((0, offset, 0, definitions[alias]))
    blob = b'KBZ1'+struct.pack('<IIIIII', 1, 2, len(block), 0, len(relocs), len(assets))
    blob += block+b''.join(struct.pack('<BIBI', *r) for r in relocs)
    blob += b''.join(struct.pack('<IBI', *a) for a in assets)
    (output/(name+'.kbz')).write_bytes(blob)


for zone in ('patch', 'frontend', 'common_zombie', 'zombie_theater'):
    name = (',' if zone == 'zombie_theater' else '')+'skin_pore_detail_test'
    image, definition = retail_image(zone, name)
    fixture(zone, [(name, image, definition, 0)])
image, definition = retail_image('common_zombie', 'skin_pore_detail_test')
assert struct.unpack_from('<I', definition, 4)[0] == 0x35545844
fixture('aliases', [(n, image, definition, 0) for n in ('alias_first', 'alias_second')])
fixture('disabled', [('disabled', image, definition, 0)])

# Compile the loader against the engine test double.
include = output/'include'
source = loader.read_text()
for header in re.findall(r'^#include <([^>]+/[^>]+)>', source, re.M):
    stub = include/header
    stub.parent.mkdir(parents=True, exist_ok=True)
    stub.write_text('// Engine boundary provided by kbz-image-shim.h\n')
exe = output/'kbz-image-overrides.exe'
flags = ['-std=c++20', '-O2', '-Wall', '-Wextra', '-DKISAK_NX', '-I'+str(include),
         '-DNX_KBZ_SOURCE="'+str(loader)+'"']
if 'void NX_KBZ_ValidateImages()' in source:
    flags.append('-DNX_KBZ_HAS_IMAGE_AUDIT')
if 'NxKbzStorage' in source:
    flags.extend(['-DNX_KBZ_HAS_STORAGE', str(loader.with_name('nx_kbz_storage.cpp'))])
subprocess.run([args.compiler, *flags, str(Path(__file__).with_name('kbz-image-overrides.cpp')),
                '-o', str(exe)], check=True)
subprocess.run([str(exe), str(output)], check=True)
