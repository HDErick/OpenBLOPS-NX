"""Build the existing FastFile converter for the browser using an active emsdk."""
import os
from pathlib import Path
import subprocess

installer = Path(__file__).resolve().parent
output = installer / "generated"
output.mkdir(exist_ok=True)
compiler = os.environ.get("EMXX", "em++")
flags = [
    "-O2", "-std=c++17", "-sUSE_ZLIB=1", "-sMODULARIZE=1",
    "-sEXPORT_ES6=1", "-sENVIRONMENT=worker,node", "-sINVOKE_RUN=0",
    "-sEXPORTED_RUNTIME_METHODS=FS,callMain", "-sFORCE_FILESYSTEM=1",
    "-sALLOW_MEMORY_GROWTH=1", "-sSTACK_SIZE=16777216", "-sINITIAL_MEMORY=33554432",
    "-sMAXIMUM_MEMORY=2147483648", "-sDISABLE_EXCEPTION_CATCHING=0",
]
command = [compiler]
if Path(compiler).suffix == ".py":
    import sys
    command = [sys.executable, compiler]
subprocess.run(command + [
    str(installer.parent / "tools" / "ffconv" / "convert.cpp"),
    *flags, "-o", str(output / "convert.mjs"),
], check=True)
