#Requires -Version 7.0
[CmdletBinding()]
param(
    [string[]]$CandidatePath = @(),
    [switch]$ChangedOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (& git -c core.quotepath=false rev-parse --show-toplevel 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($repoRoot)) {
    throw 'Run this check from a Git worktree.'
}
$repoRoot = [IO.Path]::GetFullPath($repoRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
$repoPrefix = $repoRoot + [IO.Path]::DirectorySeparatorChar

function Convert-ToRepoRelativePath([string]$InputPath) {
    $fullPath = if ([IO.Path]::IsPathRooted($InputPath)) {
        [IO.Path]::GetFullPath($InputPath)
    } else {
        [IO.Path]::GetFullPath((Join-Path $repoRoot $InputPath))
    }
    if (-not $fullPath.StartsWith($repoPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Candidate path is outside the repository: $InputPath"
    }
    return $fullPath.Substring($repoPrefix.Length).Replace([IO.Path]::DirectorySeparatorChar, '/')
}

function Test-TextCandidate([string]$RelativePath) {
    if ($RelativePath -like 'third_party/qt/*' -or
        $RelativePath -like 'third_party/yyjson/src/*' -or
        $RelativePath -eq 'third_party/yyjson/LICENSE') {
        return $false
    }
    if ([IO.Path]::GetFileName($RelativePath) -eq 'CMakeLists.txt') {
        return $true
    }
    return [IO.Path]::GetExtension($RelativePath) -in @(
        '.bat', '.c', '.cc', '.cmake', '.cmd', '.cpp', '.h', '.hpp', '.json', '.jsonl',
        '.md', '.ps1', '.tsv', '.txt', '.yaml', '.yml'
    )
}

function Test-UncPath([string]$Line) {
    $slash = [char]92
    for ($index = 0; $index + 3 -lt $Line.Length; ++$index) {
        if ($Line[$index] -ne $slash -or $Line[$index + 1] -ne $slash) {
            continue
        }
        if ($index -gt 0 -and -not [char]::IsWhiteSpace($Line[$index - 1]) -and
            $Line[$index - 1] -notin @('''', '"', '`', '(')) {
            continue
        }
        $remainder = $Line.Substring($index + 2)
        if ($remainder -match '^[A-Za-z0-9._-]+\\[A-Za-z0-9$._-]+') {
            return $true
        }
    }
    return $false
}

$trackedFiles = @()
if ($ChangedOnly) {
    $trackedFiles += @(
        & git -c core.quotepath=false -c core.safecrlf=false diff --name-only --diff-filter=ACMR --
    )
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to enumerate modified Git-tracked files.'
    }
    $trackedFiles += @(& git -c core.quotepath=false diff --cached --name-only --diff-filter=ACMR --)
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to enumerate staged Git-tracked files.'
    }
} else {
    $trackedFiles = @(& git -c core.quotepath=false ls-files)
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to enumerate Git-tracked files.'
    }
}

$candidateFiles = [Collections.Generic.List[string]]::new()
foreach ($candidate in $CandidatePath) {
    $relativePath = Convert-ToRepoRelativePath $candidate
    $fullPath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        throw "Candidate path does not exist: $candidate"
    }
    if (Test-Path -LiteralPath $fullPath -PathType Container) {
        Get-ChildItem -LiteralPath $fullPath -File -Recurse | ForEach-Object {
            $candidateFiles.Add((Convert-ToRepoRelativePath $_.FullName))
        }
    } else {
        $candidateFiles.Add($relativePath)
    }
}

$files = @($trackedFiles) + @($candidateFiles)
$files = @($files | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Sort-Object -Unique)
$drivePattern = [regex]'(?i)(?<![A-Z0-9_])[A-Z]:[\\/]'
$unixHomePattern = [regex]'(?<![A-Za-z0-9_])/(?:home|Users)/[^/\s]+/'
$hits = [Collections.Generic.List[object]]::new()
$scanned = 0

foreach ($relativePath in $files) {
    $relativePath = $relativePath.Replace([IO.Path]::DirectorySeparatorChar, '/')
    if (-not (Test-TextCandidate $relativePath)) {
        continue
    }
    $fullPath = Join-Path $repoRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        continue
    }
    ++$scanned
    $lineNumber = 0
    foreach ($line in [IO.File]::ReadLines($fullPath)) {
        ++$lineNumber
        $kind = if ($drivePattern.IsMatch($line)) {
            'WINDOWS_DRIVE'
        } elseif (Test-UncPath $line) {
            'UNC'
        } elseif ($unixHomePattern.IsMatch($line)) {
            'UNIX_HOME'
        } else {
            $null
        }
        if ($null -ne $kind) {
            $hits.Add([pscustomobject]@{ Kind = $kind; Path = $relativePath; Line = $lineNumber })
        }
    }
}

foreach ($hit in $hits) {
    "PORTABLE_PATH_HIT kind=$($hit.Kind) path=$($hit.Path) line=$($hit.Line)"
}
if ($hits.Count -ne 0) {
    [Console]::Error.WriteLine("Portable path check failed: files=$scanned hits=$($hits.Count)")
    exit 2
}

"PORTABLE_PATH_CHECK_PASS files=$scanned hits=0"
