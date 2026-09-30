[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $ProductRoot,
    [Parameter(Mandatory = $true)] [string] $AppVersion,
    [ValidateSet('Release')] [string] $Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$product = (Resolve-Path -LiteralPath $ProductRoot).Path
$entryPath = Join-Path $product 'tools/release_entry.json'
$contractPath = Join-Path $product 'tools/release_contract.json'
$entry = Get-Content -LiteralPath $entryPath -Raw | ConvertFrom-Json
$contract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if ($entry.product -ne $contract.product) { throw 'Release entry identity disagrees with contract' }
if ($entry.script -notmatch '^tools/[a-z_]+\.ps1$' -or
    $entry.version_parameter -notmatch '^(AppVersion|FirmwareVersion)$') {
    throw 'Invalid declared product release entry'
}
$boot = Join-Path $product 'platform/targets/stm32f105_abox_boot/dist/ABox_Boot.bin'
if ((Get-FileHash -LiteralPath $boot -Algorithm SHA256).Hash -ne $contract.frozen_boot_sha256) {
    throw 'Frozen Boot mismatch'
}
# Product packagers retain their manifest ABI and extra gates. They must stage
# every output and call the common publisher; this entry never writes dist.
$arguments = @{ Configuration = $Configuration }
$arguments[$entry.version_parameter] = $AppVersion
& (Join-Path $product $entry.script) @arguments
if (-not $?) { throw 'Product release failed' }
& python (Join-Path $PSScriptRoot 'publish_release.py') --contract $contractPath --stage (Join-Path $product 'dist') --product-root $product --validate-only | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'Published release failed validation' }
Write-Host "Formal release completed for $($entry.product): $AppVersion"
