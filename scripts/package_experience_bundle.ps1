[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$productHead = 'adeae30d942ba42cc518c244c220730b7e462d2c'
$sdkInput = Join-Path $repo 'out/build/experience-sdk-docs-adeae30-20260927'
$labInput = Join-Path $repo 'out/build/experience-lab-adeae30-20260927/standalone-static-Release/deploy/Release'
$deliveryRoot = Join-Path $repo 'deliverables/sdk/adeae30-experience'
$bundleName = 'PAE-Lab-Windows-x64-adeae30'
$bundleRoot = Join-Path $deliveryRoot $bundleName
$zipPath = Join-Path $deliveryRoot "$bundleName.zip"
$encoding = [System.Text.UTF8Encoding]::new($false)

if ((git -C $repo rev-parse HEAD).Trim() -ne $productHead) { throw 'Unexpected product HEAD' }
if (Test-Path -LiteralPath $deliveryRoot) { throw "Delivery target already exists: $deliveryRoot" }
if (-not (Test-Path -LiteralPath $labInput -PathType Container)) { throw "Missing Lab input: $labInput" }

$packages = [ordered]@{
    'pae-sdk-source' = 'source package with spaces'
    'pae-sdk-static-debug' = 'static-yaml-Debug-install'
    'pae-sdk-static-release' = 'static-yaml-Release-install'
    'pae-sdk-shared-debug' = 'shared-yaml-Debug-install'
    'pae-sdk-shared-release' = 'shared-yaml-Release-install'
}
foreach ($name in $packages.Keys) {
    $inputRoot = Join-Path $sdkInput $packages[$name]
    if (-not (Test-Path -LiteralPath $inputRoot -PathType Container)) { throw "Missing SDK input: $inputRoot" }
    $provenance = Get-Content -LiteralPath (Join-Path $inputRoot 'PROVENANCE.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if ($provenance.source_head -cne $productHead -or
        $provenance.documentation_overlay.version -cne 'experience-sdk-docs/1' -or
        -not $provenance.derived_package) {
        throw "Unexpected SDK identity: $inputRoot"
    }
}

New-Item -ItemType Directory -Path $bundleRoot -Force | Out-Null
foreach ($name in $packages.Keys) {
    $target = Join-Path $bundleRoot "sdk/$name"
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path (Join-Path (Join-Path $sdkInput $packages[$name]) '*') -Destination $target -Recurse -Force
}
$labTarget = Join-Path $bundleRoot 'lab'
New-Item -ItemType Directory -Path $labTarget -Force | Out-Null
Copy-Item -Path (Join-Path $labInput '*') -Destination $labTarget -Recurse -Force

$rootDocs = @('README.md', '01-Lab体验.md', '02-SDK接入.md', '03-配置与边界.md')
foreach ($name in $rootDocs) {
    Copy-Item -LiteralPath (Join-Path $repo "docs/experience/$name") -Destination (Join-Path $bundleRoot $name)
}
$projected = @(
    @('sdk-docs/schema/pae.schema.json', 'schema/pae.schema.json'),
    @('sdk-docs/schema/strict_json_profile_v0.1.md', 'schema/strict_json_profile_v0.1.md'),
    @('sdk-docs/schema/pae_yaml_profile_v0.1.md', 'schema/pae_yaml_profile_v0.1.md'),
    @('sdk-docs/docs/engineering/json_loader_diagnostic_contract_v0.1.md', 'docs/engineering/json_loader_diagnostic_contract_v0.1.md')
)
foreach ($pair in $projected) {
    $target = Join-Path $bundleRoot $pair[1]
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo "docs/experience/$($pair[0])") -Destination $target
}
$noticeTarget = Join-Path $bundleRoot 'notices/qt-package.md'
New-Item -ItemType Directory -Path (Split-Path $noticeTarget -Parent) -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'third_party/qt-package.md') -Destination $noticeTarget

$identity = [ordered]@{
    bundle_kind = 'local-windows-x64-experience-candidate'
    product_source_head = $productHead
    sdk_documentation_overlay = 'experience-sdk-docs/1'
    lab_kind = 'standalone-static-Release'
    qt_snapshot_version = '5.13.0'
    qt_redistribution_review = 'not_closed'
    top_level_schema_source = 'docs/experience/sdk-docs projection'
    sdk_packages = @($packages.Keys)
}
[System.IO.File]::WriteAllText((Join-Path $bundleRoot 'BUNDLE-IDENTITY.json'), ($identity | ConvertTo-Json -Depth 8) + "`n", $encoding)

$manifest = [System.Collections.Generic.List[string]]::new()
$sha = [System.Collections.Generic.List[string]]::new()
$payloadFiles = @(Get-ChildItem -LiteralPath $bundleRoot -Recurse -File | Sort-Object FullName)
foreach ($file in $payloadFiles) {
    $relative = $file.FullName.Substring($bundleRoot.Length + 1).Replace('\', '/')
    if ($relative -in @('MANIFEST.txt', 'SHA256SUMS.txt')) { throw 'Unexpected self-referential input file' }
    $manifest.Add("$($file.Length)  $relative")
    $sha.Add("$((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant())  $relative")
}
[System.IO.File]::WriteAllLines((Join-Path $bundleRoot 'MANIFEST.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $manifest, $encoding)
[System.IO.File]::WriteAllLines((Join-Path $bundleRoot 'SHA256SUMS.txt'), @('# Excludes MANIFEST.txt and SHA256SUMS.txt to avoid self-reference') + $sha, $encoding)

Compress-Archive -LiteralPath $bundleRoot -DestinationPath $zipPath -CompressionLevel Optimal
$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText((Join-Path $deliveryRoot 'ZIP.sha256'), "$zipHash  $bundleName.zip`n", $encoding)
Write-Output "BUNDLE_ROOT=$bundleRoot"
Write-Output "PAYLOAD_FILES=$($payloadFiles.Count)"
Write-Output "ZIP_SHA256=$zipHash"
