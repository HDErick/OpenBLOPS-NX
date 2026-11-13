"""Download the latest Switch release and serve the installer on loopback."""

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
from tempfile import TemporaryDirectory


INSTALLER = Path(__file__).resolve().parent
REPOSITORY = "HDErick/OpenBLOPS-NX"
ARCHIVE = "OpenBLOPS-NX.zip"


def refresh_release():
    release_dir = INSTALLER / "release"
    release_dir.mkdir(exist_ok=True)
    with TemporaryDirectory(dir=release_dir) as temporary:
        subprocess.run(
            ["gh", "release", "download", "--repo", REPOSITORY,
             "--pattern", ARCHIVE, "--dir", temporary],
            check=True,
        )
        archive = Path(temporary) / ARCHIVE
        subprocess.run(
            ["node", str(INSTALLER / "check-release.mjs"), str(archive)],
            check=True,
        )
        archive.replace(release_dir / ARCHIVE)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    if not all((INSTALLER / "generated" / name).is_file()
               for name in ("convert.mjs", "convert.wasm")):
        parser.exit(1, "Build the converter first with python installer/build.py in an active emsdk shell.\n")

    handler = partial(SimpleHTTPRequestHandler, directory=str(INSTALLER))
    with ThreadingHTTPServer(("127.0.0.1", args.port), handler) as server:
        print("Downloading and validating the latest Switch release...", flush=True)
        refresh_release()
        print(f"Open http://127.0.0.1:{server.server_port}/ in Chrome or Edge.", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    try:
        main()
    except (OSError, subprocess.CalledProcessError) as error:
        sys.exit(f"Could not start the installer: {error}")
