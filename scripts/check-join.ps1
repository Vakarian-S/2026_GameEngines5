<#
.SYNOPSIS
Checks joining and leaving over GNS with a real server and three real clients.

.DESCRIPTION
Starts the server, client A at 2 s, client B at 9 s, closes A at 16 s and B at 19 s, starts client
C at 21 s, then lets the server stop on its own after -Ticks. It passes when:
  - the server prints players 1, 2, 1, 0, 1
  - A prints "player 2 joined" and B prints "player 1 left"
  - C prints "lost the server: server closed" within 2 s of the server's exit, and exits 1
  - the server, A and B exit 0
  - every process shows 0 leaked blocks, and nothing prints [WARNING]
Build first. The script runs whatever exe is there. It opens three game windows, so leave the PC
alone for about 45 s.

.EXAMPLE
.\scripts\check-join.ps1
#>
param(
    [ValidateSet("Debug", "Release")] [string]$Config = "Debug",
    [string]$OutDir = "$env:TEMP\ge5-check-join",
    [int]$Ticks = 1800
)
$ErrorActionPreference = "Stop"
$exe = Join-Path (Split-Path $PSScriptRoot) "bin\x64\$Config\GameEngines5.exe"
if (-not (Test-Path $exe)) { throw "No exe at $exe. Build first: cmake --build --preset x64-$Config" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$env:GE5_NO_DIALOGS = "1"
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$failures = [System.Collections.Generic.List[string]]::new()

function Write-Step([string]$Text)
{
    Write-Host ("{0,6:N1}s  {1}" -f $clock.Elapsed.TotalSeconds, $Text)
}

function Start-Role([string]$Name, [string[]]$ArgList)
{
    Write-Step "start $Name ($($ArgList -join ' '))"
    $process = Start-Process $exe -ArgumentList $ArgList -PassThru -WorkingDirectory (Split-Path $exe) `
        -RedirectStandardOutput "$OutDir\$Name.out" -RedirectStandardError "$OutDir\$Name.err"
    # Read the handle now, or ExitCode stays empty after the process ends
    $null = $process.Handle
    return $process
}

function Stop-Role([string]$Name, $Process)
{
    Write-Step "close $Name"
    $Process.CloseMainWindow() | Out-Null
    if (-not $Process.WaitForExit(15000)) { $Process.Kill(); $failures.Add("$Name did not close within 15 s") }
}

function Read-Log([string]$Name)
{
    # The leak report colours its numbers. Strip the colour codes so the text can be matched
    (Get-Content "$OutDir\$Name.out", "$OutDir\$Name.err" -ErrorAction SilentlyContinue) -replace "\x1b\[[0-9;]*m", ""
}

function Test-Check([string]$What, [bool]$Held)
{
    Write-Host ("  {0}  {1}" -f $(if ($Held) { "pass" } else { "FAIL" }), $What)
    if (-not $Held) { $failures.Add($What) }
}

$server = Start-Role "server" @("--server", "--scene", "4", "--ticks", "$Ticks")
Start-Sleep -Seconds 2
$a = Start-Role "clientA" @("--connect", "127.0.0.1", "--scene", "4")
Start-Sleep -Seconds 7
$b = Start-Role "clientB" @("--connect", "127.0.0.1", "--scene", "4")
Start-Sleep -Seconds 7
Stop-Role "clientA" $a
Start-Sleep -Seconds 3
Stop-Role "clientB" $b
Start-Sleep -Seconds 2
$c = Start-Role "clientC" @("--connect", "127.0.0.1", "--scene", "4")
Write-Step "waiting for the server's $Ticks ticks to run out"
if (-not $server.WaitForExit(($Ticks / 60 + 30) * 1000)) { $server.Kill(); $failures.Add("server did not stop on its own") }
if (-not $c.WaitForExit(20000)) { Stop-Role "clientC" $c; $failures.Add("client C kept running after the server stopped") }
$noticed = ($c.ExitTime - $server.ExitTime).TotalSeconds
Write-Step ("client C exited {0:N1} s after the server" -f $noticed)

Write-Host "`nChecks"
$serverLog = Read-Log "server"
$counts = ($serverLog | Select-String "players: (\d+)" | ForEach-Object { $_.Matches[0].Groups[1].Value }) -join ","
Test-Check "the server prints players 1, 2, 1, 0, 1 (got $counts)" ($counts -eq "1,2,1,0,1")
Test-Check "client A hears that player 2 joined" ([bool]((Read-Log "clientA") -match "\[Client\] player 2 joined"))
Test-Check "client B hears that player 1 left" ([bool]((Read-Log "clientB") -match "\[Client\] player 1 left"))
Test-Check "client C is told the server closed" ([bool]((Read-Log "clientC") -match "lost the server: server closed"))
Test-Check ("client C notices within 2 s (took {0:N1} s)" -f $noticed) ($noticed -lt 2.0)
Test-Check "the server, A and B exit 0" ($server.ExitCode -eq 0 -and $a.ExitCode -eq 0 -and $b.ExitCode -eq 0)
Test-Check "client C exits 1" ($c.ExitCode -eq 1)
foreach ($name in "server", "clientA", "clientB", "clientC")
{
    $log = Read-Log $name
    Test-Check "$name shows 0 leaked blocks" ([bool]($log -match "Leaked blocks\s*:\s*0\b"))
    Test-Check "$name prints no [WARNING]" (-not ($log -match "\[WARNING\]"))
}

Write-Host "`nLogs in $OutDir"
if ($failures.Count -eq 0) { Write-Host "CHECK PASSED"; exit 0 }
Write-Host "CHECK FAILED: $($failures.Count) item(s)"
exit 1
