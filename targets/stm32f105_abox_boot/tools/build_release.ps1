[CmdletBinding()]
param([ValidateSet('Debug', 'Release')] [string] $Configuration = 'Release')

$ErrorActionPreference = 'Stop'
$targetRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $targetRoot ("build/{0}" -f $Configuration)
$toolchain = Join-Path $targetRoot 'cmake/gcc-arm-none-eabi.cmake'
$dist = Join-Path $targetRoot 'dist'

& cmake -S $targetRoot -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DCMAKE_BUILD_TYPE=$Configuration" | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'ABox Boot configure failed' }
& cmake --build $build --parallel 4 | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'ABox Boot build failed' }

$built = Join-Path $build 'ABox_Boot.bin'
if (-not (Test-Path -LiteralPath $built -PathType Leaf)) { throw "Missing Boot artifact: $built" }
& python (Join-Path $PSScriptRoot 'verify_boot_artifact.py') $built | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'ABox Boot artifact validation failed' }

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$output = Join-Path $dist 'ABox_Boot.bin'
Copy-Item -LiteralPath $built -Destination $output -Force
$manifest = [ordered]@{
    manifest_version = 1
    target = 'stm32f105_abox_boot'
    mcu = 'STM32F105RCT6'
    hardware_contract = 'abox_stm32f105_ec800_v1'
    boot_version = 'abox-boot-2.3.0'
    load_address = '0x08000000'
    app_address = '0x08008000'
    boot_state_pages = @('0x0803E800', '0x0803F000')
    config_page = '0x0803F800'
    size = (Get-Item -LiteralPath $output).Length
    crc32 = (& python -c "import pathlib,zlib;print(f'{zlib.crc32(pathlib.Path(r'''$output''').read_bytes())&0xffffffff:08X}')").Trim()
    sha256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $dist 'manifest.json') -Encoding utf8
Write-Host "Frozen ABox Boot artifact written to $output"
