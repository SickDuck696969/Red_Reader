param(
    [Parameter(Mandatory=$true)][string]$ZigPath,
    [Parameter(Mandatory=$true)][string]$ArchiveIncludeDir,
    [Parameter(Mandatory=$true)][string]$ArchiveLibDir,
    [Parameter(Mandatory=$true)][string]$RuntimeRoot,
    [string]$ExpatIncludeDir='C:/devkitPro/portlibs/switch/include',
    [string]$EpubPath=''
)
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
$previousPath=$env:PATH
try {
    python tests/generate_asset_fixtures.py
    if($LASTEXITCODE -ne 0){throw 'Fixture generation failed'}
    New-Item -ItemType Directory -Force build/asset-include | Out-Null
    Get-ChildItem -LiteralPath $ExpatIncludeDir -Filter 'expat*.h' | Copy-Item -Destination build/asset-include -Force
    & $ZigPath c++ -target x86_64-windows-gnu -std=c++17 -DLIBARCHIVE_STATIC -DXML_STATIC -Iinclude -Ibuild/asset-include "-I$ArchiveIncludeDir" tests/asset_tests.cpp source/document_assets.cpp source/model.cpp "-L$ArchiveLibDir" "-L$RuntimeRoot/lib" -larchive -llzma -llz4 -lzstd -lbz2 -lz -lcrypto -lnettle -lexpat -llzo2 -liconv -lcharset -lbcrypt -lcrypt32 -lws2_32 -o build/asset_tests.exe
    if($LASTEXITCODE -ne 0){throw 'Archive test compilation failed'}
    $env:PATH="$RuntimeRoot/bin;"+$env:PATH
    if($EpubPath){ & ./build/asset_tests.exe $EpubPath } else { & ./build/asset_tests.exe }
    if($LASTEXITCODE -ne 0){throw 'Archive tests failed'}
} finally { $env:PATH=$previousPath; Pop-Location }
