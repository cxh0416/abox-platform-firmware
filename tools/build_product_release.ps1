[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $ProductRoot,
    [Parameter(Mandatory = $true)] [string] $AppTarget,
    [Parameter(Mandatory = $true)] [string] $BootTarget,
    [ValidateSet('Debug', 'Release')] [string] $Configuration = 'Release',
    [string] $CombinedName
)

$ErrorActionPreference = 'Stop'
$product = (Resolve-Path -LiteralPath $ProductRoot).Path
$dist = Join-Path $product 'dist'
New-Item -ItemType Directory -Force -Path $dist | Out-Null

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

$platformCommit = (& git -C (Join-Path $product 'platform') rev-parse HEAD 2>$null).Trim()
$manifest = [pscustomobject]@{
    product = Split-Path $product -Leaf
    configuration = $Configuration
    platform_commit = $platformCommit
    artifacts = $artifacts
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dist 'release.manifest.json') -Encoding UTF8
Write-Host "Release artifacts written to $dist"
