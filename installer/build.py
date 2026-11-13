"""Build the browser tools with an active emsdk: the FastFile converter and kbzpack.

kbzpack compresses each converted zone (zstd) for the Switch; it needs the zstd
sources in build-nx/deps/zstd:
    git clone --depth 1 --branch v1.5.7 https://github.com/facebook/zstd.git build-nx/deps/zstd
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

installer = Path(__file__).resolve().parent
root = installer.parent
output = installer / "generated"
output.mkdir(exist_ok=True)
compiler = os.environ.get("EMXX", "em++")


def tool(name):
    """em++ or emcc, run through Python when given as a .py script."""
    path = compiler if name == "em++" else str(Path(compiler).with_name(Path(compiler).name.replace("em++", name)))
    return [sys.executable, path] if Path(path).suffix == ".py" else [path]


common = [
    "-O2", "-sMODULARIZE=1", "-sEXPORT_ES6=1", "-sENVIRONMENT=worker,node", "-sINVOKE_RUN=0",
    "-sEXPORTED_RUNTIME_METHODS=FS,callMain", "-sFORCE_FILESYSTEM=1",
    "-sALLOW_MEMORY_GROWTH=1", "-sSTACK_SIZE=16777216", "-sINITIAL_MEMORY=33554432",
    "-sMAXIMUM_MEMORY=2147483648",
]
subprocess.run(tool("em++") + [
    str(root / "tools" / "ffconv" / "convert.cpp"),
    "-std=c++17", "-sUSE_ZLIB=1", "-sDISABLE_EXCEPTION_CATCHING=0",
    *common, "-o", str(output / "convert.mjs"),
], check=True)

zstd = root / "build-nx" / "deps" / "zstd" / "lib"
if not (zstd / "zstd.h").is_file():
    sys.exit("kbzpack needs the zstd sources in build-nx/deps/zstd (see this file's docstring).")
sources = [str(p) for part in ("common", "compress", "decompress") for p in sorted((zstd / part).glob("*.c"))]
with tempfile.TemporaryDirectory() as objects:
    subprocess.run(tool("emcc") + ["-O2", "-c", "-DZSTD_DISABLE_ASM", "-DZSTD_MULTITHREAD=0", f"-I{zstd}", *sources],
                   check=True, cwd=objects)
    subprocess.run(tool("em++") + [
        str(root / "tools" / "ffconv" / "kbzpack.cpp"), f"-I{zstd}", "-std=c++17",
        *sorted(str(p) for p in Path(objects).glob("*.o")),
        *common, "-o", str(output / "kbzpack.mjs"),
    ], check=True)
print("built", output / "convert.mjs", "and", output / "kbzpack.mjs")
