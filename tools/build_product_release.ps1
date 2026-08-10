[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $ProductRoot,
    [Parameter(Mandatory = $true)] [string] $AppTarget,
    [Parameter(Mandatory = $true)] [string] $BootTarget,
    [ValidateSet('Debug', 'Release')] [string] $Configuration = 'Release',
    [string] $CombinedName,
    [Parameter(Mandatory = $true)] [string] $AppLinker,
    [Parameter(Mandatory = $true)] [string] $BootLinker
)

$ErrorActionPreference = 'Stop'
$product = (Resolve-Path -LiteralPath $ProductRoot).Path
$dist = Join-Path $product 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$platform = Join-Path $product 'platform'

function Get-GitValue([string] $Repo, [string[]] $Arguments) {
    $value = (& git -C $Repo @Arguments 2>$null | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($value)) { throw "Could not read git metadata from $Repo" }
    return $value
}

function Get-GitDirty([string] $Repo) {
    $status = (& git -C $Repo status --porcelain 2>$null | Out-String).Trim()
    return -not [string]::IsNullOrWhiteSpace($status)
}

function Get-VersionLine([string] $Command, [string[]] $Arguments) {
    $output = @(& $Command @Arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0 -or $output.Count -eq 0) { throw "Could not read tool version: $Command" }
    return ([string]$output[0]).Trim()
}

function Resolve-ProductPath([string] $RelativePath) {
    $path = Join-Path $product $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing product file: $path" }
    return (Resolve-Path -LiteralPath $path).Path
}

$appLinkerPath = Resolve-ProductPath $AppLinker
$bootLinkerPath = Resolve-ProductPath $BootLinker
$validateScript = Join-Path $platform 'tools/validate_flash_layout.ps1'
if (-not (Test-Path -LiteralPath $validateScript -PathType Leaf)) { throw "Missing platform flash validator: $validateScript" }

& $validateScript -BootLinker $bootLinkerPath -AppLinker $appLinkerPath | Out-Host

$productCommit = Get-GitValue $product @('rev-parse', 'HEAD')
$platformCommit = Get-GitValue $platform @('rev-parse', 'HEAD')
$productDirty = Get-GitDirty $product
$platformDirty = Get-GitDirty $platform
$toolchain = [ordered]@{
    arm_none_eabi_gcc = Get-VersionLine 'arm-none-eabi-gcc' @('--version')
    cmake = Get-VersionLine 'cmake' @('--version')
    ninja = Get-VersionLine 'ninja' @('--version')
}

function Invoke-ProductBuild([string] $role, [string] $targetName) {
    $source = Join-Path $product $role.ToLowerInvariant()
    $toolchain = Join-Path $source 'cmake/gcc-arm-none-eabi.cmake'
    if (-not (Test-Path -LiteralPath $toolchain)) {
        throw "$role has no GCC ARM toolchain file: $toolchain"
    }

    $build = Join-Path $source ("build/ABoxUnified{0}" -f $Configuration)
    & cmake -S $source -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$toolchain" "-DCMAKE_BUILD_TYPE=$Configuration" | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$role configure failed" }
    & cmake --build $build --parallel 4 | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$role build failed" }

    $bin = Join-Path $build "$targetName.bin"
    if (-not (Test-Path -LiteralPath $bin)) { throw "Missing $role artifact: $bin" }
    return $bin
}

$appBin = Invoke-ProductBuild 'app' $AppTarget
$bootBin = Invoke-ProductBuild 'boot' $BootTarget
$appOut = Join-Path $dist "$AppTarget.bin"
$bootOut = Join-Path $dist "$BootTarget.bin"
Copy-Item -LiteralPath $appBin -Destination $appOut -Force
Copy-Item -LiteralPath $bootBin -Destination $bootOut -Force

$artifacts = @(
    [pscustomobject]@{ name = (Split-Path $appOut -Leaf); path = $appOut; size = (Get-Item $appOut).Length; sha256 = (Get-FileHash $appOut -Algorithm SHA256).Hash },
    [pscustomobject]@{ name = (Split-Path $bootOut -Leaf); path = $bootOut; size = (Get-Item $bootOut).Length; sha256 = (Get-FileHash $bootOut -Algorithm SHA256).Hash }
)

if ($CombinedName) {
    $bootBytes = [System.IO.File]::ReadAllBytes($bootOut)
    $appBytes = [System.IO.File]::ReadAllBytes($appOut)
    if ($bootBytes.Length -gt 0x8000) { throw 'Boot artifact exceeds the 32 KiB reserved region' }
    $fullBytes = New-Object byte[] (0x8000 + $appBytes.Length)
    [Array]::Copy($bootBytes, 0, $fullBytes, 0, $bootBytes.Length)
    [Array]::Copy($appBytes, 0, $fullBytes, 0x8000, $appBytes.Length)
    $fullOut = Join-Path $dist $CombinedName
    [System.IO.File]::WriteAllBytes($fullOut, $fullBytes)
    $artifacts += [pscustomobject]@{ name = $CombinedName; path = $fullOut; size = $fullBytes.Length; sha256 = (Get-FileHash $fullOut -Algorithm SHA256).Hash }
}

$manifest = [pscustomobject]@{
    manifest_version = 2
    product = Split-Path $product -Leaf
    configuration = $Configuration
    product_commit = $productCommit
    product_dirty = $productDirty
    platform_commit = $platformCommit
    platform_dirty = $platformDirty
    toolchain = $toolchain
    artifacts = $artifacts
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dist 'release.manifest.json') -Encoding UTF8
Write-Host "Release artifacts written to $dist"
