[CmdletBinding()]
param(
    [string] $Workspace = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent)
)

$ErrorActionPreference = 'Stop'
$expectedMcu = 'STM32F105R(8-B-C)Tx'
$expectedPackage = 'LQFP64'
$products = @(
    @{ Name = 'Airport_vehicle'; Uart5 = 115200; Can1 = 9; Can2 = 4; Boot = 'boot/STM32F105XX_FLASH.ld'; App = 'app/STM32F105XX_FLASH.ld' },
    @{ Name = 'CageDumper'; Uart5 = 115200; Can1 = 4; Can2 = 4; Boot = 'boot/STM32F105XX_FLASH.ld'; App = 'app/STM32F105XX_FLASH.ld' },
    @{ Name = 'LockCtrlBoard_cheweishi'; Uart5 = 115200; Can1 = 9; Can2 = 4; Boot = 'boot/STM32F105XX_FLASH.ld'; App = 'app/STM32F105XX_FLASH.ld' },
    @{ Name = 'Meal_Delivery_Vehicle'; Uart5 = 115200; Can1 = 9; Can2 = 4; Boot = 'boot/STM32F105xx_FLASH.ld'; App = 'app/STM32F105XX_FLASH.ld' },
    @{ Name = 'Sweeper_VCU'; Uart5 = 9600; Can1 = 4; Can2 = 4; Boot = 'boot/STM32F105XX_FLASH.ld'; App = 'app/STM32F105xx_FLASH.ld' }
)

function Assert-Contains([string] $Path, [string] $Pattern, [string] $Label) {
    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) { throw "$Label missing in $Path" }
}

$flashScript = Join-Path $PSScriptRoot 'validate_flash_layout.ps1'
foreach ($product in $products) {
    $root = Join-Path $Workspace $product.Name
    $ioc = Get-ChildItem (Join-Path $root 'app') -Filter *.ioc -File | Select-Object -First 1
    if ($null -eq $ioc) { throw "No App IOC found for $($product.Name)" }
    Assert-Contains $ioc.FullName "^Mcu.Name=$([regex]::Escape($expectedMcu))$" "$($product.Name) MCU"
    Assert-Contains $ioc.FullName "^Mcu.Package=$([regex]::Escape($expectedPackage))$" "$($product.Name) package"

    $cfg = Join-Path $root 'platform_config/abox_product_config.h'
    if (-not (Test-Path -LiteralPath $cfg)) { throw "Missing product config: $cfg" }
    Assert-Contains $cfg 'ABOX_PRODUCT_CONFIG' "$($product.Name) platform config"

    $uart = Join-Path $root 'app/Core/Src/usart.c'
    Assert-Contains $uart "huart5\.Init\.BaudRate\s*=\s*$($product.Uart5)" "$($product.Name) UART5 base baud"
    Assert-Contains $ioc.FullName "^CAN1\.Prescaler=$($product.Can1)$" "$($product.Name) CAN1 prescaler"
    Assert-Contains $ioc.FullName "^CAN2\.Prescaler=$($product.Can2)$" "$($product.Name) CAN2 prescaler"

    & $flashScript -BootLinker (Join-Path $root $product.Boot) -AppLinker (Join-Path $root $product.App) | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "$($product.Name) Flash layout check failed" }
    Write-Host "PASS $($product.Name) hardware/config matrix"
}
