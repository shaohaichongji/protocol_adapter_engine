[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourcePackageRoot,
    [Parameter(Mandatory = $true)][string]$OverlayRoot,
    [Parameter(Mandatory = $true)][string]$DestinationRoot,
    [Parameter(Mandatory = $true)][string]$EvidenceRoot,
    [Parameter(Mandatory = $true)][ValidateSet("source", "static", "shared")][string]$Kind,
    [Parameter(Mandatory = $true)][bool]$HasYaml,
    [Parameter(Mandatory = $true)][ValidatePattern('^[0-9a-f]{40}$')][string]$ExpectedSourceHead,
    [Parameter(Mandatory = $true)][ValidatePattern('^[A-Za-z0-9._/-]+$')][string]$OverlayIdentity
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [System.IO.Path]::TrimEndingDirectorySeparator([System.IO.Path]::GetFullPath($SourcePackageRoot))
$overlayRootPath = [System.IO.Path]::TrimEndingDirectorySeparator([System.IO.Path]::GetFullPath($OverlayRoot))
$destination = [System.IO.Path]::TrimEndingDirectorySeparator([System.IO.Path]::GetFullPath($DestinationRoot))
$evidence = [System.IO.Path]::TrimEndingDirectorySeparator([System.IO.Path]::GetFullPath($EvidenceRoot))
$verifyScript = Join-Path $PSScriptRoot "verify_yaml_sdk_package.ps1"

$projectionIdentity = Get-Content -LiteralPath (Join-Path $overlayRootPath 'PROJECTION-IDENTITY.json') -Raw -Encoding UTF8 | ConvertFrom-Json
if ($projectionIdentity.product_source_head -cne $ExpectedSourceHead -or $projectionIdentity.version -cne $OverlayIdentity) {
    throw 'Documentation projection identity does not match the requested product'
}
foreach ($entry in $projectionIdentity.files) {
    if ($entry.path -match '(^/|^[A-Za-z]:|\\|(^|/)\.\.(/|$))' -or
        (Get-FileHash -LiteralPath (Join-Path $overlayRootPath $entry.path)).Hash.ToLowerInvariant() -cne $entry.sha256) {
        throw "Documentation projection hash mismatch: $($entry.path)"
    }
}

if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container) -or
    -not (Test-Path -LiteralPath $overlayRootPath -PathType Container)) {
    throw "Source package or documentation overlay is missing"
}
if (Test-Path -LiteralPath $destination) {
    throw "Derived package destination already exists: $destination"
}
if (Test-Path -LiteralPath $evidence) {
    throw "Original metadata evidence destination already exists: $evidence"
}
$sourcePrefix = $sourceRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
$overlayPrefix = $overlayRootPath.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($destination.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $destination.StartsWith($overlayPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Derived package destination must be outside source and overlay roots"
}
$destinationPrefix = $destination.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
if ($evidence.StartsWith($sourcePrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $evidence.StartsWith($overlayPrefix, [System.StringComparison]::OrdinalIgnoreCase) -or
    $evidence.StartsWith($destinationPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Original metadata evidence must be outside source, overlay and derived package roots"
}

& $verifyScript -PackageRoot $sourceRoot -Kind $Kind -HasYaml:$HasYaml
if (-not $?) { throw "Original SDK package verification failed" }

$schemaBase = if ($Kind -eq "source") { "schema" } else { "share/pae/schema" }
$diagnosticDestination = if ($Kind -eq "source") {
    "docs/engineering/json_loader_diagnostic_contract_v0.1.md"
} else {
    "share/pae/docs/engineering/json_loader_diagnostic_contract_v0.1.md"
}
$mapping = @(
    @{ source = "docs/sdk/README.md"; destination = "docs/sdk/README.md" },
    @{ source = $(if ($Kind -eq 'source') { 'examples/getting_started/README.md' } else { 'examples/getting_started/README.binary.md' }); destination = 'examples/getting_started/README.md' },
    @{ source = "schema/README.md"; destination = "$schemaBase/README.md" },
    @{ source = "schema/strict_json_profile_v0.1.md"; destination = "$schemaBase/strict_json_profile_v0.1.md" },
    @{ source = "schema/protocol_plan_execution_semantics_v0.1.md"; destination = "$schemaBase/protocol_plan_execution_semantics_v0.1.md" },
    @{ source = "schema/pae_yaml_profile_v0.1.md"; destination = "$schemaBase/pae_yaml_profile_v0.1.md" },
    @{ source = "schema/pae.schema.json"; destination = "$schemaBase/pae.schema.json" },
    @{ source = "docs/engineering/json_loader_diagnostic_contract_v0.1.md"; destination = $diagnosticDestination }
)
if ($Kind -eq "source") {
    foreach ($relative in @(
        "examples/public_api_codec/README.md",
        "examples/public_api_framer/README.md",
        "examples/public_api_host/README.md",
        "third_party/README.md"
    )) {
        $mapping += @{ source = $relative; destination = $relative }
    }
}

$baseMetadataNames = @("PROVENANCE.json", "MANIFEST.txt", "SHA256SUMS.txt")
foreach ($name in $baseMetadataNames) {
    if (-not (Test-Path -LiteralPath (Join-Path $sourceRoot $name) -PathType Leaf)) {
        throw "Original package metadata is missing: $name"
    }
}
$overlayEntries = @()
foreach ($item in $mapping) {
    $overlayFile = Join-Path $overlayRootPath $item.source
    $oldFile = Join-Path $sourceRoot $item.destination
    if (-not (Test-Path -LiteralPath $overlayFile -PathType Leaf)) {
        throw "Required documentation projection is missing: $($item.source)"
    }
    if (@($projectionIdentity.files | Where-Object { $_.path -ceq $item.source }).Count -ne 1) {
        throw "Projection identity does not list a unique required input: $($item.source)"
    }
    $overlayHash = (Get-FileHash -LiteralPath $overlayFile -Algorithm SHA256).Hash.ToLowerInvariant()
    $oldHash = if (Test-Path -LiteralPath $oldFile -PathType Leaf) {
        (Get-FileHash -LiteralPath $oldFile -Algorithm SHA256).Hash.ToLowerInvariant()
    } else { $null }
    if ($item.source -eq "schema/pae.schema.json" -and ($null -eq $oldHash -or $oldHash -cne $overlayHash)) {
        throw "Schema JSON projection must be byte-identical to the original package"
    }
    if ($item.source -ne "schema/pae.schema.json" -and
        [System.IO.Path]::GetExtension($item.source) -cne ".md") {
        throw "Only the Schema JSON and selected Markdown can be projected"
    }
    $overlayEntries += [ordered]@{
        source = $item.source
        destination = $item.destination
        source_sha256 = $overlayHash
        original_sha256 = $oldHash
    }
}

$originalProvenance = Get-Content -Raw -LiteralPath (Join-Path $sourceRoot "PROVENANCE.json") -Encoding UTF8 |
    ConvertFrom-Json
if ($originalProvenance.package_kind -cne $Kind -or
    [bool]$originalProvenance.yaml_frontend_installed -ne $HasYaml -or
    [bool]$originalProvenance.source_worktree_dirty -or
    $originalProvenance.source_head -cne $ExpectedSourceHead) {
    throw "Original package identity does not match the fixed clean source"
}

$baseManifestHash = (Get-FileHash -LiteralPath (Join-Path $sourceRoot "MANIFEST.txt") -Algorithm SHA256).Hash.ToLowerInvariant()
$baseHashListHash = (Get-FileHash -LiteralPath (Join-Path $sourceRoot "SHA256SUMS.txt") -Algorithm SHA256).Hash.ToLowerInvariant()
$baseProvenanceHash = (Get-FileHash -LiteralPath (Join-Path $sourceRoot "PROVENANCE.json") -Algorithm SHA256).Hash.ToLowerInvariant()
$identityText = @($ExpectedSourceHead, $baseManifestHash, $baseHashListHash, $OverlayIdentity) +
    @($overlayEntries | Sort-Object destination | ForEach-Object { "$($_.destination):$($_.source_sha256)" })
$identityBytes = [System.Text.Encoding]::UTF8.GetBytes(($identityText -join "`n"))
$derivedId = [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($identityBytes)).ToLowerInvariant()

New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $evidence) -Force | Out-Null
New-Item -ItemType Directory -Path $destination | Out-Null
New-Item -ItemType Directory -Path $evidence | Out-Null
Get-ChildItem -LiteralPath $sourceRoot -Force | Copy-Item -Destination $destination -Recurse -Force
foreach ($name in $baseMetadataNames) {
    Copy-Item -LiteralPath (Join-Path $sourceRoot $name) -Destination (Join-Path $evidence $name)
}
foreach ($item in $mapping) {
    $target = Join-Path $destination $item.destination
    $targetParent = Split-Path -Parent $target
    if (-not (Test-Path -LiteralPath $targetParent)) {
        New-Item -ItemType Directory -Path $targetParent -Force | Out-Null
    }
    Copy-Item -LiteralPath (Join-Path $overlayRootPath $item.source) -Destination $target -Force
}

$derivedProvenance = [ordered]@{
    schema_version = 2
    package_kind = $originalProvenance.package_kind
    configuration = $originalProvenance.configuration
    architecture = $originalProvenance.architecture
    toolchain = $originalProvenance.toolchain
    msvc_runtime = $originalProvenance.msvc_runtime
    project_version = $originalProvenance.project_version
    source_head = $originalProvenance.source_head
    source_worktree_dirty = $originalProvenance.source_worktree_dirty
    source_snapshot_note = "Product build came from the fixed clean source. This derived SDK adds documentation projections; it is not byte-identical to the source revision or original package."
    yaml_frontend_source_optional = $originalProvenance.yaml_frontend_source_optional
    yaml_frontend_installed = $originalProvenance.yaml_frontend_installed
    yaml_frontend_linkage = $originalProvenance.yaml_frontend_linkage
    derived_package = $true
    derived_package_id = $derivedId
    base_package_manifest_sha256 = $baseManifestHash
    base_package_sha256s_sha256 = $baseHashListHash
    base_package_provenance_sha256 = $baseProvenanceHash
    documentation_overlay = [ordered]@{
        version = $OverlayIdentity
        product_source_head = $ExpectedSourceHead
        files = $overlayEntries
    }
    generated_utc = [DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")
}
$derivedProvenance | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $destination "PROVENANCE.json") -Encoding UTF8

$manifestPath = Join-Path $destination "MANIFEST.txt"
$hashPath = Join-Path $destination "SHA256SUMS.txt"
$packageFiles = @(Get-ChildItem -LiteralPath $destination -Recurse -File)
$manifestEntries = @($packageFiles | Where-Object { $_.FullName -ne $manifestPath -and $_.FullName -ne $hashPath } |
    ForEach-Object { "$($_.Length)  $($_.FullName.Substring($destination.Length + 1).Replace('\', '/'))" } |
    Sort-Object)
$manifestEntries | Set-Content -LiteralPath $manifestPath -Encoding UTF8
$hashEntries = @(Get-ChildItem -LiteralPath $destination -Recurse -File |
    Where-Object { $_.FullName -ne $hashPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relative = $_.FullName.Substring($destination.Length + 1).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    })
$hashEntries | Set-Content -LiteralPath $hashPath -Encoding ascii

# Source files outside the exact documentation whitelist and metadata must remain byte-identical.
$allowed = @{}
foreach ($item in $mapping) { $allowed[$item.destination] = $true }
foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -Recurse -File) {
    $relative = $file.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
    if ($relative -in $baseMetadataNames -or $allowed.ContainsKey($relative)) { continue }
    $derivedFile = Join-Path $destination $relative
    if (-not (Test-Path -LiteralPath $derivedFile -PathType Leaf) -or
        (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -cne
        (Get-FileHash -LiteralPath $derivedFile -Algorithm SHA256).Hash) {
        throw "Non-document package byte changed: $relative"
    }
}

foreach ($file in Get-ChildItem -LiteralPath $destination -Recurse -File -Filter '*.md') {
    $body = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    $links = @([regex]::Matches($body, '\[[^\]]+\]\(([^)]+)\)') | ForEach-Object { $_.Groups[1].Value }) +
        @([regex]::Matches($body, '(?m)^\[[^\]]+\]:\s*(\S+)') | ForEach-Object { $_.Groups[1].Value })
    foreach ($link in $links) {
        $link = $link.Trim('<', '>', '"', ' ')
        if ($link -match '^(https?://|mailto:|#)' -or -not $link) { continue }
        $relative = [uri]::UnescapeDataString(($link -split '[#?]', 2)[0])
        $path = [IO.Path]::GetFullPath((Join-Path $file.DirectoryName $relative))
        if (-not $path.StartsWith($destination + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
            -not (Test-Path -LiteralPath $path)) { throw "Missing or external local document link: $link in $($file.Name)" }
    }
}
& $verifyScript -PackageRoot $destination -Kind $Kind -HasYaml:$HasYaml
if (-not $?) { throw "Derived SDK package verification failed" }
Write-Output "PAE_EXPERIENCE_SDK_DOCS_READY kind=$Kind configuration=$($originalProvenance.configuration) derived_id=$derivedId files=$($hashEntries.Count + 1) root=$destination"
