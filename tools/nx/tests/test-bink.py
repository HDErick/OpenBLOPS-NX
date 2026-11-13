"""Compare the Horizon Bink implementation against FFmpeg using local movie fixtures."""
import argparse
from pathlib import Path
import subprocess
import array

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ffmpeg-source', type=Path, required=True)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
parser.add_argument('--memory', action='store_true')
parser.add_argument('movies', type=Path, nargs='+')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root/'build-nx/bink-tests'
include = output/'include/binklib'
include.mkdir(parents=True, exist_ok=True)
(include/'bink.h').write_text('''#define __RADLINUX__
#define __int64 long long
#define __RADWIN__
#define __RADNT__
#define __RAD32__
#define __RAD64__
#define __RADX64__
#include_next <binklib/bink.h>
''')
source = args.ffmpeg_source.resolve()
exe = output/'bink.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Wno-attributes',
                '-I'+str(include.parent), '-I'+str(root/'src/nx'), '-I'+str(root/'src'), '-I'+str(source),
                str(root/'tools/nx/tests/bink.cpp'), str(root/'src/nx/nx_bink.cpp'),
                str(root/'src/nx/nx_bink_audio.cpp'), str(source/'libavcodec/libavcodec.a'),
                str(source/'libswresample/libswresample.a'), str(source/'libavutil/libavutil.a'),
                '-lwinpthread', '-lbcrypt', '-lm', '-o', str(exe)], check=True)
for movie in args.movies:
    prefix = output/movie.stem
    with movie.open('rb') as stream:
        header = stream.read(44)
        track_count = int.from_bytes(header[40:44], 'little')
        if header[3] == ord('k'):
            stream.read(4)
        track_headers = stream.read(track_count * 12)
    track_ids = [int.from_bytes(track_headers[track_count * 8 + i * 4:track_count * 8 + i * 4 + 4], 'little')
                 for i in range(track_count)]
    subprocess.run([str(exe), str(movie), str(prefix), str(track_ids[0] if track_ids else 0),
                    'memory' if args.memory else 'file', '2'], check=True)
    reference = prefix.with_suffix('.reference.md5')
    subprocess.run([str(source/'ffmpeg.exe'), '-v', 'error', '-y', '-i', str(movie), '-map', '0:v:0',
                    '-f', 'framemd5', str(reference)], check=True)
    expected = [line.split(',')[-1].strip() for line in reference.read_text().splitlines() if not line.startswith('#')]
    actual = prefix.with_suffix('.md5').read_text().splitlines()
    assert actual == expected * 2, f'{movie}: video differs from FFmpeg'
    if track_count:
        for track in range(min(2, track_count)):
            pcm_reference = output/f'{movie.stem}.{track}.reference.s16'
            subprocess.run([str(source/'ffmpeg.exe'), '-v', 'error', '-y', '-i', str(movie), '-map', f'0:a:{track}',
                            '-ac', '2', '-ar', '48000', '-f', 's16le', str(pcm_reference)], check=True)
            if track:
                subprocess.run([str(exe), str(movie), str(prefix), str(track_ids[track]), 'memory', '1'], check=True)
            a, b = array.array('h'), array.array('h')
            a.frombytes(prefix.with_suffix('.s16').read_bytes())
            b.frombytes(pcm_reference.read_bytes())
            size = min(len(a), len(b), len(a)//2 if not track else len(a))
            difference = max(abs(x-y) for x, y in zip(a[:size], b[:size]))
            assert difference <= 1, f'{movie}: track {track} differs by {difference}'
            print(f'{movie.name}: track {track}, {size//2} stereo frames match FFmpeg (max error {difference})')
    print(f'{movie.name}: all {len(expected)} video frames match FFmpeg, including a second loop')
