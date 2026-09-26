[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [Parameter(Mandatory = $true)]
    [ValidateSet("source", "static", "shared")]
    [string]$Kind,

    [Parameter(Mandatory = $true)]
    [bool]$HasYaml
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$root = [System.IO.Path]::GetFullPath($PackageRoot)
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw "Package root does not exist: $root"
}

function Read-Entries {
    param([string]$FileName, [string]$Pattern)

    $entries = @{}
    foreach ($line in (Get-Content -LiteralPath (Join-Path $root $FileName) -Encoding UTF8)) {
        if ($line -cnotmatch $Pattern) {
            throw "Invalid $FileName entry: $line"
        }
        $relative = $Matches[2]
        if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or $entries.ContainsKey($relative)) {
            throw "Unsafe or duplicate $FileName path: $relative"
        }
        $entries.Add($relative, $Matches[1])
    }
    return $entries
}

$manifest = Read-Entries "MANIFEST.txt" '^([0-9]+)  (.+)$'
$hashes = Read-Entries "SHA256SUMS.txt" '^([0-9a-f]{64})  (.+)$'
$actual = @{}
foreach ($file in (Get-ChildItem -LiteralPath $root -Recurse -File)) {
    $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
    if ($actual.ContainsKey($relative)) {
        throw "Duplicate package path: $relative"
    }
    $actual.Add($relative, $file)
}
if ($actual.Count -ne ($manifest.Count + 2) -or $actual.Count -ne ($hashes.Count + 1)) {
    throw "Manifest or SHA256SUMS file count differs from package contents"
}
foreach ($relative in $actual.Keys) {
    $file = $actual[$relative]
    if ($relative -notin @("MANIFEST.txt", "SHA256SUMS.txt")) {
        if (-not $manifest.ContainsKey($relative) -or [uint64]$manifest[$relative] -ne [uint64]$file.Length) {
            throw "Manifest length mismatch: $relative"
        }
    }
    if ($relative -ne "SHA256SUMS.txt") {
        $actualHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        if (-not $hashes.ContainsKey($relative) -or $hashes[$relative] -cne $actualHash) {
            throw "SHA-256 mismatch: $relative"
        }
    }
}
foreach ($relative in $manifest.Keys) {
    if (-not $actual.ContainsKey($relative)) { throw "Manifest contains missing file: $relative" }
}
foreach ($relative in $hashes.Keys) {
    if (-not $actual.ContainsKey($relative)) { throw "SHA256SUMS contains missing file: $relative" }
}
$provenance = Get-Content -Raw -LiteralPath (Join-Path $root "PROVENANCE.json") -Encoding UTF8 | ConvertFrom-Json
if ($provenance.package_kind -cne $Kind -or [bool]$provenance.yaml_frontend_installed -ne $HasYaml -or
    [bool]$provenance.yaml_frontend_source_optional -ne ($Kind -eq "source")) {
    throw "Package provenance does not match expected kind or YAML availability"
}
if ($Kind -eq "source" -and $provenance.yaml_frontend_linkage -cne "none") {
    throw "Source package incorrectly declares an installed YAML component"
}
if ($Kind -ne "source" -and $provenance.yaml_frontend_linkage -cne $(if ($HasYaml) { "static-addon" } else { "none" })) {
    throw "Binary package YAML linkage declaration is incorrect"
}
Write-Output "PAE_YAML_SDK_PACKAGE_VERIFIED kind=$Kind yaml=$HasYaml files=$($actual.Count) root=$root"
