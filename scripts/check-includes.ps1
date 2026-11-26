<#
Checks that each module includes only the modules it may, as STATE.md's architecture rules say.
A module is a folder pair, include/<module> and src/<module>. The rules live in the tables below,
so a new rule is one line.

  .\scripts\check-includes.ps1
  .\scripts\check-includes.ps1 -Root C:\some\copy     checks another tree with the same layout

Prints each include that breaks a rule as file:line, and exits 1 if there is any. Exits 0 when the
code follows every rule.
#>
param([string]$Root = "")

$ErrorActionPreference = "Stop"
$root = if ($Root) { (Resolve-Path $Root).Path.TrimEnd('\') } else { Split-Path -Parent $PSScriptRoot }

# Which modules each module may include, besides itself and the standard library.
# A module missing from this table may include no other module.
$allowedModules = @{
    "main"    = @("app", "utils")
    "app"     = @("core", "ecs", "game", "net", "physics", "scenes", "utils", "vulkan", "assets")
    "scenes"  = @("core", "ecs", "game", "net", "physics", "utils", "assets")
    "game"    = @("core", "ecs", "net", "physics", "utils")
    "physics" = @("core", "ecs", "utils")
    "vulkan"  = @("core", "utils", "assets")
    "core"    = @("ecs", "utils")
    "ecs"     = @("utils")
    # Profiler reads the World's frame counters, so utils and ecs include each other today.
    "utils"   = @("ecs")
    "net"     = @()
    "assets"  = @()
}

# Outside headers in angle brackets, by their first folder, and the only modules that may name them.
$allowedOutside = @{
    "steam"  = @("net")
    "vulkan" = @("vulkan")
}

# Files that stay free of every outside header in $allowedOutside, so targets without GNS or
# Vulkan, Win32 included, can compile them. Paths are relative to the repo root.
$standardOnlyFiles = @(
    "include/net/ByteWriter.h", "src/net/ByteWriter.cpp",
    "include/net/ByteReader.h", "src/net/ByteReader.cpp",
    "include/net/Components.h"
)

$problems = New-Object System.Collections.Generic.List[string]

function Get-SourceFiles {
    $files = @()
    foreach ($top in "include", "src") {
        $topPath = Join-Path $root $top
        if (-not (Test-Path $topPath)) { continue }
        Get-ChildItem $topPath -Recurse -File -Include *.h, *.hpp, *.cpp | ForEach-Object {
            $relative = $_.FullName.Substring($root.Length + 1) -replace '\\', '/'
            $module = ($relative -split '/')[1]
            $files += [pscustomobject]@{ Path = $_.FullName; Relative = $relative; Module = $module }
        }
    }
    $main = Join-Path $root "Main.cpp"
    if (Test-Path $main) {
        $files += [pscustomobject]@{ Path = $main; Relative = "Main.cpp"; Module = "main" }
    }
    return $files
}

$files = Get-SourceFiles
$knownModules = $files | ForEach-Object { $_.Module } | Sort-Object -Unique

foreach ($file in $files) {
    $lineNumber = 0
    foreach ($line in [System.IO.File]::ReadLines($file.Path)) {
        $lineNumber++

        if ($line -match '^\s*#\s*include\s*"([^"/]+)/') {
            $target = $Matches[1]
            if ($target -eq $file.Module -or $knownModules -notcontains $target) { continue }
            $allowed = $allowedModules[$file.Module]
            if ($null -eq $allowed) { $allowed = @() }
            if ($allowed -notcontains $target) {
                $problems.Add("$($file.Relative):$lineNumber  $($file.Module) includes $target, which it may not. Allowed: $($allowed -join ', ')")
            }
        }

        if ($line -match '^\s*#\s*include\s*<([^>/]+)/') {
            $outside = $Matches[1]
            if (-not $allowedOutside.ContainsKey($outside)) { continue }
            if ($standardOnlyFiles -contains $file.Relative) {
                $problems.Add("$($file.Relative):$lineNumber  includes <$outside/...>, but this file must build without it")
            }
            elseif ($allowedOutside[$outside] -notcontains $file.Module) {
                $problems.Add("$($file.Relative):$lineNumber  $($file.Module) includes <$outside/...>, which only $($allowedOutside[$outside] -join ', ') may")
            }
        }
    }
}

if ($problems.Count -eq 0) {
    Write-Host "check-includes: $($files.Count) files, every include follows the rules"
    exit 0
}

foreach ($problem in $problems) { Write-Host $problem }
Write-Host "check-includes: $($problems.Count) include(s) break the rules"
exit 1
