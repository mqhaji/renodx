param([string]$SdkDirectory = '', [string]$BuildDirectory = '')

# Run from an x64 Visual Studio developer shell, as with the game addon.
# This builds the pinned SDK only. Deployment remains a separate, game-closed step.
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../..'))
$interfaces = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../third_party/nrd'))
if (!$SdkDirectory) { $SdkDirectory = Join-Path $repository 'build/dependencies/mgsv-nrd' }
if (!(Test-Path -LiteralPath (Join-Path $SdkDirectory 'Include/NRD.h'))) {
    throw 'Pass -SdkDirectory with a complete NRD 4.18.0 checkout. The addon itself uses its bundled interfaces.'
}
$SdkDirectory = (Resolve-Path -LiteralPath $SdkDirectory).Path
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repository 'build/mgsv-nrd' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$output = Join-Path $BuildDirectory 'bin'

# Check the public interface before building an independently obtained SDK.
# A matching version number alone does not prove the same data layout.
foreach ($file in @('Include/NRD.h', 'Include/NRDDescs.h', 'Include/NRDSettings.h', 'Shaders/NRD.hlsli')) {
    $source = Join-Path $SdkDirectory $file
    $bundled = Join-Path $interfaces (Split-Path $file -Leaf)
    if ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $bundled).Hash) {
        throw "NRD SDK interface differs from the bundled version: $file"
    }
}

& cmake -S $SdkDirectory -B $BuildDirectory -G Ninja -DCMAKE_BUILD_TYPE=Release `
    -DNRD_STATIC_LIBRARY=OFF -DREBLUR_PERFORMANCE_MODE=OFF `
    -DNRD_NRI=OFF -DNRD_EMBEDS_DXIL_SHADERS=ON -DNRD_EMBEDS_DXBC_SHADERS=OFF `
    -DNRD_EMBEDS_SPIRV_SHADERS=OFF -DNRD_NORMAL_ENCODING=2 -DNRD_ROUGHNESS_ENCODING=1 `
    "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=$output" "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=$output" `
    "-DDXC_PATH=$(Join-Path $repository 'bin/dxc.exe')"
if ($LASTEXITCODE) { throw 'NRD configuration failed.' }
if ((Get-FileHash -LiteralPath (Join-Path $SdkDirectory 'Shaders/NRDConfig.hlsli')).Hash -ne
    (Get-FileHash -LiteralPath (Join-Path $interfaces 'NRDConfig.hlsli')).Hash) {
    throw 'NRD shader configuration differs from the bundled packing. Use a clean SDK build directory.'
}
& cmake --build $BuildDirectory --config Release --target NRD
if ($LASTEXITCODE) { throw 'NRD build failed.' }
Copy-Item -LiteralPath (Join-Path $interfaces 'LICENSE.txt') -Destination (Join-Path $output 'NRD_LICENSE.txt')

Write-Output "NRD.dll: $(Join-Path $output 'NRD.dll')"
Write-Output "License: $(Join-Path $output 'NRD_LICENSE.txt')"
