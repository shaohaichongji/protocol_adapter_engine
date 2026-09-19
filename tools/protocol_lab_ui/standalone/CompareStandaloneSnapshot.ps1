param(
    [Parameter(Mandatory = $true)][string]$RepositoryRoot,
    [Parameter(Mandatory = $true)][string]$SnapshotRoot,
    [Parameter(Mandatory = $true)][string]$OutputLog
)

$ErrorActionPreference = 'Stop'
$repo = [System.IO.Path]::GetFullPath($RepositoryRoot)
$snapshot = [System.IO.Path]::GetFullPath($SnapshotRoot)
$output = [System.IO.Path]::GetFullPath($OutputLog)
if (Test-Path -LiteralPath $output) {
    throw "OutputLog already exists; refusing to overwrite: $output"
}

$manifestPath = Join-Path $snapshot 'INPUT_SHA256.json'
$manifest = Get-Content -Raw -Encoding utf8 -LiteralPath $manifestPath | ConvertFrom-Json
$prefix = 'inputs/lab/'
$records = foreach ($entry in $manifest) {
    if (-not $entry.path.StartsWith($prefix) -or $entry.path.StartsWith('inputs/lab/standalone-configs/')) {
        continue
    }
    $relative = $entry.path.Substring($prefix.Length).Replace('/', '\')
    $current = Join-Path $repo $relative
    $exists = Test-Path -LiteralPath $current -PathType Leaf
    $currentHash = if ($exists) { (Get-FileHash -LiteralPath $current -Algorithm SHA256).Hash } else { $null }
    [pscustomobject]@{
        relative_path = $relative
        snapshot_sha256 = $entry.sha256
        current_sha256 = $currentHash
        matches = $exists -and $currentHash -eq $entry.sha256
    }
}

$snapshotStandalone = Join-Path $snapshot 'inputs\lab\tools\protocol_lab_ui\standalone'
$repoStandalone = Join-Path $repo 'tools\protocol_lab_ui\standalone'
$snapshotStandaloneFiles = @{}
Get-ChildItem -LiteralPath $snapshotStandalone -Recurse -File | ForEach-Object {
    $snapshotStandaloneFiles[$_.FullName.Substring($snapshotStandalone.Length + 1)] = $true
}
$extraStandaloneFiles = @(
    Get-ChildItem -LiteralPath $repoStandalone -Recurse -File | ForEach-Object {
        $relative = $_.FullName.Substring($repoStandalone.Length + 1)
        if (-not $snapshotStandaloneFiles.ContainsKey($relative)) { $relative }
    }
)

$evidence = [ordered]@{
    checked_utc = [DateTime]::UtcNow.ToString('o')
    repository_root = $repo
    snapshot_root = $snapshot
    compared_snapshot_whitelist_files = $records.Count
    mismatch_count = @($records | Where-Object { -not $_.matches }).Count
    mismatches = @($records | Where-Object { -not $_.matches })
    current_standalone_files_absent_from_snapshot = $extraStandaloneFiles
    note = 'standalone-configs are generated or duplicate snapshot inputs and are excluded from direct repository-path comparison'
}
[System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($output)) | Out-Null
[System.IO.File]::WriteAllText($output, ($evidence | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))
$output
