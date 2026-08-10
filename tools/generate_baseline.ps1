[CmdletBinding()]
param(
    [string] $Workspace = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [string] $Output = (Join-Path (Split-Path $PSScriptRoot -Parent) 'docs/baseline.json')
)

$projects = Get-ChildItem -LiteralPath $Workspace -Directory |
    Where-Object { $_.Name -notmatch '^\.' -and $_.Name -notin @('build','ABox_Platform') }
$result = foreach ($project in $projects) {
    $files = Get-ChildItem -LiteralPath $project.FullName -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Extension -in '.ioc','.ld','.sct','.uvprojx' -or $_.Name -in 'CMakeLists.txt','CMakePresets.json' }
    [pscustomobject]@{
        product = $project.Name
        has_git = Test-Path (Join-Path $project.FullName '.git')
        files = @($files | ForEach-Object { $_.FullName.Substring($project.FullName.Length + 1).Replace('\','/') })
    }
}
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $Output -Encoding UTF8
Write-Host "Wrote $Output"
