param(
    [string] $DirectXSdk,
    [string] $GameFolder,
    [string] $CMake = 'cmake',
    [int] $Jobs = 6
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$output = Join-Path $repo 'build-nx/PC-build'
New-Item -ItemType Directory -Path $output -Force | Out-Null

foreach ($mode in 'sp', 'mp') {
    $build = Join-Path $output $mode
    $singlePlayer = if ($mode -eq 'sp') { 'ON' } else { 'OFF' }
    $options = @('-DKISAK_DIRECT_CONNECT=ON', '-DKISAK_EXPERIMENTAL_SPLITSCREEN=ON', "-DKISAK_SP=$singlePlayer")
    if ($DirectXSdk) { $options += "-DDXSDK_DIR=$DirectXSdk" }
    & $CMake -S $repo -B $build -G 'Visual Studio 17 2022' -A Win32 @options
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed for $mode." }
    & $CMake --build $build --config Release --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw "The $mode build failed." }
}

if ($GameFolder) {
    $game = (Resolve-Path -LiteralPath $GameFolder).Path
    foreach ($name in 'main', 'zone') {
        $target = (Resolve-Path -LiteralPath (Join-Path $game $name)).Path
        $link = Join-Path $output $name
        if (Test-Path -LiteralPath $link) {
            $existing = Get-Item -LiteralPath $link -Force
            if ($existing.LinkType -ne 'Junction' -or $existing.Target -ne $target) {
                throw "Existing game data at $link needs to be moved before linking $target."
            }
        } else {
            New-Item -ItemType Junction -Path $link -Target $target | Out-Null
        }
    }
    foreach ($name in 'localization.txt', 'binkw32.dll', 'Steam.dll', 'steamclient.dll', 'tier0_s.dll', 'vpx.dll', 'vstdlib_s.dll') {
        $path = Join-Path $game $name
        if (Test-Path -LiteralPath $path) { Copy-Item -LiteralPath $path -Destination $output }
    }
    $steamApi = Join-Path $repo 'src/steam/redistributable_bin/steam_api.dll'
    if (Test-Path -LiteralPath $steamApi) { Copy-Item -LiteralPath $steamApi -Destination $output }
}

Get-Item -LiteralPath (Join-Path $output 'OpenBLOPS.exe'), (Join-Path $output 'OpenBLOPS-MP.exe')
