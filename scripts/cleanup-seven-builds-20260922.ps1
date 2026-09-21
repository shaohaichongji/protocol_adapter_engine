#Requires -Version 7.0
[CmdletBinding()]
param([switch]$Execute)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Fixed, user-approved scope. No caller-supplied paths or wildcards.
$repo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$expectedRepo = 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine'
if ($repo -ne $expectedRepo) { throw "Unexpected repository: $repo" }
$buildRoot = Join-Path $repo 'out\build'
$evidenceRoot = Join-Path $repo 'deliverables\evidence\cleanup-preparation-20260921'
$specs = @(
    @{ Name = 'windows-msvc-lab-editor-commit-diagnosis-20260920'; Files = 62; Bytes = 50798452L }
    @{ Name = 'windows-msvc-lab-editor-commit-repair-20260920'; Files = 991; Bytes = 296806972L }
    @{ Name = 'windows-msvc-lab-g2-a'; Files = 641; Bytes = 117586939L }
    @{ Name = 'windows-msvc-lab-g2-b'; Files = 1036; Bytes = 207922854L }
    @{ Name = 'windows-msvc-lab-g2-c'; Files = 3803; Bytes = 1630151827L }
    @{ Name = 'windows-msvc-protocol-metadata-20260920'; Files = 2572; Bytes = 1983992868L }
    @{ Name = 'windows-msvc-qt-smoke-repair-20260920'; Files = 991; Bytes = 292722927L }
)

function Assert-NoReparseAncestors([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Reparse point is not allowed: $($item.FullName)"
        }
        $item = $item.Parent
    }
}

function Get-SafeFiles([string]$Path) {
    Assert-NoReparseAncestors $Path
    $pending = [Collections.Generic.Stack[string]]::new()
    $pending.Push($Path)
    while ($pending.Count -gt 0) {
        foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse point is not allowed: $($item.FullName)"
            }
            if ($item.PSIsContainer) { $pending.Push($item.FullName) }
            else { $item }
        }
    }
}

function Assert-NoBuildProcesses {
    $busy = @(Get-Process | Where-Object {
        $_.ProcessName -match '^(cmake|ctest|msbuild|ninja|devenv|pae.*lab.*)$'
    })
    if ($busy.Count -gt 0) {
        throw ('Close Lab and build tools first: ' + (($busy.ProcessName | Sort-Object -Unique) -join ', '))
    }
}

function Assert-Build([hashtable]$Spec) {
    $path = [IO.Path]::GetFullPath((Join-Path $buildRoot $Spec.Name))
    if ((Split-Path -Parent $path) -ne $buildRoot) { throw "Outside build root: $path" }
    if (-not (Test-Path -LiteralPath $path -PathType Container)) {
        throw "Expected directory missing; stop and review, do not change this script: $path"
    }
    $files = @(Get-SafeFiles $path)
    $bytes = [long](($files | Measure-Object Length -Sum).Sum)
    if ($files.Count -ne $Spec.Files -or $bytes -ne $Spec.Bytes) {
        throw "Inventory changed: $path; files=$($files.Count), bytes=$bytes"
    }
    foreach ($file in $files) {
        $handle = [IO.File]::Open($file.FullName, [IO.FileMode]::Open,
            [IO.FileAccess]::Read, [IO.FileShare]::None)
        $handle.Dispose()
    }
    Write-Host "CHECKED $path ($bytes bytes)"
}

# Verify preserved evidence, not the original build files which will be deleted.
function Assert-Evidence {
    Assert-NoReparseAncestors $evidenceRoot
    $entries = @(Import-Csv -LiteralPath (Join-Path $evidenceRoot 'evidence-manifest.tsv') -Delimiter "`t")
    if ($entries.Count -ne 148) { throw 'Expected 148 evidence records.' }
    foreach ($entry in $entries) {
        $target = [IO.Path]::GetFullPath($entry.Target)
        if (-not $target.StartsWith($evidenceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw "Evidence target outside preservation directory: $target"
        }
        Assert-NoReparseAncestors (Split-Path -Parent $target)
        $file = Get-Item -LiteralPath $target -Force
        if ($file.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked evidence: $target" }
        if ($file.Length -ne [long]$entry.Bytes -or
            (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.SHA256) {
            throw "Evidence mismatch: $target"
        }
    }
    Write-Host 'EVIDENCE_OK files=148'
}

Assert-NoBuildProcesses
Assert-Evidence
foreach ($spec in $specs) { Assert-Build $spec }
Write-Host 'Approved scope: 7 directories, 10096 files, 4579982839 bytes (4.265 GiB).'
Write-Host 'Deletion is permanent. Preserved logs are NOT a complete build backup.'

if (-not $Execute) {
    Write-Host 'CHECK_ONLY_PASS: nothing deleted. Run again with -Execute only if you intend to delete.'
    exit 0
}

$confirmation = Read-Host 'Type DELETE-7 to permanently delete ONLY the seven directories listed above'
if ($confirmation -cne 'DELETE-7') { throw 'Cancelled: confirmation did not match.' }

Assert-NoBuildProcesses
Assert-Evidence
foreach ($spec in $specs) { Assert-Build $spec }
$deleted = [Collections.Generic.List[string]]::new()
try {
    foreach ($spec in $specs) {
        $path = [IO.Path]::GetFullPath((Join-Path $buildRoot $spec.Name))
        # Recheck each target immediately before deletion. Keep all build tools closed.
        Assert-NoBuildProcesses
        Assert-Build $spec
        Remove-Item -LiteralPath $path -Recurse -Force
        if (Test-Path -LiteralPath $path) { throw "Directory still exists: $path" }
        $deleted.Add($path)
        Write-Host "DELETED $path"
    }
    Assert-Evidence
    Write-Host 'CLEANUP_PASS deleted_roots=7 bytes=4579982839'
} finally {
    Write-Host "Confirmed deleted directories: $($deleted.Count)"
    foreach ($path in $deleted) { Write-Host $path }
    Write-Host 'If interrupted or failed, do not edit the whitelist or rerun blindly; report the output.'
}
