# Checks a Half-Life install's "syndicate" folder against manifest.sha256 (this folder).
# Usage (PowerShell):   .\verify.ps1                      (default Steam path)
#                       .\verify.ps1 "D:\Games\Half-Life"
# It only READS files. It reports files that are missing or different, and files in the override folders
# (syndicate_addon, syndicate_hd, syndicate_downloads, syndicate_<language>) that the game would load INSTEAD.
param([string]$HalfLife = "C:\Program Files (x86)\Steam\steamapps\common\Half-Life")

$mod = Join-Path $HalfLife "syndicate"
if (-not (Test-Path $mod)) { Write-Host "Not found: $mod"; exit 1 }
$manifest = Join-Path $PSScriptRoot "manifest.sha256"
if (-not (Test-Path $manifest)) { Write-Host "manifest.sha256 is not next to this script"; exit 1 }

$overrides = Get-ChildItem -Path $HalfLife -Directory -Filter "syndicate_*" | Select-Object -ExpandProperty FullName
$missing = 0; $different = 0; $shadowed = 0; $ok = 0

foreach ($line in Get-Content $manifest) {
    if ($line -notmatch '^([0-9a-f]{64})\s+(.+)$') { continue }
    $want = $Matches[1]; $rel = $Matches[2] -replace '/', '\'
    $file = Join-Path $mod $rel
    if (-not (Test-Path -LiteralPath $file)) {
        Write-Host "MISSING    $rel"; $missing++
    } else {
        $have = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLower()
        if ($have -ne $want) { Write-Host "DIFFERENT  $rel"; $different++ } else { $ok++ }
    }
    foreach ($o in $overrides) {
        $other = Join-Path $o $rel
        if (Test-Path -LiteralPath $other) {
            Write-Host "SHADOWED   $rel   by $(Split-Path $o -Leaf)\$rel"; $shadowed++
        }
    }
}

Write-Host ""
Write-Host "$ok identical, $different different, $missing missing, $shadowed shadowed by an override folder."
Write-Host "DIFFERENT / MISSING: copy the file from this package's syndicate folder."
Write-Host "SHADOWED: the game loads the copy in the override folder instead; delete or rename that copy."
