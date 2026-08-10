[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string] $BootLinker,
    [Parameter(Mandatory = $true)] [string] $AppLinker,
    [uint32] $FlashBase = 0x08000000,
    [uint32] $FlashSize = 0x40000,
    [uint32] $BootSize = 0x8000,
    [uint32] $AppStart = 0x08008000,
    [uint32] $OtaInfoStart = 0x0803F800,
    [uint32] $OtaInfoSize = 0x800
)

function Read-GnuFlash([string] $Path) {
    $text = Get-Content -LiteralPath $Path -Raw -Encoding UTF8
    $match = [regex]::Match($text, 'FLASH\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(0x[0-9A-Fa-f]+),\s*LENGTH\s*=\s*(0x[0-9A-Fa-f]+|[0-9]+K?)')
    if (-not $match.Success) { return $null }
    [pscustomobject]@{ Origin = [Convert]::ToUInt32($match.Groups[1].Value.Substring(2), 16); Length = Convert-Size $match.Groups[2].Value }
}

function Convert-Size([string] $value) {
    if ($value -match '^0x') { return [Convert]::ToUInt32($value.Substring(2), 16) }
    if ($value.EndsWith('K')) { return [uint32]([uint32]$value.TrimEnd('K') * 1024) }
    return [uint32]$value
}

function Assert-Equal([string] $Name, [uint32] $Actual, [uint32] $Expected) {
    if ($Actual -ne $Expected) { throw "$Name expected 0x$('{0:X8}' -f $Expected), got 0x$('{0:X8}' -f $Actual)" }
    Write-Host "PASS $Name = 0x$('{0:X8}' -f $Actual)"
}

$boot = Read-GnuFlash $BootLinker
$app = Read-GnuFlash $AppLinker
if ($null -eq $boot) { throw "Cannot parse GNU Flash region from $BootLinker" }
if ($null -eq $app) { throw "Cannot parse GNU Flash region from $AppLinker" }

Assert-Equal 'Boot origin' $boot.Origin $FlashBase
Assert-Equal 'Boot length' $boot.Length $BootSize
Assert-Equal 'App origin' $app.Origin $AppStart
$otaEnd = [uint64]$OtaInfoStart + $OtaInfoSize
$appEnd = [uint64]$app.Origin + $app.Length
if ($appEnd -gt $OtaInfoStart) { throw 'App Flash region overlaps OTA info page' }
if ($otaEnd -gt ([uint64]$FlashBase + $FlashSize)) { throw 'OTA info page is outside declared device Flash capacity' }
Write-Host "PASS OTA page 0x$('{0:X8}' -f $OtaInfoStart)-0x$('{0:X8}' -f ($otaEnd - 1))"
