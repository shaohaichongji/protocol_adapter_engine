[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceBundleRoot,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceHead,
    [Parameter(Mandatory = $true)][string]$FixedSourceRoot,
    [Parameter(Mandatory = $true)][string]$DestinationRoot,
    [Parameter(Mandatory = $true)][string]$OverlayIdentity,
    [string]$DocumentationRoot = (Join-Path $PSScriptRoot '../docs/experience')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$source = [IO.Path]::GetFullPath($SourceBundleRoot).TrimEnd('\', '/')
$destination = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\', '/')
$fixedSource = [IO.Path]::GetFullPath($FixedSourceRoot)
$docs = [IO.Path]::GetFullPath($DocumentationRoot)
$name = Split-Path $source -Leaf
if (Test-Path -LiteralPath $destination) { throw 'Target exists; do not overwrite an earlier bundle.' }
if ($destination.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    $destination.StartsWith($docs.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Target must be outside source and documents' }
$fixedHead = git -C $fixedSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $fixedHead.Trim() -cne $ExpectedSourceHead) { throw 'Unexpected fixed-source HEAD' }
$fixedStatus = @(git -C $fixedSource status --porcelain)
if ($LASTEXITCODE -ne 0 -or $fixedStatus.Count) { throw 'Fixed-source snapshot must be clean' }
$identity = Get-Content -LiteralPath (Join-Path $source 'BUNDLE-IDENTITY.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($identity.product_source_head -cne $ExpectedSourceHead) { throw 'Unexpected base identity' }
$baseHashes = @{}; $baseLengths = @{}
foreach ($line in Get-Content -LiteralPath (Join-Path $source 'MANIFEST.txt') -Encoding UTF8) {
    if ($line.StartsWith('#') -or -not $line.Trim()) { continue }
    if ($line -cnotmatch '^([0-9]+)  (.+)$') { throw 'Invalid base manifest entry' }
    $length = [int64]$Matches[1]; $relative = $Matches[2]
    if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or $baseLengths.ContainsKey($relative)) { throw 'Unsafe or duplicate base manifest path' }
    $baseLengths[$relative] = $length
}
foreach ($line in Get-Content -LiteralPath (Join-Path $source 'SHA256SUMS.txt') -Encoding UTF8) {
    if ($line.StartsWith('#') -or -not $line.Trim()) { continue }
    if ($line -cnotmatch '^([a-fA-F0-9]{64})  (.+)$') { throw 'Invalid base hash entry' }
    $expected = $Matches[1].ToLowerInvariant(); $relative = $Matches[2]
    if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or $baseHashes.ContainsKey($relative)) { throw 'Unsafe or duplicate base hash path' }
    $baseHashes[$relative] = $expected
}
$baseFiles = @(Get-ChildItem -LiteralPath $source -File -Recurse)
foreach ($file in $baseFiles) {
    $relative = $file.FullName.Substring($source.Length + 1).Replace('\', '/')
    if ($relative -in @('MANIFEST.txt', 'SHA256SUMS.txt')) { continue }
    if (-not $baseHashes.ContainsKey($relative) -or -not $baseLengths.ContainsKey($relative) -or
        $baseLengths[$relative] -ne $file.Length -or (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant() -cne $baseHashes[$relative]) { throw "Base mismatch: $relative" }
}
if ($baseHashes.Count -ne $baseFiles.Count - 2 -or $baseLengths.Count -ne $baseFiles.Count - 2) { throw 'Incomplete base lists' }
$tutorialFiles = @('README.md', '01-配置结构与二进制.md', '02-文本与流式.md', '03-构建运行教学程序.md',
    '04-常用配置字段速查.md', '05-从协议表到SDK练习.md', '01-最小二进制.pae.json', '02-最小文本.pae.json', 'CMakeLists.txt', 'verify.cpp')
$overlayFiles = @('README.md', '01-Lab体验.md', '02-SDK接入.md', '03-配置与边界.md') +
    @($tutorialFiles | ForEach-Object { "tutorials/$_" })
$docIdentity = Get-Content -LiteralPath (Join-Path $docs 'DOCUMENTATION-IDENTITY.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($docIdentity.product_source_head -cne $ExpectedSourceHead -or $docIdentity.version -cne $OverlayIdentity) { throw 'Unexpected tutorial documentation identity' }
foreach ($relative in $overlayFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $docs $relative) -PathType Leaf)) { throw "Missing overlay: $relative" }
    $entry = @($docIdentity.files | Where-Object { $_.path -ceq $relative })
    if ($entry.Count -ne 1 -or (Get-FileHash -LiteralPath (Join-Path $docs $relative)).Hash.ToLowerInvariant() -cne $entry[0].sha256) { throw "Tutorial document hash mismatch: $relative" }
}
foreach ($relative in @('tutorials/01-最小二进制.pae.json', 'tutorials/02-最小文本.pae.json', 'tutorials/CMakeLists.txt', 'tutorials/verify.cpp')) {
    if ((Get-FileHash -LiteralPath (Join-Path $docs $relative)).Hash -cne
        (Get-FileHash -LiteralPath (Join-Path $fixedSource "docs/experience/$relative")).Hash) { throw "Teaching code differs from fixed source: $relative" }
}
if (@(Get-ChildItem -LiteralPath $source -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Reparse point in base bundle' }
New-Item -ItemType Directory -Path $destination | Out-Null
$target = Join-Path $destination $name
Copy-Item -LiteralPath $source -Destination $target -Recurse
$entries = @()
foreach ($relative in $overlayFiles) {
    $path = Join-Path $target $relative
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $docs $relative) -Destination $path -Force
    $entries += [ordered]@{ path = $relative; sha256 = (Get-FileHash -LiteralPath $path).Hash.ToLowerInvariant() }
}
$identity | Add-Member -NotePropertyName tutorial_overlay -NotePropertyValue ([ordered]@{
    version = $OverlayIdentity
    product_source_head = $ExpectedSourceHead
    base_identity_sha256 = (Get-FileHash -LiteralPath (Join-Path $source 'BUNDLE-IDENTITY.json')).Hash.ToLowerInvariant()
    files = $entries
}) -Force
$identity | Add-Member -NotePropertyName tutorial_note -NotePropertyValue 'Documentation is derived; teaching code/configs equal the fixed source; SDK and Lab payload unchanged.' -Force
$encoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText((Join-Path $target 'BUNDLE-IDENTITY.json'), ($identity | ConvertTo-Json -Depth 12), $encoding)
foreach ($file in $baseFiles) {
    $relative = $file.FullName.Substring($source.Length + 1).Replace('\', '/')
    if ($relative -in $overlayFiles + @('BUNDLE-IDENTITY.json', 'MANIFEST.txt', 'SHA256SUMS.txt')) { continue }
    if ((Get-FileHash -LiteralPath $file.FullName).Hash -cne (Get-FileHash -LiteralPath (Join-Path $target $relative)).Hash) { throw "Non-overlay byte changed: $relative" }
}
foreach ($file in Get-ChildItem -LiteralPath $target -Recurse -File -Filter '*.md') {
    $body = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    $links = @([regex]::Matches($body, '\[[^\]]+\]\(([^)]+)\)') | ForEach-Object { $_.Groups[1].Value }) +
        @([regex]::Matches($body, '(?m)^\[[^\]]+\]:\s*(\S+)') | ForEach-Object { $_.Groups[1].Value })
    foreach ($link in $links) {
        $link = $link.Trim('<', '>', '"', ' ')
        if ($link -match '^(https?://|mailto:|#)' -or -not $link) { continue }
        $relative = [uri]::UnescapeDataString(($link -split '[#?]', 2)[0])
        $path = [IO.Path]::GetFullPath((Join-Path $file.DirectoryName $relative))
        if (-not $path.StartsWith($target + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $path)) { throw "Missing or external local document link: $link in $($file.Name)" }
    }
}
$files = @(Get-ChildItem -LiteralPath $target -File -Recurse | Where-Object { $_.Name -notin @('MANIFEST.txt', 'SHA256SUMS.txt') -or $_.DirectoryName -ne $target } | Sort-Object FullName)
$hashes = @(); $manifest = @()
foreach ($file in $files) {
    $relative = $file.FullName.Substring($target.Length + 1).Replace('\', '/')
    $hashes += "$((Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant())  $relative"
    $manifest += "$($file.Length)  $relative"
}
[IO.File]::WriteAllLines((Join-Path $target 'SHA256SUMS.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $hashes, $encoding)
[IO.File]::WriteAllLines((Join-Path $target 'MANIFEST.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $manifest, $encoding)
$zip = Join-Path $destination "$name.zip"
Compress-Archive -LiteralPath $target -DestinationPath $zip
[IO.File]::WriteAllText((Join-Path $destination 'ZIP.sha256'), "$((Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant())  $name.zip" + [Environment]::NewLine, $encoding)
Write-Output "Created $destination"
