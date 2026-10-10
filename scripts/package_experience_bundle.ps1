[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceHead,
    [Parameter(Mandatory = $true)][string]$SdkInputRoot,
    [Parameter(Mandatory = $true)][string]$LabInputRoot,
    [Parameter(Mandatory = $true)][string]$LabProvenancePath,
    [Parameter(Mandatory = $true)][string]$LabHashesPath,
    [Parameter(Mandatory = $true)][string]$LabBuildIdentityPath,
    [Parameter(Mandatory = $true)][string]$FixedSourceRoot,
    [Parameter(Mandatory = $true)][string]$DestinationRoot,
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9._-]+$')][string]$BundleName,
    [Parameter(Mandatory = $true)][string]$SdkOverlayIdentity,
    [Parameter(Mandatory = $true)][string]$DocumentationIdentity,
    [string]$DocumentationRoot = (Join-Path $PSScriptRoot '../docs/experience'),
    [hashtable]$SdkPackageDirectories = @{
        'pae-sdk-source' = 'source'
        'pae-sdk-static-debug' = 'static-Debug'
        'pae-sdk-static-release' = 'static-Release'
        'pae-sdk-shared-debug' = 'shared-Debug'
        'pae-sdk-shared-release' = 'shared-Release'
    }
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Get-DirectoryPath([string]$Path) {
    return [IO.Path]::TrimEndingDirectorySeparator([IO.Path]::GetFullPath($Path))
}
function Get-PayloadHashes([string]$Root) {
    $result = @{}
    foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File -Force) {
        $result[$file.FullName.Substring($Root.Length + 1).Replace('\', '/')] = (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    }
    return $result
}
function Assert-CopiedHashes([string]$Root, [hashtable]$Expected) {
    $actual = Get-PayloadHashes $Root
    if ($actual.Count -ne $Expected.Count) { throw "Copied payload count mismatch: $Root" }
    foreach ($relative in $Expected.Keys) {
        if (-not $actual.ContainsKey($relative) -or $actual[$relative] -cne $Expected[$relative]) { throw "Copied payload hash mismatch: $relative" }
    }
}
function Assert-EvidenceHash([string]$Path, [string]$Expected) {
    if ($Expected -cnotmatch '^[0-9a-fA-F]{64}$' -or (Get-FileHash -LiteralPath $Path).Hash.ToLowerInvariant() -cne $Expected.ToLowerInvariant()) { throw "Build evidence hash mismatch: $Path" }
}
function Assert-LabBuildIdentity([string]$IdentityPath, [string]$PreparationPath, [string]$DeploymentHashesPath,
    [string]$DeploymentRoot, [string]$FrozenSource, [string]$Head, [hashtable]$SdkProvenances) {
    $identity = Get-Content -LiteralPath $IdentityPath -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($identity.schema_version -ne 1 -or $identity.product_source_head -cne $Head -or
        $identity.package_kind -cne 'static' -or $identity.configuration -cne 'Release' -or
        $identity.source_worktree_dirty -cne $false -or @($identity.uncommitted_patches).Count -ne 0) { throw 'Lab actual build is not the final clean static Release source' }
    Assert-EvidenceHash $PreparationPath $identity.preparation_provenance_sha256
    Assert-EvidenceHash $DeploymentHashesPath $identity.deployment_hashes_sha256
    Assert-EvidenceHash $identity.actual_input_manifest_path $identity.actual_input_manifest_sha256
    if ($identity.build.exit_code -ne 0 -or
        $identity.build.input_manifest_before_sha256 -cne $identity.actual_input_manifest_sha256 -or
        $identity.build.input_manifest_after_sha256 -cne $identity.actual_input_manifest_sha256) { throw 'Lab build failed or actual inputs changed during build' }
    Assert-EvidenceHash $identity.build.log_path $identity.build.log_sha256
    Assert-EvidenceHash $identity.build.executable_path $identity.build.executable_sha256
    Assert-EvidenceHash (Join-Path $DeploymentRoot 'pae_protocol_lab_ui.exe') $identity.build.executable_sha256
    if ($identity.ctest.exit_code -ne 0) { throw 'Required Lab CTest did not pass' }
    Assert-EvidenceHash $identity.ctest.result_path $identity.ctest.result_sha256
    [xml]$testXml = Get-Content -LiteralPath $identity.ctest.result_path -Raw -Encoding UTF8
    $tests = @($testXml.SelectNodes('/Site/Testing/Test'))
    $requiredTests = @('yaml_entry', 'yaml_ui_smoke', 'compile_queue', 'document_state', 'schema_dispatch', 'unicode_literal')
    if ($tests.Count -ne $requiredTests.Count -or @($tests | Where-Object { $_.Status -cne 'passed' }).Count) { throw 'Required Lab CTest results are incomplete or failed' }
    foreach ($name in $requiredTests) {
        if (@($tests | Where-Object { $_.Name -ceq "pae.tools.protocol_lab_ui.$name" }).Count -ne 1) { throw "Required Lab CTest missing: $name" }
    }
    if ($identity.ui_smoke.exit_code -ne 0) { throw 'Lab UI smoke failed' }
    Assert-EvidenceHash $identity.ui_smoke.log_path $identity.ui_smoke.log_sha256
    if (-not (Get-Content -LiteralPath $identity.ui_smoke.log_path -Raw -Encoding UTF8).Contains('UI_SMOKE_PASS detail=2 document(s)')) { throw 'Lab UI smoke success marker missing' }
    $inputRoot = Get-DirectoryPath $identity.actual_input_root
    $entries = @(Get-Content -LiteralPath $identity.actual_input_manifest_path -Raw -Encoding UTF8 | ConvertFrom-Json)
    $inputHashes = @{}
    foreach ($entry in $entries) {
        if ($entry.path -notmatch '^inputs/(lab|sdk|qt|yyjson)/' -or $entry.path -match '(\\|(^|/)\.\.(/|$))' -or $inputHashes.ContainsKey($entry.path)) { throw 'Unsafe or duplicate actual Lab input path' }
        $file = Get-Item -LiteralPath (Join-Path $inputRoot $entry.path)
        if ($file.Length -ne $entry.bytes) { throw "Actual Lab input length mismatch: $($entry.path)" }
        Assert-EvidenceHash $file.FullName $entry.sha256
        $inputHashes[$entry.path] = $entry.sha256.ToLowerInvariant()
        $frozenRelative = $null
        if ($entry.path.StartsWith('inputs/lab/')) {
            $relative = $entry.path.Substring('inputs/lab/'.Length)
            if ($relative.StartsWith('standalone-configs/')) {
                if ($relative -eq 'standalone-configs/synthetic_ui_max.pae.json') { continue } # Generated by the frozen generator checked below.
                $sameSource = @($entries | Where-Object { $_.path.StartsWith('inputs/lab/') -and -not $_.path.StartsWith('inputs/lab/standalone-configs/') -and
                    [IO.Path]::GetFileName($_.path) -ceq [IO.Path]::GetFileName($relative) -and $_.sha256 -ieq $entry.sha256 })
                if (-not $sameSource.Count) { throw "Unbound standalone config: $relative" }
                $frozenRelative = $sameSource[0].path.Substring('inputs/lab/'.Length)
            } else { $frozenRelative = $relative }
        } elseif ($entry.path.StartsWith('inputs/qt/')) { $frozenRelative = 'third_party/qt/' + $entry.path.Substring('inputs/qt/'.Length)
        } elseif ($entry.path.StartsWith('inputs/yyjson/')) { $frozenRelative = 'third_party/yyjson/' + $entry.path.Substring('inputs/yyjson/'.Length) }
        if ($frozenRelative) { Assert-EvidenceHash (Join-Path $FrozenSource $frozenRelative) $entry.sha256 }
    }
    foreach ($required in @('inputs/lab/tools/protocol_lab_ui/standalone/CMakeLists.txt', 'inputs/lab/cmake/GenerateProtocolLabUiMaxFixture.cmake')) {
        if (-not $inputHashes.ContainsKey($required)) { throw "Actual Lab input missing: $required" }
    }
    $actualFiles = @(Get-ChildItem -LiteralPath (Join-Path $inputRoot 'inputs') -Recurse -File -Force)
    if ($actualFiles.Count -ne $inputHashes.Count) { throw 'Actual Lab input manifest is incomplete' }
    if (@(Get-ChildItem -LiteralPath (Join-Path $inputRoot 'inputs') -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw 'Reparse point in actual Lab inputs' }
    foreach ($configuration in @('debug', 'release')) {
        $sdkRoot = Join-Path $inputRoot "inputs/sdk/pae-sdk-static-$configuration"
        & $verifyScript -PackageRoot $sdkRoot -Kind static -HasYaml:$true
        if (-not $?) { throw 'Actual Lab SDK verification failed' }
        $sdk = $SdkProvenances["pae-sdk-static-$configuration"]
        Assert-EvidenceHash (Join-Path $sdkRoot 'PROVENANCE.json') $sdk.base_package_provenance_sha256
        Assert-EvidenceHash (Join-Path $sdkRoot 'SHA256SUMS.txt') $sdk.base_package_sha256s_sha256
    }
}
$sdkInput = Get-DirectoryPath $SdkInputRoot
$labInput = Get-DirectoryPath $LabInputRoot
$fixedSource = Get-DirectoryPath $FixedSourceRoot
$docs = Get-DirectoryPath $DocumentationRoot
$deliveryRoot = Get-DirectoryPath $DestinationRoot
$bundleRoot = Join-Path $deliveryRoot $BundleName
$zipPath = Join-Path $deliveryRoot "$BundleName.zip"
$encoding = [Text.UTF8Encoding]::new($false)
$verifyScript = Join-Path $PSScriptRoot 'verify_yaml_sdk_package.ps1'
if (Test-Path -LiteralPath $deliveryRoot) { throw "Delivery target already exists: $deliveryRoot" }
foreach ($inputRoot in @($sdkInput, $labInput, $fixedSource, $docs)) {
    if (-not (Test-Path -LiteralPath $inputRoot -PathType Container)) { throw "Missing input: $inputRoot" }
    if ($deliveryRoot.Equals($inputRoot, [StringComparison]::OrdinalIgnoreCase) -or
        $deliveryRoot.StartsWith($inputRoot.TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Delivery target must be outside input roots'
    }
    if (@(Get-ChildItem -LiteralPath $inputRoot -Recurse -Force |
        Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }).Count) { throw "Reparse point in input: $inputRoot" }
}
$fixedHead = git -C $fixedSource rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $fixedHead.Trim() -cne $ExpectedSourceHead) { throw 'Unexpected fixed-source HEAD' }
$fixedStatus = @(git -C $fixedSource status --porcelain)
if ($LASTEXITCODE -ne 0 -or $fixedStatus.Count) { throw 'Fixed-source snapshot must be clean' }
$packages = @('pae-sdk-source', 'pae-sdk-static-debug', 'pae-sdk-static-release', 'pae-sdk-shared-debug', 'pae-sdk-shared-release')
if ($SdkPackageDirectories.Count -ne 5) { throw 'Exactly five SDK inputs are required' }
$sdkIdentity = @()
$sdkProvenances = @{}
$sdkPayloadHashes = @{}
foreach ($name in $packages) {
    if (-not $SdkPackageDirectories.ContainsKey($name)) { throw "SDK mapping missing: $name" }
    $relative = [string]$SdkPackageDirectories[$name]
    if ([IO.Path]::IsPathRooted($relative) -or $relative -match '(^|[\\/])\.\.([\\/]|$)') { throw 'Unsafe SDK input mapping' }
    $inputRoot = Get-DirectoryPath (Join-Path $sdkInput $relative)
    $provenance = Get-Content -LiteralPath (Join-Path $inputRoot 'PROVENANCE.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $kind = if ($name -eq 'pae-sdk-source') { 'source' } elseif ($name -like 'pae-sdk-static-*') { 'static' } else { 'shared' }
    $configuration = if ($kind -eq 'source') { 'Debug+Release' } elseif ($name.EndsWith('debug')) { 'Debug' } else { 'Release' }
    if ($provenance.source_head -cne $ExpectedSourceHead -or $provenance.source_worktree_dirty -or
        $provenance.configuration -cne $configuration -or $provenance.architecture -cne 'x64' -or
        $provenance.documentation_overlay.version -cne $SdkOverlayIdentity -or -not $provenance.derived_package) { throw "Unexpected SDK identity: $name" }
    & $verifyScript -PackageRoot $inputRoot -Kind $kind -HasYaml:($kind -ne 'source')
    if (-not $?) { throw "SDK verification failed: $name" }
    $sdkProvenances[$name] = $provenance
    $sdkPayloadHashes[$name] = Get-PayloadHashes $inputRoot
    $sdkIdentity += [ordered]@{ name = $name; derived_package_id = $provenance.derived_package_id
        provenance_sha256 = (Get-FileHash -LiteralPath (Join-Path $inputRoot 'PROVENANCE.json')).Hash.ToLowerInvariant() }
}
# Use the Lab task's actual preparation provenance and deployed-file hashes, not a directory name as source proof.
$labProvenance = Get-Content -LiteralPath $LabProvenancePath -Raw -Encoding UTF8 | ConvertFrom-Json
if ($labProvenance.repository_head -cne $ExpectedSourceHead -or @($labProvenance.repository_status).Count -ne 0 -or
    $labProvenance.package_kind -cne 'static' -or $labProvenance.sdk_release_provenance.source_head -cne $ExpectedSourceHead -or
    $labProvenance.sdk_release_provenance.source_worktree_dirty -or $labProvenance.sdk_release_provenance.configuration -cne 'Release') {
    throw 'Lab preparation provenance does not match fixed static Release source'
}
$labHashes = @{}
foreach ($line in Get-Content -LiteralPath $LabHashesPath -Encoding UTF8) {
    if ($line.StartsWith('#') -or -not $line.Trim()) { continue }
    if ($line -cnotmatch '^([0-9a-fA-F]{64})  (.+)$') { throw 'Invalid Lab hash list' }
    $hash = $Matches[1].ToLowerInvariant(); $relative = $Matches[2]
    if ($relative -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or $labHashes.ContainsKey($relative)) { throw 'Unsafe or duplicate Lab hash path' }
    $labHashes[$relative] = $hash
}
$labFiles = @(Get-ChildItem -LiteralPath $labInput -Recurse -File)
if ($labFiles.Count -ne $labHashes.Count) { throw 'Lab file list is incomplete' }
$runtimeNames = @('pae_protocol_lab_ui.exe', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'platforms/qwindows.dll')
foreach ($file in $labFiles) {
    $relative = $file.FullName.Substring($labInput.Length + 1).Replace('\', '/')
    if ($relative -notin $runtimeNames -and $relative -notmatch '^configs/[^/]+\.pae\.(json|yaml)$') { throw "Unexpected Lab runtime file: $relative" }
    if (-not $labHashes.ContainsKey($relative) -or $labHashes[$relative] -cne (Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant()) { throw "Lab hash mismatch: $relative" }
}
foreach ($relative in $runtimeNames) { if (-not $labHashes.ContainsKey($relative)) { throw "Missing Lab runtime: $relative" } }
$requiredConfigs = @('configs/synthetic_binary_ui_stage1.pae.json', 'configs/synthetic_ascii_literal_only.pae.json', 'configs/synthetic_ascii_literal_only.pae.yaml')
foreach ($relative in $requiredConfigs) { if (-not $labHashes.ContainsKey($relative)) { throw "Missing first-use Lab config: $relative" } }
Assert-LabBuildIdentity $LabBuildIdentityPath $LabProvenancePath $LabHashesPath $labInput $fixedSource $ExpectedSourceHead $sdkProvenances
$rootDocs = @('README.md', '01-Lab体验.md', '02-SDK接入.md', '03-配置与边界.md')
$tutorialFiles = @('README.md', '01-配置结构与二进制.md', '02-文本与流式.md', '03-构建运行教学程序.md',
    '04-常用配置字段速查.md', '05-从协议表到SDK练习.md', '01-最小二进制.pae.json', '02-最小文本.pae.json', 'CMakeLists.txt', 'verify.cpp')
$projected = @(
    @('sdk-docs/schema/README.md', 'schema/README.md'),
    @('sdk-docs/schema/pae.schema.json', 'schema/pae.schema.json'),
    @('sdk-docs/schema/strict_json_profile_v0.1.md', 'schema/strict_json_profile_v0.1.md'),
    @('sdk-docs/schema/pae_yaml_profile_v0.1.md', 'schema/pae_yaml_profile_v0.1.md'),
    @('sdk-docs/schema/protocol_plan_execution_semantics_v0.1.md', 'schema/protocol_plan_execution_semantics_v0.1.md'),
    @('sdk-docs/docs/engineering/json_loader_diagnostic_contract_v0.1.md', 'docs/engineering/json_loader_diagnostic_contract_v0.1.md')
)
foreach ($relative in $rootDocs + @($tutorialFiles | ForEach-Object { "tutorials/$_" }) + @($projected | ForEach-Object { $_[0] })) {
    if (-not (Test-Path -LiteralPath (Join-Path $docs $relative) -PathType Leaf)) { throw "Missing document input: $relative" }
}
foreach ($relative in @('tutorials/01-最小二进制.pae.json', 'tutorials/02-最小文本.pae.json', 'tutorials/CMakeLists.txt', 'tutorials/verify.cpp')) {
    if ((Get-FileHash -LiteralPath (Join-Path $docs $relative)).Hash -cne
        (Get-FileHash -LiteralPath (Join-Path $fixedSource "docs/experience/$relative")).Hash) { throw "Teaching code differs from fixed product: $relative" }
}
if ((Get-FileHash -LiteralPath (Join-Path $docs 'sdk-docs/schema/pae.schema.json')).Hash -cne
    (Get-FileHash -LiteralPath (Join-Path $fixedSource 'schema/pae.schema.json')).Hash) { throw 'Schema JSON differs from fixed product' }
$noticeSource = Join-Path $fixedSource 'third_party/qt-package.md'
if (-not (Test-Path -LiteralPath $noticeSource -PathType Leaf)) { throw 'Missing fixed Qt notice' }
$docIdentity = Get-Content -LiteralPath (Join-Path $docs 'DOCUMENTATION-IDENTITY.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($docIdentity.product_source_head -cne $ExpectedSourceHead -or $docIdentity.version -cne $DocumentationIdentity) { throw 'Unexpected bundle documentation identity' }
foreach ($entry in $docIdentity.files) {
    if ($entry.path -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or
        (Get-FileHash -LiteralPath (Join-Path $docs $entry.path)).Hash.ToLowerInvariant() -cne $entry.sha256) { throw "Bundle document hash mismatch: $($entry.path)" }
}
foreach ($relative in $rootDocs + @($tutorialFiles | ForEach-Object { "tutorials/$_" }) + @($projected | ForEach-Object { $_[0] })) {
    if (@($docIdentity.files | Where-Object { $_.path -ceq $relative }).Count -ne 1) { throw "Bundle document identity missing: $relative" }
}
New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null
foreach ($name in $packages) {
    $target = Join-Path $bundleRoot "sdk/$name"
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Get-ChildItem -LiteralPath (Join-Path $sdkInput $SdkPackageDirectories[$name]) -Force | Copy-Item -Destination $target -Recurse
}
$labTarget = Join-Path $bundleRoot 'lab'
New-Item -ItemType Directory -Path $labTarget | Out-Null
Get-ChildItem -LiteralPath $labInput -Force | Copy-Item -Destination $labTarget -Recurse
$documentationEntries = @()
$copyPairs = @($rootDocs | ForEach-Object { ,@($_, $_) }) +
    @($tutorialFiles | ForEach-Object { ,@("tutorials/$_", "tutorials/$_") }) + $projected
foreach ($pair in $copyPairs) {
    $target = Join-Path $bundleRoot $pair[1]
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $docs $pair[0]) -Destination $target
    $documentationEntries += [ordered]@{ path = $pair[1]; sha256 = (Get-FileHash -LiteralPath $target).Hash.ToLowerInvariant() }
}
$noticeTarget = Join-Path $bundleRoot 'notices/qt-package.md'
New-Item -ItemType Directory -Path (Split-Path $noticeTarget -Parent) -Force | Out-Null
Copy-Item -LiteralPath $noticeSource -Destination $noticeTarget
$identity = [ordered]@{
    bundle_kind = 'local-windows-x64-experience-candidate'
    product_source_head = $ExpectedSourceHead
    sdk_documentation_overlay = $SdkOverlayIdentity
    sdk_packages = $sdkIdentity
    documentation_overlay = [ordered]@{ version = $DocumentationIdentity; files = $documentationEntries }
    lab_kind = 'standalone-static-Release'
    lab_preparation_provenance_sha256 = (Get-FileHash -LiteralPath $LabProvenancePath).Hash.ToLowerInvariant()
    lab_deployment_hashes_sha256 = (Get-FileHash -LiteralPath $LabHashesPath).Hash.ToLowerInvariant()
    lab_actual_build_identity_sha256 = (Get-FileHash -LiteralPath $LabBuildIdentityPath).Hash.ToLowerInvariant()
    qt_snapshot_version = '5.13.0'
    qt_redistribution_review = 'not_closed'
}
[IO.File]::WriteAllText((Join-Path $bundleRoot 'BUNDLE-IDENTITY.json'), ($identity | ConvertTo-Json -Depth 12), $encoding)
# Scan all package Markdown (inline links and reference definitions); local paths cannot escape the bundle.
foreach ($file in Get-ChildItem -LiteralPath $bundleRoot -Recurse -File -Filter '*.md') {
    $body = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    $links = @([regex]::Matches($body, '\[[^\]]+\]\(([^)]+)\)') | ForEach-Object { $_.Groups[1].Value }) +
        @([regex]::Matches($body, '(?m)^\[[^\]]+\]:\s*(\S+)') | ForEach-Object { $_.Groups[1].Value })
    foreach ($link in $links) {
        $link = $link.Trim('<', '>', '"', ' ')
        if ($link -match '^(https?://|mailto:|#)' -or -not $link) { continue }
        $relative = [uri]::UnescapeDataString(($link -split '[#?]', 2)[0])
        $path = [IO.Path]::GetFullPath((Join-Path $file.DirectoryName $relative))
        if (-not $path.StartsWith($bundleRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $path)) { throw "Missing or external local document link: $link in $($file.Name)" }
    }
}
$manifest = @(); $sha = @()
$payloadFiles = @(Get-ChildItem -LiteralPath $bundleRoot -Recurse -File | Sort-Object FullName)
foreach ($file in $payloadFiles) {
    $relative = $file.FullName.Substring($bundleRoot.Length + 1).Replace('\', '/')
    $manifest += "$($file.Length)  $relative"
    $sha += "$((Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant())  $relative"
}
[IO.File]::WriteAllLines((Join-Path $bundleRoot 'MANIFEST.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $manifest, $encoding)
[IO.File]::WriteAllLines((Join-Path $bundleRoot 'SHA256SUMS.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $sha, $encoding)
foreach ($name in $packages) {
    $target = Join-Path $bundleRoot "sdk/$name"
    $kind = if ($name -eq 'pae-sdk-source') { 'source' } elseif ($name -like 'pae-sdk-static-*') { 'static' } else { 'shared' }
    & $verifyScript -PackageRoot $target -Kind $kind -HasYaml:($kind -ne 'source')
    if (-not $?) { throw "Copied SDK verification failed: $name" }
    Assert-CopiedHashes $target $sdkPayloadHashes[$name]
}
Assert-CopiedHashes $labTarget $labHashes
Compress-Archive -LiteralPath $bundleRoot -DestinationPath $zipPath -CompressionLevel Optimal
$zipHash = (Get-FileHash -LiteralPath $zipPath).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $deliveryRoot 'ZIP.sha256'), "$zipHash  $BundleName.zip" + [Environment]::NewLine, $encoding)
Write-Output "BUNDLE_ROOT=$bundleRoot"
Write-Output "PAYLOAD_FILES=$($payloadFiles.Count)"
Write-Output "ZIP_SHA256=$zipHash"
