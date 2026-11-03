param(
    [Parameter(Mandatory=$true)][string]$ZigPath,
    [Parameter(Mandatory=$true)][string]$SdlDllDir,
    [string]$SdlHeaderDir='C:/devkitPro/portlibs/switch/include/SDL2'
)
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    New-Item -ItemType Directory -Force build/host-include/SDL2 | Out-Null
    Get-ChildItem -LiteralPath $SdlHeaderDir -Filter '*.h' -File | Copy-Item -Destination build/host-include/SDL2 -Force
    & $ZigPath c++ -target x86_64-windows-gnu -std=c++17 -Iinclude tests/model_tests.cpp source/model.cpp -o build/model_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Model compilation failed'}
    & ./build/model_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Model tests failed'}
    $libraries=@('SDL2.dll','SDL2_ttf.dll','SDL2_image.dll') | ForEach-Object {Join-Path $SdlDllDir $_}
    & $ZigPath c++ -target x86_64-windows-gnu -std=c++17 -DRR_DESKTOP -Itests/stubs -Iinclude -Ibuild/host-include tests/desktop_tests.cpp source/model.cpp source/input.cpp source/reader.cpp source/ui.cpp @libraries -o build/desktop_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Desktop compilation failed'}
    $previousPath=$env:PATH
    try {
        $env:PATH=$SdlDllDir+';'+$env:PATH
        & ./build/desktop_tests.exe
        if($LASTEXITCODE -ne 0){throw 'Desktop tests failed'}
    } finally {$env:PATH=$previousPath}
} finally {Pop-Location}
