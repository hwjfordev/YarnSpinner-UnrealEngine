param(
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT
)
$ErrorActionPreference = 'Stop'
if (-not $EngineRoot) { throw 'Provide -EngineRoot or UE_ENGINE_ROOT (the UE installation directory).' }
$pluginRoot = Split-Path -Parent $PSScriptRoot
$pluginFile = Join-Path $pluginRoot 'YarnSpinner.uplugin'
$packagePath = [IO.Path]::GetFullPath($OutputDirectory)
# BuildPlugin clears its output directory. Never point it at an existing folder.
if (Test-Path -LiteralPath $packagePath) { throw 'Choose a new output directory; BuildPlugin deletes existing output contents.' }
if ($packagePath.StartsWith([IO.Path]::GetFullPath($pluginRoot) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build outside the source plugin directory.'
}
$uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
& $uat BuildPlugin "-Plugin=$pluginFile" "-Package=$packagePath" -TargetPlatforms=Win64 -UTF8Output
if ($LASTEXITCODE -ne 0) { throw "Plugin package build failed: $LASTEXITCODE" }
Write-Host "Packaged Win64 Editor, Development and Shipping plugin: $packagePath"
