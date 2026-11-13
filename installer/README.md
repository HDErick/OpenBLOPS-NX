# OpenBLOPS-NX Switch installer

Open the installer in desktop Chrome or Edge. Choose the directory containing
your PC copy's `main/`, `zone/`, and `localization.txt`, then choose your SD card
or a staging directory. The installer writes `switch/OpenBLOPS-NX/` with both
Switch executables, the original game data, and converted `kbz/` zones.

The browser reads and converts game data locally. It attempts every `.ff` in the
selected installation, including installed DLC and localized zones. Check the
conversion report for unsupported or damaged files. A reported failure can
affect the map or mode that uses that zone.

Hold **R** while opening an installed Switch game to enter the Homebrew Menu in
application mode. Launch **OpenBLOPS-NX** for Campaign or Zombies or
**OpenBLOPS-NX-MP** for Multiplayer.

## Run locally

Install Python, Node.js 22 or newer, and GitHub CLI. Run `gh auth login` with
an account that can read the repository. If the browser converter hasn't
been built yet, activate [Emscripten SDK](https://github.com/emscripten-core/emsdk)
4.0.20 and run `python installer/build.py` from the repository root.

```powershell
python installer/serve.py
```

Open `http://127.0.0.1:8000/` in Chrome or Edge. The launcher downloads
`OpenBLOPS-NX.zip` from the
[latest GitHub release](https://github.com/HDErick/OpenBLOPS-NX/releases/latest)
and checks both NROs before serving the installer. The browser uses that ZIP
automatically.

## GitHub Pages

Configure Pages to use GitHub Actions, then run **Publish Switch installer**.
The workflow builds the converter, validates the latest release ZIP, and serves
it alongside the installer from the same origin. Publishing or editing a stable
release queues a fresh deployment from `main`. Visitors get both NROs
automatically when they install. The manual ZIP picker is also available for
local use or to select a different release.

After replacing only the ZIP on an existing release, run the workflow manually
to refresh the site's copy.
