[CmdletBinding()]
param(
    [switch]$Execute,
    [switch]$VerifyAfter
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedRepo = 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine'
$workspace = 'F:\PersonalWorkspace'
$repo = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\')
$repoOut = Join-Path $repo 'out'

function Normalize-Path([string]$Path) {
    return [IO.Path]::GetFullPath($Path).TrimEnd('\')
}

function Test-SamePath([string]$Left, [string]$Right) {
    return (Normalize-Path $Left).Equals((Normalize-Path $Right), [StringComparison]::OrdinalIgnoreCase)
}

function Test-IsChildPath([string]$Path, [string]$Parent) {
    $normalizedPath = Normalize-Path $Path
    $normalizedParent = Normalize-Path $Parent
    return $normalizedPath.StartsWith($normalizedParent + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Write-JsonFile([string]$Path, $Value) {
    $json = $Value | ConvertTo-Json -Depth 12
    [IO.File]::WriteAllText($Path, $json + [Environment]::NewLine, [Text.UTF8Encoding]::new($false))
}

function Get-TreeEntriesNoFollow([string]$Root) {
    $pending = [Collections.Generic.Stack[IO.DirectoryInfo]]::new()
    $pending.Push([IO.DirectoryInfo]::new($Root))
    while ($pending.Count -gt 0) {
        $directory = $pending.Pop()
        foreach ($entry in $directory.EnumerateFileSystemInfos('*', [IO.SearchOption]::TopDirectoryOnly)) {
            $isReparse = [bool]($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)
            $isDirectory = [bool]($entry.Attributes -band [IO.FileAttributes]::Directory)
            $linkTarget = $null
            if ($isReparse) {
                $linkTarget = (Get-Item -LiteralPath $entry.FullName -Force).Target
            }
            [pscustomobject]@{
                FullName = $entry.FullName
                IsDirectory = $isDirectory
                IsReparse = $isReparse
                LinkTarget = $linkTarget
                Length = if ($isDirectory) { [int64]0 } else { [int64]([IO.FileInfo]$entry).Length }
                LastWriteTimeUtc = $entry.LastWriteTimeUtc
            }
            if ($isDirectory -and -not $isReparse) {
                $pending.Push([IO.DirectoryInfo]$entry)
            }
        }
    }
}

function Get-TreeStats([string]$Root) {
    $entries = @(Get-TreeEntriesNoFollow $Root)
    $signatureLines = $entries | Sort-Object FullName | ForEach-Object {
        '{0}|{1}|{2}|{3}|{4}' -f $_.FullName, $_.IsDirectory, $_.IsReparse, $_.Length, $_.LastWriteTimeUtc.Ticks
    }
    $signatureBytes = [Text.Encoding]::UTF8.GetBytes(($signatureLines -join "`n"))
    $hasher = [Security.Cryptography.SHA256]::Create()
    try {
        $signature = [Convert]::ToHexString($hasher.ComputeHash($signatureBytes)).ToLowerInvariant()
    } finally {
        $hasher.Dispose()
    }
    return [pscustomobject]@{
        Files = @($entries | Where-Object { -not $_.IsDirectory -and -not $_.IsReparse }).Count
        Directories = @($entries | Where-Object { $_.IsDirectory -and -not $_.IsReparse }).Count
        ReparsePoints = @($entries | Where-Object IsReparse).Count
        Bytes = [int64](($entries | Where-Object { -not $_.IsDirectory -and -not $_.IsReparse } | Measure-Object Length -Sum).Sum)
        Signature = $signature
        Entries = $entries
    }
}

function Assert-NoActiveBuildProcesses {
    $blocked = @(Get-Process | Where-Object {
        $_.ProcessName -match '^(cmake|ctest|msbuild|ninja|pae_protocol_lab_ui)$'
    })
    if ($blocked.Count -gt 0) {
        $details = $blocked | ForEach-Object { '{0}:{1}' -f $_.ProcessName, $_.Id }
        throw "发现占用/构建进程，停止执行：$($details -join ', ')"
    }
}

function Assert-NoReparseAncestor([string]$Path) {
    $current = [IO.DirectoryInfo]::new((Normalize-Path $Path)).Parent
    while ($null -ne $current) {
        if ($current.Exists -and ($current.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "候选祖先是重解析点：$($current.FullName)"
        }
        $current = $current.Parent
    }
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-ProtectedSnapshot([string[]]$Paths) {
    $files = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    $links = [Collections.Generic.Dictionary[string, object]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path)) {
            throw "必须保留路径不存在：$path"
        }
        $item = Get-Item -LiteralPath $path -Force
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            $links[$item.FullName] = [pscustomobject]@{ Path = $item.FullName; Target = @($item.Target) }
            continue
        }
        if ($item.PSIsContainer) {
            foreach ($entry in Get-TreeEntriesNoFollow $item.FullName) {
                if ($entry.IsReparse) {
                    $links[$entry.FullName] = [pscustomobject]@{ Path = $entry.FullName; Target = @($entry.LinkTarget) }
                } elseif (-not $entry.IsDirectory) {
                    $files[$entry.FullName] = [pscustomobject]@{
                        Path = $entry.FullName
                        Length = $entry.Length
                        Sha256 = Get-Sha256 $entry.FullName
                    }
                }
            }
        } else {
            $files[$item.FullName] = [pscustomobject]@{
                Path = $item.FullName
                Length = [int64]$item.Length
                Sha256 = Get-Sha256 $item.FullName
            }
        }
    }
    return [pscustomobject]@{
        Files = @($files.Values | Sort-Object Path)
        ReparsePoints = @($links.Values | Sort-Object Path)
    }
}

function Assert-SameProtectedSnapshot($Before, $After) {
    $beforeFiles = @($Before.Files)
    $afterFiles = @($After.Files)
    if ($beforeFiles.Count -ne $afterFiles.Count) {
        throw "保护文件数量变化：$($beforeFiles.Count) -> $($afterFiles.Count)"
    }
    $afterMap = @{}
    foreach ($file in $afterFiles) { $afterMap[$file.Path.ToLowerInvariant()] = $file }
    foreach ($file in $beforeFiles) {
        $key = $file.Path.ToLowerInvariant()
        if (-not $afterMap.ContainsKey($key)) { throw "保护文件缺失：$($file.Path)" }
        $candidate = $afterMap[$key]
        if ($file.Length -ne $candidate.Length -or $file.Sha256 -ne $candidate.Sha256) {
            throw "保护文件内容变化：$($file.Path)"
        }
    }
    $beforeLinks = @($Before.ReparsePoints | ForEach-Object { $_.Path.ToLowerInvariant() })
    $afterLinks = @($After.ReparsePoints | ForEach-Object { $_.Path.ToLowerInvariant() })
    if (@(Compare-Object $beforeLinks $afterLinks).Count -ne 0) {
        throw '保护范围内重解析点集合发生变化。'
    }
}

function Test-IsEvidenceFile($Entry, [string]$Root) {
    if ($Entry.IsDirectory -or $Entry.IsReparse) { return $false }
    $relative = [IO.Path]::GetRelativePath($Root, $Entry.FullName)
    $classificationPath = $relative
    if ((Test-SamePath $Root (Join-Path $repoOut 'build'))) {
        $parts = @($relative -split '\\')
        if ($parts.Count -gt 1) { $classificationPath = @($parts[1..($parts.Count - 1)]) -join '\' }
    }
    $name = [IO.Path]::GetFileName($Entry.FullName)
    $extension = [IO.Path]::GetExtension($Entry.FullName).ToLowerInvariant()
    $evidenceExtensions = @('.txt', '.json', '.xml', '.md', '.csv', '.trace', '.etl', '.bin', '.dat', '.yaml', '.yml')
    $dumpExtensions = @('.dmp', '.mdmp', '.dump', '.core')
    if ($extension -eq '.log') { return $true }
    if ($classificationPath -match '(^|\\)(Testing|logs)(\\|$)') { return $true }
    if ($name -like 'INPUT_*.json') { return $true }
    if ($name -match '^(PROVENANCE\.json|MANIFEST\.txt|SHA256SUMS(?:\.txt)?|README(?:\..+)?)$') { return $true }
    if ($extension -in $dumpExtensions -or $name -match '(?i)(mini)?dump|crash') { return $true }
    if ($classificationPath -match '(?i)(^|\\)(manual[^\\]*|acceptance[^\\]*|crash[^\\]*|diagnos[^\\]*|review[^\\]*|negative[^\\]*|evidence[^\\]*|[^\\]*evidence[^\\]*|run_reader[^\\]*|test-output|test-results)(\\|$)' -and $extension -in $evidenceExtensions) {
        return $true
    }
    return $false
}

function Test-IsCoreInventoryEvidence($Entry, [string]$Root) {
    if ($Entry.IsDirectory -or $Entry.IsReparse) { return $false }
    $relative = [IO.Path]::GetRelativePath($Root, $Entry.FullName)
    $classificationPath = $relative
    if ((Test-SamePath $Root (Join-Path $repoOut 'build'))) {
        $parts = @($relative -split '\\')
        if ($parts.Count -gt 1) { $classificationPath = @($parts[1..($parts.Count - 1)]) -join '\' }
    }
    $name = [IO.Path]::GetFileName($Entry.FullName)
    $extension = [IO.Path]::GetExtension($Entry.FullName).ToLowerInvariant()
    if ($extension -eq '.log') { return $true }
    if ($classificationPath -match '(^|\\)Testing(\\|$)') { return $true }
    if ($extension -in @('.dmp', '.mdmp', '.dump', '.core') -or $name -match '(?i)(mini)?dump') { return $true }
    if ($classificationPath -match '(?i)(^|\\)(manual[^\\]*|acceptance[^\\]*|crash[^\\]*|diagnos[^\\]*|review[^\\]*)(\\|$)' -and
        $extension -in @('.txt', '.json', '.xml', '.md', '.csv', '.trace', '.etl', '.bin', '.dat')) {
        return $true
    }
    return $false
}

function Get-ContainingTarget([string]$Path, $Targets) {
    foreach ($target in $Targets) {
        if ((Test-SamePath $Path $target.Path) -or (Test-IsChildPath $Path $target.Path)) { return $target }
    }
    return $null
}

if (-not (Test-SamePath $repo $expectedRepo)) {
    throw "仓库路径不匹配：$repo"
}
if ((git -C $repo branch --show-current).Trim() -ne 'main') { throw '当前分支不是 main。' }
if ((git -C $repo rev-parse HEAD).Trim() -ne '0b89beded03b39d8ffe019bfec659b5a5885a8f1') { throw '当前 HEAD 已变化。' }
if (@(git -C $repo diff --cached --name-only).Count -ne 0) { throw '暂存区非空。' }

$buildNames = @(
    'build', 'b', 'v09-full-debug', 'integrated-ui-v09', 'dec042b-c3-debug', 'yv',
    'dec042b-c2-run-debug', 'dec042b-lab-c1-debug', 'recovery-v09-lab-tests',
    'recovery-v09-tests', 'recovery-v09-lab', 'dec042b-c3-testing-off-debug', 'be-debug',
    'dec042b-c3-release-msvc', 'v09-full-release', 'dec042b-c2-lab-on-testing-off-debug',
    'dec042b-c3-legacy-lab-default-debug', 'dec042b-c2-run-release', 'b42bd',
    'v09-lab-testing-off', 'dec042b-c3-testing-off-release-msvc', 'dec042b-lab-c1-release',
    'recovery-v09-compiler', 'b042b', 'dec042b-c3-product-only-debug-msvc',
    'dec042b-c2-product-only-debug', 'dec042b-c1-lab-on-testing-off',
    'dec042b-c2-lab-on-testing-off-release', 'be-release', 'v09-product-only',
    'be-testing-off-release', 'dec042b-c3-product-only-release-msvc', 'b42br',
    'dec042b-c2-product-only-release', 'host-endpoint-gates', 'dec042b-c1-product-only',
    'b42product', 'dec042b-c3-release', 'dec042b-c2-gate-missing-prereqs',
    'dec042b-c1-gate-missing-slices', 'dec042b-c1-gate-testing-off', 'b42gate-testoff',
    'be-default-off-release', 'dec042b-c2-gate-testing-off',
    'dec042b-c3-gate-missing-dependencies', 'dec042b-c3-gate-missing-explicit',
    'dec042b-c2-gate-ordinary-lab-v05', 'dec042b-c1-gate-ordinary-lab-schema05', 'b42gate-lab'
)

$preflightNames = @(
    'lab-sdk-standalone-preflight-deploy', 'lab-sdk-standalone-preflight-deploy-2',
    'lab-sdk-standalone-preflight-deploy-3', 'lab-sdk-standalone-preflight-deploy-4'
)
$oldRepoSdkNames = @('sdk-public-stream-description', 'sdk-stage3', 'sdk-stage4-ascii', 'sdk-stage4-p')
$staticRoot = Join-Path $workspace 'pae-lab-clean-sdk-static-20260919'
$sharedRoot = Join-Path $workspace 'pae-lab-clean-sdk-shared-20260919'
$staticSubdirs = @(
    'build', 'build-debug-testing-off', 'build-debug-testing-on', 'build-release-testing-off',
    'build-release-testing-on', 'inputs', 'negative-static-debug-sdk-release-build',
    'negative-static-kind-mismatch', 'negative-static-release-sdk-debug-build'
)
$sharedSubdirs = @(
    'build-debug-testing-off', 'build-debug-testing-on', 'build-release-testing-off',
    'build-release-testing-on', 'derived-negative-inputs', 'inputs',
    'negative-shared-debug-sdk-release-build', 'negative-shared-kind-mismatch',
    'negative-shared-missing-runtime-dll', 'negative-shared-release-sdk-debug-build',
    'negative-shared-wrong-runtime-dll'
)
$oldLabNames = @(
    'pae-lab-sdk-static-20260919-r6', 'pae-lab-sdk-static-20260919-r7',
    'pae-lab-sdk-static-20260919-r5', 'pae-lab-sdk-shared-20260919-r2',
    'pae-lab-sdk-static-20260919-r2', 'pae-lab-sdk-static-20260919-r8',
    'pae-lab-sdk-static-20260919-r3', 'pae-lab-sdk-static-20260919-r4',
    'pae-lab-sdk-static-20260919', 'pae-lab-sdk-static-regression-20260919',
    'pae-lab-sdk-shared-20260919', 'pae-lab-ascii-a1-static-consumer-20260918'
)
$oldSdkNames = @(
    'pae-sdk-stage4-ascii-validation-candidate1-20260918',
    'pae-sdk-stage3-validation-final5-20260915',
    'pae-sdk-stage3-validation-final4-20260915',
    'pae-sdk-stage3-validation-final3-20260915',
    'pae-sdk-stage3-validation-recovery-20260915',
    'pae-sdk-stage3-validation-final2-20260915',
    'pae-sdk-stage3-validation-final-20260915',
    'pae-sdk-public-stream-validation-candidate1-20260918'
)

$targets = [Collections.Generic.List[object]]::new()
foreach ($name in $buildNames) { $targets.Add([pscustomobject]@{ Category = 'out-build-root'; Path = Normalize-Path (Join-Path $repoOut $name); Scope = $repoOut }) }
foreach ($name in $preflightNames) { $targets.Add([pscustomobject]@{ Category = 'old-preflight'; Path = Normalize-Path (Join-Path $repoOut $name); Scope = $repoOut }) }
foreach ($name in $staticSubdirs) { $targets.Add([pscustomobject]@{ Category = 'current-static-subdir'; Path = Normalize-Path (Join-Path $staticRoot $name); Scope = $staticRoot }) }
foreach ($name in $sharedSubdirs) { $targets.Add([pscustomobject]@{ Category = 'current-shared-subdir'; Path = Normalize-Path (Join-Path $sharedRoot $name); Scope = $sharedRoot }) }
foreach ($name in $oldRepoSdkNames) { $targets.Add([pscustomobject]@{ Category = 'old-repo-sdk'; Path = Normalize-Path (Join-Path $repoOut $name); Scope = $repoOut }) }
foreach ($name in $oldLabNames) { $targets.Add([pscustomobject]@{ Category = 'old-external-lab'; Path = Normalize-Path (Join-Path $workspace $name); Scope = $workspace }) }
foreach ($name in $oldSdkNames) { $targets.Add([pscustomobject]@{ Category = 'old-external-sdk'; Path = Normalize-Path (Join-Path $workspace $name); Scope = $workspace }) }
$clonePath = Normalize-Path (Join-Path $workspace 'pae-clean-checkpoint-98df5e0-source')
$currentConsumerPath = Normalize-Path (Join-Path $workspace 'pae-sdk-clean-checkpoint-validation-candidate1-20260919')
$targets.Add([pscustomobject]@{ Category = 'clean-clone'; Path = $clonePath; Scope = $workspace })
$targets.Add([pscustomobject]@{ Category = 'current-external-consumer'; Path = $currentConsumerPath; Scope = $workspace })

if ($buildNames.Count -ne 49 -or $staticSubdirs.Count -ne 9 -or $sharedSubdirs.Count -ne 11 -or $oldLabNames.Count -ne 12 -or $oldSdkNames.Count -ne 8 -or $targets.Count -ne 99) {
    throw '固定白名单数量断言失败。'
}

$protectedPaths = [Collections.Generic.List[string]]::new()
@(
    (Join-Path $repo 'deliverables\sdk\98df5e0'),
    (Join-Path $repo 'deliverables\lab\98df5e0'),
    (Join-Path $repoOut 'sdk-clean-checkpoint\candidate1-20260919'),
    (Join-Path $repoOut 'sdk-clean-checkpoint-validation'),
    (Join-Path $repo 'src'), (Join-Path $repo 'tests'), (Join-Path $repo 'third_party'),
    (Join-Path $staticRoot 'deploy'), (Join-Path $staticRoot 'logs'),
    (Join-Path $staticRoot 'INPUT_PROVENANCE.json'), (Join-Path $staticRoot 'INPUT_SHA256.json'),
    (Join-Path $sharedRoot 'deploy'), (Join-Path $sharedRoot 'logs'),
    (Join-Path $sharedRoot 'INPUT_PROVENANCE.json'), (Join-Path $sharedRoot 'INPUT_SHA256.json')
) | ForEach-Object { $protectedPaths.Add((Normalize-Path $_)) }
@('downloads', 'sources', 'manual-lab', 'review', 'validation') | ForEach-Object {
    $path = Join-Path $repoOut $_
    if (Test-Path -LiteralPath $path) { $protectedPaths.Add((Normalize-Path $path)) }
}
Get-ChildItem -LiteralPath $repoOut -Directory -Force | Where-Object Name -like 'agent-c3-acceptance-*' | ForEach-Object { $protectedPaths.Add((Normalize-Path $_.FullName)) }
Get-ChildItem -LiteralPath $repoOut -File -Force | ForEach-Object { $protectedPaths.Add((Normalize-Path $_.FullName)) }

if ($Execute -and $VerifyAfter) { throw '-Execute 与 -VerifyAfter 不能同时使用。' }
if ($VerifyAfter) {
    $evidenceRoot = Join-Path $repo 'deliverables\evidence\cleanup-20260919'
    $executionLog = Join-Path $evidenceRoot 'execution-log.json'
    $protectedBeforePath = Join-Path $evidenceRoot 'protected-before.json'
    if (-not (Test-Path -LiteralPath $executionLog -PathType Leaf) -or -not (Test-Path -LiteralPath $protectedBeforePath -PathType Leaf)) {
        throw '缺少既有执行日志或删除前保护快照。'
    }
    $executionState = Get-Content -LiteralPath $executionLog -Raw -Encoding utf8 | ConvertFrom-Json
    if (@($executionState.DeletedRoots).Count -ne 99 -or @($executionState.DeletedLinks).Count -ne 26) {
        throw '既有执行日志未记录完整的 99 根/26 链接删除。'
    }
    foreach ($target in $targets) {
        if (Test-Path -LiteralPath $target.Path) { throw "已执行候选仍存在：$($target.Path)" }
    }
    $protectedBefore = Get-Content -LiteralPath $protectedBeforePath -Raw -Encoding utf8 | ConvertFrom-Json
    $protectedAfter = Get-ProtectedSnapshot @($protectedPaths)
    Assert-SameProtectedSnapshot $protectedBefore $protectedAfter
    Write-JsonFile (Join-Path $evidenceRoot 'protected-after.json') $protectedAfter
    $preservedLink = Get-Item -LiteralPath (Join-Path $repoOut 'symlink-capability-check\link.txt') -Force
    if (-not ($preservedLink.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw '范围外 symlink-capability-check 链接未保留。'
    }
    if (-not (Test-Path -LiteralPath 'D:\develop_env\Qt\Qt5.11.3\5.11.3')) { throw '本机 Qt 路径异常。' }
    $executionState.Status = 'complete-after-postcheck-retry'
    $executionState.FailedItem = $null
    $executionState | Add-Member -NotePropertyName PostcheckNote -NotePropertyValue 'Initial delete completed; the first postcheck stopped on an empty Compare-Object .Count strict-mode bug. This read-only retry verified protected hashes and paths.' -Force
    $executionState.CompletedUtc = [datetime]::UtcNow.ToString('o')
    Write-JsonFile $executionLog $executionState
    Write-Host '删除后只读复核完成：99 个候选根均不存在，26 个范围内链接已删除，保护集合哈希一致，范围外链接仍保留。'
    return
}

if ((Test-SamePath $repo $repoOut) -or (Test-SamePath $repo $workspace)) { throw '仓库边界断言失败。' }
foreach ($target in $targets) {
    if (-not (Test-Path -LiteralPath $target.Path -PathType Container)) { throw "候选目录不存在：$($target.Path)" }
    if (-not (Test-IsChildPath $target.Path $target.Scope)) { throw "候选不在预期 scope：$($target.Path)" }
    if ((Test-SamePath $target.Path $repo) -or (Test-SamePath $target.Path $repoOut) -or (Test-SamePath $target.Path $workspace)) { throw "禁止删除父根：$($target.Path)" }
    $targetItem = Get-Item -LiteralPath $target.Path -Force
    if ($targetItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "候选根本身是重解析点：$($target.Path)" }
    Assert-NoReparseAncestor $target.Path
    foreach ($protected in $protectedPaths) {
        if ((Test-SamePath $target.Path $protected) -or (Test-IsChildPath $target.Path $protected) -or (Test-IsChildPath $protected $target.Path)) {
            throw "候选与保护路径重叠：$($target.Path) <-> $protected"
        }
    }
}
for ($i = 0; $i -lt $targets.Count; $i++) {
    for ($j = $i + 1; $j -lt $targets.Count; $j++) {
        if ((Test-SamePath $targets[$i].Path $targets[$j].Path) -or (Test-IsChildPath $targets[$i].Path $targets[$j].Path) -or (Test-IsChildPath $targets[$j].Path $targets[$i].Path)) {
            throw "候选存在父子/重复关系：$($targets[$i].Path) <-> $($targets[$j].Path)"
        }
    }
}

Assert-NoActiveBuildProcesses

$cloneHead = (& git -C $clonePath rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $cloneHead -ne '98df5e0d844413fb6ad16a75dfceedcf17f2f1d6') { throw 'clean clone HEAD 不匹配。' }
$cloneStatus = @(& git -C $clonePath status --porcelain)
if ($LASTEXITCODE -ne 0 -or $cloneStatus.Count -ne 0) { throw 'clean clone 不干净。' }
& git -C $clonePath symbolic-ref -q HEAD *> $null
if ($LASTEXITCODE -eq 0) { throw 'clean clone 不是 detached HEAD。' }
$cloneWorktrees = @(& git -C $clonePath worktree list --porcelain | Where-Object { $_ -like 'worktree *' })
if ($LASTEXITCODE -ne 0 -or $cloneWorktrees.Count -ne 1) { throw 'clean clone 存在额外 worktree。' }
& git -C $repo merge-base --is-ancestor $cloneHead HEAD
if ($LASTEXITCODE -ne 0) { throw 'clean clone HEAD 不是当前仓库 HEAD 的祖先，可能存在独有提交。' }

$expectedLinks = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$v06Bases = @(
    'b42bd', 'b42br', 'build\windows-msvc-protocol-lab', 'dec042b-c2-run-debug',
    'dec042b-c2-run-release', 'dec042b-c3-debug', 'dec042b-c3-release-msvc'
)
foreach ($base in $v06Bases) {
    $prefix = Join-Path $repoOut "$base\tests\protocol_lab_v06_evidence\v06-evidence-runs"
    [void]$expectedLinks.Add((Normalize-Path (Join-Path $prefix 'negative_directory_link\inputs')))
    [void]$expectedLinks.Add((Normalize-Path (Join-Path $prefix 'negative_manifest_link\SHA256SUMS')))
    [void]$expectedLinks.Add((Normalize-Path (Join-Path $prefix 'negative_payload_link\inputs\protocol.pae.json')))
}
$v07Bases = @('build\windows-msvc-protocol-lab', 'dec042b-c2-run-debug', 'dec042b-c2-run-release', 'dec042b-c3-debug', 'dec042b-c3-release-msvc')
foreach ($base in $v07Bases) {
    [void]$expectedLinks.Add((Normalize-Path (Join-Path $repoOut "$base\tests\protocol_lab_v07_run_evidence\v07_run_evidence_test_output\run_reader_link")))
}
[void]$expectedLinks.Add((Normalize-Path (Join-Path $repoOut 'symlink-capability-check\link.txt')))
if ($expectedLinks.Count -ne 27) { throw '预期重解析点数量断言失败。' }

$outEntries = @(Get-TreeEntriesNoFollow $repoOut)
$actualLinks = @($outEntries | Where-Object IsReparse)
if ($actualLinks.Count -ne 27) { throw "out 重解析点数量变化：$($actualLinks.Count)" }
foreach ($link in $actualLinks) {
    if (-not $expectedLinks.Contains((Normalize-Path $link.FullName))) { throw "发现未知重解析点：$($link.FullName)" }
}
foreach ($expected in $expectedLinks) {
    if (-not (@($actualLinks | Where-Object { Test-SamePath $_.FullName $expected }).Count -eq 1)) { throw "预期重解析点缺失：$expected" }
}
$linksToRemove = @($actualLinks | Where-Object { $null -ne (Get-ContainingTarget $_.FullName $targets) })
$linksToPreserve = @($actualLinks | Where-Object { $null -eq (Get-ContainingTarget $_.FullName $targets) })
if ($linksToRemove.Count -ne 26 -or $linksToPreserve.Count -ne 1 -or -not (Test-SamePath $linksToPreserve[0].FullName (Join-Path $repoOut 'symlink-capability-check\link.txt'))) {
    throw '重解析点删除/保护分区与预期不符。'
}

Write-Host '统计 99 个固定候选（不跟随重解析点）...'
$initialStats = [Collections.Generic.List[object]]::new()
$evidenceItems = [Collections.Generic.List[object]]::new()
$coreEvidenceItems = [Collections.Generic.List[object]]::new()
for ($index = 0; $index -lt $targets.Count; $index++) {
    $target = $targets[$index]
    $stats = Get-TreeStats $target.Path
    $initialStats.Add([pscustomobject]@{
        Index = $index + 1
        Category = $target.Category
        Path = $target.Path
        Files = $stats.Files
        Directories = $stats.Directories
        ReparsePoints = $stats.ReparsePoints
        Bytes = $stats.Bytes
        Signature = $stats.Signature
    })
    foreach ($entry in $stats.Entries) {
        if (Test-IsEvidenceFile $entry $target.Path) {
            $relative = [IO.Path]::GetRelativePath($target.Path, $entry.FullName)
            $label = ('{0:D3}_{1}' -f ($index + 1), ([IO.Path]::GetFileName($target.Path) -replace '[^A-Za-z0-9._-]', '_'))
            $evidenceItems.Add([pscustomobject]@{
                TargetIndex = $index + 1
                Category = $target.Category
                SourceRoot = $target.Path
                SourcePath = $entry.FullName
                RelativePath = $relative
                DestinationRelativePath = Join-Path (Join-Path 'targets' $label) $relative
                Length = $entry.Length
                MtimeUtc = $entry.LastWriteTimeUtc.ToString('o')
            })
        }
        if (($target.Category -eq 'out-build-root' -or $target.Category -eq 'old-external-sdk') -and
            (Test-IsCoreInventoryEvidence $entry $target.Path)) {
            $coreEvidenceItems.Add([pscustomobject]@{ SourcePath = $entry.FullName; Length = $entry.Length })
        }
    }
    Write-Host ('[{0:D3}/099] {1}: {2} files, {3} bytes, {4} links' -f ($index + 1), $target.Category, $stats.Files, $stats.Bytes, $stats.ReparsePoints)
}

$totalBytes = [int64](($initialStats | Measure-Object Bytes -Sum).Sum)
$totalFiles = [int64](($initialStats | Measure-Object Files -Sum).Sum)
$evidenceBytes = [int64](($evidenceItems | Measure-Object Length -Sum).Sum)
$coreEvidenceBytes = [int64](($coreEvidenceItems | Measure-Object Length -Sum).Sum)
if ($coreEvidenceItems.Count -ne 337 -or $coreEvidenceBytes -ne 1342277) {
    throw "核心证据集变化：$($coreEvidenceItems.Count) files / $coreEvidenceBytes bytes"
}
$destinationKeys = @($evidenceItems | ForEach-Object { $_.DestinationRelativePath.ToLowerInvariant() })
if (($destinationKeys | Sort-Object -Unique).Count -ne $destinationKeys.Count) {
    throw '证据目标相对路径发生碰撞。'
}
$summary = [pscustomobject]@{
    Mode = if ($Execute) { 'execute' } else { 'dry-run' }
    CandidateRoots = $targets.Count
    CandidateFiles = $totalFiles
    CandidateLogicalBytes = $totalBytes
    EvidenceFiles = $evidenceItems.Count
    EvidenceLogicalBytes = $evidenceBytes
    CoreInventoryEvidenceFiles = $coreEvidenceItems.Count
    CoreInventoryEvidenceBytes = $coreEvidenceBytes
    ReparsePointsToRemove = $linksToRemove.Count
    ReparsePointsToPreserve = $linksToPreserve.Count
}
$summary | Format-List

if (-not $Execute) {
    Write-Host 'Dry-run 完成；未创建、复制或删除任何内容。使用 -Execute 才会执行。'
    return
}

$evidenceParent = Join-Path $repo 'deliverables\evidence'
[IO.Directory]::CreateDirectory($evidenceParent) | Out-Null
$evidenceRoot = Join-Path $evidenceParent 'cleanup-20260919'
$suffix = 2
while (Test-Path -LiteralPath $evidenceRoot) {
    $evidenceRoot = Join-Path $evidenceParent ("cleanup-20260919-$suffix")
    $suffix++
}
[IO.Directory]::CreateDirectory($evidenceRoot) | Out-Null

$protectedBefore = Get-ProtectedSnapshot @($protectedPaths)
Write-JsonFile (Join-Path $evidenceRoot 'protected-before.json') $protectedBefore
Write-JsonFile (Join-Path $evidenceRoot 'candidate-roots-before.json') @($initialStats)
Write-JsonFile (Join-Path $evidenceRoot 'reparse-points.json') @($actualLinks | ForEach-Object {
    $owner = Get-ContainingTarget $_.FullName $targets
    [pscustomobject]@{
        Path = $_.FullName
        IsDirectory = $_.IsDirectory
        SavedTarget = @($_.LinkTarget)
        Disposition = if ($null -ne $owner) { 'remove-link-object-before-owner-root' } else { 'preserve-outside-approved-roots' }
        OwnerRoot = if ($null -ne $owner) { $owner.Path } else { $null }
    }
})

$markdownFiles = @(Get-ChildItem -LiteralPath $repo -Filter '*.md' -File -Recurse -Force | Where-Object {
    -not (Test-IsChildPath $_.FullName $repoOut) -and
    -not (Test-IsChildPath $_.FullName (Join-Path $repo 'deliverables')) -and
    -not (Test-IsChildPath $_.FullName (Join-Path $repo '.git'))
})
$references = [Collections.Generic.List[object]]::new()
foreach ($target in $targets) {
    $leaf = [IO.Path]::GetFileName($target.Path)
    foreach ($file in $markdownFiles) {
        $lineNumber = 0
        foreach ($line in [IO.File]::ReadLines($file.FullName, [Text.Encoding]::UTF8)) {
            $lineNumber++
            if ($line.Contains($target.Path, [StringComparison]::OrdinalIgnoreCase) -or ($leaf.Length -ge 12 -and $line.Contains($leaf, [StringComparison]::OrdinalIgnoreCase))) {
                $references.Add([pscustomobject]@{ CandidateRoot = $target.Path; Report = $file.FullName; Line = $lineNumber; Text = $line.Trim() })
            }
        }
    }
}
Write-JsonFile (Join-Path $evidenceRoot 'report-references.json') @($references)

Write-Host "归集证据到 $evidenceRoot ..."
$manifest = [Collections.Generic.List[object]]::new()
foreach ($item in $evidenceItems) {
    $destination = Join-Path $evidenceRoot $item.DestinationRelativePath
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination)) | Out-Null
    $sourceHash = Get-Sha256 $item.SourcePath
    Copy-Item -LiteralPath $item.SourcePath -Destination $destination -Force
    [IO.File]::SetLastWriteTimeUtc($destination, [datetime]::Parse($item.MtimeUtc, $null, [Globalization.DateTimeStyles]::RoundtripKind))
    $destinationInfo = Get-Item -LiteralPath $destination -Force
    $destinationHash = Get-Sha256 $destination
    if ($destinationInfo.Length -ne $item.Length -or $sourceHash -ne $destinationHash) {
        throw "证据复制校验失败：$($item.SourcePath)"
    }
    $manifest.Add([pscustomobject]@{
        TargetIndex = $item.TargetIndex
        Category = $item.Category
        SourceRoot = $item.SourceRoot
        SourcePath = $item.SourcePath
        RelativePath = $item.RelativePath
        DestinationRelativePath = $item.DestinationRelativePath
        Length = $item.Length
        MtimeUtc = $item.MtimeUtc
        Sha256Source = $sourceHash
        Sha256Destination = $destinationHash
        Verified = $true
        ReparsePoint = $false
    })
}
Write-JsonFile (Join-Path $evidenceRoot 'evidence-manifest.json') @($manifest)

foreach ($record in $manifest) {
    if (-not (Test-Path -LiteralPath $record.SourcePath -PathType Leaf)) { throw "证据源在删除前消失：$($record.SourcePath)" }
    if ((Get-Item -LiteralPath $record.SourcePath -Force).Length -ne $record.Length -or (Get-Sha256 $record.SourcePath) -ne $record.Sha256Source) {
        throw "证据源在复制后变化：$($record.SourcePath)"
    }
}

Assert-NoActiveBuildProcesses
foreach ($stat in $initialStats) {
    $current = Get-TreeStats $stat.Path
    if ($current.Signature -ne $stat.Signature) { throw "删除前目录发生变化：$($stat.Path)" }
}

$executionState = [ordered]@{
    Status = 'deleting'
    StartedUtc = [datetime]::UtcNow.ToString('o')
    EvidenceRoot = $evidenceRoot
    SummaryBefore = $summary
    DeletedLinks = @()
    DeletedRoots = @()
    FailedItem = $null
    CompletedUtc = $null
}
$executionLog = Join-Path $evidenceRoot 'execution-log.json'
Write-JsonFile $executionLog $executionState

try {
    foreach ($link in ($linksToRemove | Sort-Object FullName -Descending)) {
        $current = Get-Item -LiteralPath $link.FullName -Force
        if (-not ($current.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "链接对象状态变化：$($link.FullName)" }
        if ($link.IsDirectory) { [IO.Directory]::Delete($link.FullName, $false) } else { [IO.File]::Delete($link.FullName) }
        if (Test-Path -LiteralPath $link.FullName) { throw "链接对象删除失败：$($link.FullName)" }
        $executionState.DeletedLinks += $link.FullName
        Write-JsonFile $executionLog $executionState
    }
    foreach ($target in $targets) {
        $remainingLinks = @((Get-TreeEntriesNoFollow $target.Path) | Where-Object IsReparse)
        if ($remainingLinks.Count -ne 0) { throw "候选根仍含重解析点：$($target.Path)" }
    }
    foreach ($target in $targets) {
        Write-Host "删除精确根：$($target.Path)"
        Remove-Item -LiteralPath $target.Path -Recurse -Force
        if (Test-Path -LiteralPath $target.Path) { throw "候选根删除后仍存在：$($target.Path)" }
        $executionState.DeletedRoots += $target.Path
        Write-JsonFile $executionLog $executionState
    }
    $protectedAfter = Get-ProtectedSnapshot @($protectedPaths)
    Assert-SameProtectedSnapshot $protectedBefore $protectedAfter
    Write-JsonFile (Join-Path $evidenceRoot 'protected-after.json') $protectedAfter
    if (-not (Test-Path -LiteralPath (Join-Path $repoOut 'symlink-capability-check\link.txt'))) {
        throw '范围外 symlink-capability-check 链接未保留。'
    }
    if (-not (Test-Path -LiteralPath 'D:\develop_env\Qt\Qt5.11.3\5.11.3')) {
        throw '本机 Qt 路径异常。'
    }
    $executionState.Status = 'complete'
    $executionState.CompletedUtc = [datetime]::UtcNow.ToString('o')
    Write-JsonFile $executionLog $executionState
} catch {
    $executionState.Status = 'failed'
    $executionState.FailedItem = $_.Exception.Message
    $executionState.CompletedUtc = [datetime]::UtcNow.ToString('o')
    Write-JsonFile $executionLog $executionState
    throw
}

Write-Host ('完成：删除 {0} 个根、{1} 个链接对象；删除前逻辑量 {2} bytes；归集 {3} 文件、{4} bytes。' -f $executionState.DeletedRoots.Count, $executionState.DeletedLinks.Count, $totalBytes, $manifest.Count, $evidenceBytes)
