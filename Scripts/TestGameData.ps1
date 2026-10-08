param(
    [Parameter(Mandatory=$true)][string]$ProjectFile,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$YarnProject = ''
)
$ErrorActionPreference = 'Stop'
if (-not $EngineRoot) { throw 'Provide -EngineRoot or UE_ENGINE_ROOT.' }
$ProjectFile = (Resolve-Path -LiteralPath $ProjectFile).Path
$projectRoot = Split-Path -Parent $ProjectFile
$editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
$logDir = Join-Path $projectRoot 'Saved/Logs'
New-Item -ItemType Directory -Path $logDir -Force | Out-Null
$reportPath = Join-Path $projectRoot 'Saved/Tests/SharedGameData.json'
$started = [DateTime]::UtcNow
$arguments = @($ProjectFile, '-run=SharedGameDataTest', '-unattended', '-nullrhi', '-nosound', '-nosplash', '-UTF8Output', "-abslog=$logDir/SharedGameDataTests.log")
if ($YarnProject) { $arguments += "-YarnProject=$YarnProject" }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Integration test failed: $LASTEXITCODE. See $logDir/SharedGameDataTests.log." }
if (-not (Test-Path -LiteralPath $reportPath) -or (Get-Item -LiteralPath $reportPath).LastWriteTimeUtc -lt $started) { throw 'Missing or stale test report.' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
$minimum = if ($YarnProject) { 100 } else { 60 }
if (-not $report.passed -or $report.failures -ne 0 -or $report.checks.Count -lt $minimum) { throw 'Incomplete or failed integration checks.' }
Write-Host "PASS: $($report.checks.Count) checks. $reportPath"
