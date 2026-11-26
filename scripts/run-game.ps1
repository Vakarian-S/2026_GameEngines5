<#
Runs GameEngines5.exe, plays a list of steps into its window, closes it the way the X button does,
and prints a summary of the run: exit code, warnings, errors, leaks and frame arena overflows.

Needs the desktop: the game takes focus while it runs, so a click elsewhere can steal a step.

  .\scripts\run-game.ps1 -Scene 0 -Steps "press F2","wait 700","shot scene1"
  .\scripts\run-game.ps1 -Steps (1..20 | ForEach-Object { "press F2", "wait 700", "press F1", "wait 700" })
  .\scripts\run-game.ps1 -Server -Scene 3 -Ticks 600

-Scene N passes --scene N. -Server runs the headless server with --server and --ticks, skips every
window step, waits for it to exit, and adds the window check and the tick lines to the summary.

Steps, one per string:
  press KEY              tap a key for 50 ms
  hold KEY[,KEY] MS      hold one or more keys together for MS milliseconds
  wait MS                do nothing for MS milliseconds
  shot NAME              save the window as NAME.png in the output folder
  move DX DY [COUNT]     move the mouse by DX, DY, COUNT times, 16 ms apart
  rightdown, rightup     the right mouse button
  leftdown, leftup       the left mouse button
  click X Y              move to X, Y inside the window and left click. Not tested yet
  resize W H             move the window to 50, 50 and size it W by H
  minimize, maximize, restore

Keys: F1 to F5, A to Z, 0 to 9, Shift, Ctrl, Alt, Enter, Space, Escape, Up, Down, Left, Right.
A combination is a hold, modifier first: "hold Ctrl,P 100", "hold Alt,Enter 100".
#>
param(
    [string]$Scene = "",
    [string[]]$Steps = @(),
    [ValidateSet("Debug", "Release")] [string]$Config = "Debug",
    [string]$OutDir = "$env:TEMP\ge5-run",
    [int]$StartWait = 4000,
    [switch]$Server,
    [int]$Ticks = 600
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GameInput {
    [StructLayout(LayoutKind.Sequential)] struct KEYBDINPUT { public ushort wVk; public ushort wScan; public uint dwFlags; public uint time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx; public int dy; public uint data; public uint dwFlags; public uint time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit, Size = 40)] struct INPUT { [FieldOffset(0)] public uint type; [FieldOffset(8)] public KEYBDINPUT ki; [FieldOffset(8)] public MOUSEINPUT mi; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int ht, bool repaint);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int command);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    static void Send(INPUT i) { SendInput(1, new[] { i }, Marshal.SizeOf(typeof(INPUT))); }
    /* Scancodes, because SDL reads keys by scancode. A code above 0xFF is an extended key, such as an arrow, which sends 0xE0 first */
    public static void Key(ushort scan, bool down) {
        uint extended = scan > 0xFF ? 0x0001u : 0u;
        Send(new INPUT { type = 1, ki = new KEYBDINPUT { wScan = (ushort)(scan & 0xFF), dwFlags = 0x0008u | extended | (down ? 0u : 0x0002u) } });
    }
    public static void Mouse(uint flags) { Send(new INPUT { type = 0, mi = new MOUSEINPUT { dwFlags = flags } }); }
    public static void MoveBy(int dx, int dy) { Send(new INPUT { type = 0, mi = new MOUSEINPUT { dx = dx, dy = dy, dwFlags = 0x0001u } }); }
    /* Absolute coordinates run from 0 to 65535 across the primary screen */
    public static void MoveTo(int x, int y) {
        int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
        Send(new INPUT { type = 0, mi = new MOUSEINPUT { dx = x * 65535 / (w - 1), dy = y * 65535 / (h - 1), dwFlags = 0x8001u } });
    }
}
"@
[void][GameInput]::SetProcessDPIAware()

$ScanCodes = @{
    "F1" = 0x3B; "F2" = 0x3C; "F3" = 0x3D; "F4" = 0x3E; "F5" = 0x3F
    "Shift" = 0x2A; "Ctrl" = 0x1D; "Alt" = 0x38; "Enter" = 0x1C; "Space" = 0x39; "Escape" = 0x01
    "Up" = 0xE048; "Down" = 0xE050; "Left" = 0xE04B; "Right" = 0xE04D
    "1" = 0x02; "2" = 0x03; "3" = 0x04; "4" = 0x05; "5" = 0x06; "6" = 0x07; "7" = 0x08; "8" = 0x09; "9" = 0x0A; "0" = 0x0B
    "Q" = 0x10; "W" = 0x11; "E" = 0x12; "R" = 0x13; "T" = 0x14; "Y" = 0x15; "U" = 0x16; "I" = 0x17; "O" = 0x18; "P" = 0x19
    "A" = 0x1E; "S" = 0x1F; "D" = 0x20; "F" = 0x21; "G" = 0x22; "H" = 0x23; "J" = 0x24; "K" = 0x25; "L" = 0x26
    "Z" = 0x2C; "X" = 0x2D; "C" = 0x2E; "V" = 0x2F; "B" = 0x30; "N" = 0x31; "M" = 0x32
}

function Get-ScanCode([string]$name) {
    if (-not $ScanCodes.ContainsKey($name)) { throw "Unknown key '$name'" }
    return [uint16]$ScanCodes[$name]
}

function Focus-Game {
    [void][GameInput]::SetForegroundWindow($game.MainWindowHandle)
    Start-Sleep -Milliseconds 100
}

function Save-Shot([string]$name) {
    if ($game.HasExited) { "  shot skipped: the game exited"; return }
    Focus-Game
    Start-Sleep -Milliseconds 400
    $rect = New-Object GameInput+RECT
    [void][GameInput]::GetWindowRect($game.MainWindowHandle, [ref]$rect)
    if ($rect.Right -le $rect.Left -or $rect.Bottom -le $rect.Top) { "  shot skipped: the window is gone"; return }
    $bitmap = New-Object System.Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
    $file = Join-Path $OutDir "$name.png"
    $bitmap.Save($file)
    $graphics.Dispose()
    $bitmap.Dispose()
    "  shot: $file"
}

$root = Split-Path -Parent $PSScriptRoot
$binDir = Join-Path $root "bin\x64\$Config"
$exe = Join-Path $binDir "GameEngines5.exe"
if (-not (Test-Path $exe)) { throw "No $exe. Build first: cmake --build --preset x64-$Config" }

New-Item -ItemType Directory -Force $OutDir | Out-Null
$outFile = Join-Path $OutDir "out.txt"
$errFile = Join-Path $OutDir "err.txt"

<# The game inherits this, so a failed assert or a crash exits with a line in err.txt instead of a window that waits for a click #>
$env:GE5_NO_DIALOGS = "1"
$startArgs = @{ FilePath = $exe; WorkingDirectory = $binDir; RedirectStandardOutput = $outFile; RedirectStandardError = $errFile; PassThru = $true }
$gameArgs = @()
if ($Server) { $gameArgs += "--server", "--ticks", "$Ticks" }
if ($Scene -ne "") { $gameArgs += "--scene", $Scene }
if ($gameArgs.Count -gt 0) { $startArgs.ArgumentList = $gameArgs }
$game = Start-Process @startArgs
<# Reading the handle once keeps it open, or ExitCode stays empty after the exit #>
$null = $game.Handle
$windowSeen = $false
if ($Server) {
    <# 60 ticks a second, plus time to start and print the reports #>
    $deadline = (Get-Date).AddSeconds($Ticks / 60 + 30)
    while (-not $game.HasExited -and (Get-Date) -lt $deadline) {
        $game.Refresh()
        if ($game.MainWindowHandle -ne [IntPtr]::Zero) { $windowSeen = $true }
        Start-Sleep -Milliseconds 200
    }
    if (-not $game.HasExited) { $game.Kill(); "KILLED: the server did not exit by its tick limit" }
    $game.WaitForExit()
}
else {
Start-Sleep -Milliseconds $StartWait
$game.Refresh()
if ($game.HasExited) { "The game exited during startup, code $($game.ExitCode)" }
else {
    Focus-Game
    foreach ($step in $Steps) {
        if ($game.HasExited) { "STOPPED: the game exited before step '$step'"; break }
        $parts = $step -split "\s+"
        switch ($parts[0]) {
            "press"     { Focus-Game; $code = Get-ScanCode $parts[1]; [GameInput]::Key($code, $true); Start-Sleep -Milliseconds 50; [GameInput]::Key($code, $false) }
            "hold"      { Focus-Game; $codes = $parts[1] -split "," | ForEach-Object { Get-ScanCode $_ }
                          foreach ($c in $codes) { [GameInput]::Key($c, $true) }
                          Start-Sleep -Milliseconds ([int]$parts[2])
                          foreach ($c in $codes) { [GameInput]::Key($c, $false) } }
            "wait"      { Start-Sleep -Milliseconds ([int]$parts[1]) }
            "shot"      { Save-Shot $parts[1] }
            "move"      { $count = if ($parts.Count -gt 3) { [int]$parts[3] } else { 1 }
                          for ($i = 0; $i -lt $count; $i++) { [GameInput]::MoveBy([int]$parts[1], [int]$parts[2]); Start-Sleep -Milliseconds 16 } }
            "rightdown" { Focus-Game; [GameInput]::Mouse(0x0008) }
            "rightup"   { [GameInput]::Mouse(0x0010) }
            "leftdown"  { Focus-Game; [GameInput]::Mouse(0x0002) }
            "leftup"    { [GameInput]::Mouse(0x0004) }
            "click"     { Focus-Game
                          $rect = New-Object GameInput+RECT
                          [void][GameInput]::GetWindowRect($game.MainWindowHandle, [ref]$rect)
                          [GameInput]::MoveTo($rect.Left + [int]$parts[1], $rect.Top + [int]$parts[2]); Start-Sleep -Milliseconds 50
                          [GameInput]::Mouse(0x0002); Start-Sleep -Milliseconds 50; [GameInput]::Mouse(0x0004) }
            "resize"    { [void][GameInput]::MoveWindow($game.MainWindowHandle, 50, 50, [int]$parts[1], [int]$parts[2], $true); Start-Sleep -Milliseconds 1000 }
            "minimize"  { [void][GameInput]::ShowWindow($game.MainWindowHandle, 6); Start-Sleep -Milliseconds 1000 }
            "maximize"  { [void][GameInput]::ShowWindow($game.MainWindowHandle, 3); Start-Sleep -Milliseconds 1000 }
            "restore"   { [void][GameInput]::ShowWindow($game.MainWindowHandle, 9); Start-Sleep -Milliseconds 1000 }
            default     { throw "Unknown step '$step'" }
        }
    }

    if (-not $game.HasExited) {
        [void]$game.CloseMainWindow()
        if (-not $game.WaitForExit(20000)) { $game.Kill(); "KILLED: the game did not close within 20 s" }
    }
}
}

<# The monitor colours its values, so strip the colour codes before reading the lines #>
$ansi = "\x1B\[[0-9;]*m"
$out = @(Get-Content $outFile | ForEach-Object { $_ -replace $ansi, "" })
$err = @(Get-Content $errFile | ForEach-Object { $_ -replace $ansi, "" })
$all = $out + $err
$asserts = @($all | Where-Object { $_ -match "^Assertion failed" })
$warnings = @($all | Where-Object { $_ -match "\[WARNING\]" })
$errors = @($all | Where-Object { $_ -match "\[ERROR\]" })

""
"-- Run summary --"
"  exit code       $($game.ExitCode)"
"  [WARNING] lines $($warnings.Count)"
"  [ERROR] lines   $($errors.Count)"
"  asserts         $($asserts.Count)"
$asserts + $warnings + $errors | Select-Object -First 5 | ForEach-Object { "    $_" }
"  scenes closed   $(@($out | Where-Object { $_ -match '^-- Closing ' }).Count)"
if ($Server) {
    "  window opened   $windowSeen"
    $tickLines = @($out | Where-Object { $_ -match "^tick \d+" })
    "  tick lines      $($tickLines.Count)"
    if ($tickLines.Count -gt 0) { "    first: $($tickLines[0])"; "    last:  $($tickLines[-1])" }
    $out | Where-Object { $_ -match "^Ran for" } | ForEach-Object { "  $_" }
}
$all | Where-Object { $_ -match "^Leaked blocks|fell to the heap|^Leak list" } | ForEach-Object { "  $($_.Trim())" }
"  logs            $outFile, $errFile"
