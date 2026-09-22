[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("source", "static", "shared")]
    [string]$Kind,

    [Parameter(Mandatory = $true)]
    [string]$PackageRoot,

    [Parameter(Mandatory = $true)]
    [string]$Configuration,

    [string]$SourceRoot = (Split-Path -Parent $PSScriptRoot),

    [string]$Toolchain = "MSVC x64"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRootPath = [System.IO.Path]::GetFullPath($SourceRoot)
$packageRootPath = [System.IO.Path]::GetFullPath($PackageRoot)

function Copy-RelativeFile {
    param([Parameter(Mandatory = $true)][string]$RelativePath)

    $sourcePath = Join-Path $sourceRootPath $RelativePath
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Required SDK source file is missing: $RelativePath"
    }
    $destinationPath = Join-Path $packageRootPath $RelativePath
    $destinationDirectory = Split-Path -Parent $destinationPath
    if (-not (Test-Path -LiteralPath $destinationDirectory)) {
        New-Item -ItemType Directory -Path $destinationDirectory | Out-Null
    }
    Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
}

if ($Kind -eq "source") {
    if (Test-Path -LiteralPath $packageRootPath) {
        throw "Source package destination already exists: $packageRootPath"
    }
    New-Item -ItemType Directory -Path $packageRootPath | Out-Null

    $sourceFiles = @(
        "CMakeLists.txt",
        "cmake/PAEConfig.cmake.in",
        "cmake/PaeSdkInstall.cmake",
        "cmake/PaeYyjson.cmake",
        "cmake/VerifyYyjsonVendor.cmake",
        "docs/sdk/README.md",
        "docs/engineering/pae-sdk-stage3-contract.md",
        "schema/README.md",
        "schema/pae.schema.json",
        "schema/protocol_plan_execution_semantics_v0.1.md",
        "schema/strict_json_profile_v0.1.md",
        "src/public_api/CMakeLists.txt",
        "src/public_api/codec.cpp",
        "src/public_api/compiled_state_internal.h",
        "src/public_api/compiler.cpp",
        "src/public_api/host_endpoint.cpp",
        "src/public_api/stream_framer.cpp",
        "src/config_compiler/CMakeLists.txt",
        "src/config_compiler/config_compiler.cpp",
        "src/config_compiler/config_compiler.h",
        "src/config_compiler/plan_snapshot.cpp",
        "src/config_compiler/schema_ir.h",
        "src/config_compiler/protocol_metadata.cpp",
        "src/config_compiler/protocol_metadata.h",
        "src/config_compiler/protocol_metadata_internal.h",
        "src/config_compiler/validation_pipeline_internal.h",
        "src/protocol_plan/CMakeLists.txt",
        "src/protocol_plan/decimal_conversion_internal.cpp",
        "src/protocol_plan/decimal_conversion_internal.h",
        "src/protocol_plan/frozen_storage.h",
        "src/protocol_plan/plan_builder.cpp",
        "src/protocol_plan/plan_builder.h",
        "src/protocol_plan/plan_bundle.cpp",
        "src/protocol_plan/plan_bundle.h",
        "src/protocol_plan/plan_draft_internal.h",
        "src/protocol_plan/plan_memory.cpp",
        "src/protocol_plan/plan_memory.h",
        "src/protocol_plan/plan_types.h",
        "src/protocol_core/CMakeLists.txt",
        "src/protocol_core/complete_record_codec.cpp",
        "src/protocol_core/complete_record_codec.h",
        "src/protocol_core/decimal_conversion_internal.cpp",
        "src/protocol_core/decimal_conversion_internal.h",
        "src/protocol_framing/CMakeLists.txt",
        "src/protocol_framing/stream_framer.cpp",
        "src/protocol_framing/stream_framer.h",
        "third_party/yyjson/LICENSE",
        "third_party/yyjson/README.md",
        "third_party/yyjson/dependency.lock.json",
        "third_party/yyjson/src/yyjson.c",
        "third_party/yyjson/src/yyjson.h"
    )
    $sourceFiles += Get-ChildItem -LiteralPath (Join-Path $sourceRootPath "include/pae") -File |
        ForEach-Object { "include/pae/$($_.Name)" }
    foreach ($exampleDirectory in @(
        "examples/public_api_compile",
        "examples/public_api_codec",
        "examples/public_api_framer",
        "examples/public_api_host",
        "examples/public_api_sdk_consumer"
    )) {
        $sourceFiles += Get-ChildItem -LiteralPath (Join-Path $sourceRootPath $exampleDirectory) -File -Recurse |
            ForEach-Object {
                $_.FullName.Substring($sourceRootPath.Length + 1).Replace("\", "/")
            }
    }
    $sourceFiles += @(
        "examples/getting_started/CMakeLists.txt",
        "examples/getting_started/main.cpp",
        "examples/getting_started/README.md",
        "examples/config/synthetic_lab_exchange_slice.pae.json",
        "examples/config/synthetic_stream_framing_slice.pae.json",
        "examples/config/synthetic_ascii_text_slice.pae.json",
        "examples/config/synthetic_ascii_stream_slice.pae.json"
    )
    $sourceFiles | Sort-Object -Unique | ForEach-Object { Copy-RelativeFile $_ }
    $consumerDirectory = Join-Path $packageRootPath "examples/sdk_consumer"
    New-Item -ItemType Directory -Path $consumerDirectory | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRootPath "examples/public_api_sdk_consumer/CMakeLists.txt") `
        -Destination $consumerDirectory
    Copy-Item -LiteralPath (Join-Path $sourceRootPath "examples/public_api_sdk_consumer/main.cpp") `
        -Destination $consumerDirectory
    New-Item -ItemType Directory -Path (Join-Path $packageRootPath "LICENSES") | Out-Null
    Copy-Item -LiteralPath (Join-Path $sourceRootPath "third_party/yyjson/LICENSE") `
        -Destination (Join-Path $packageRootPath "LICENSES/yyjson-LICENSE.txt")
} else {
    if ($Configuration -notin @("Debug", "Release")) {
        throw "Binary SDK packages require Configuration Debug or Release"
    }
    if (-not (Test-Path -LiteralPath $packageRootPath -PathType Container)) {
        throw "Binary install tree does not exist: $packageRootPath"
    }
    foreach ($requiredPath in @(
        "include/pae/compiler.h",
        "include/pae/codec.h",
        "include/pae/stream_framer.h",
        "include/pae/host_endpoint.h",
        "lib/cmake/PAE/PAEConfig.cmake",
        "lib/cmake/PAE/PAEConfigVersion.cmake",
        "docs/sdk/README.md",
        "examples/getting_started/CMakeLists.txt",
        "examples/getting_started/main.cpp",
        "examples/getting_started/README.md",
        "examples/config/synthetic_stream_framing_slice.pae.json",
        "examples/sdk_consumer/CMakeLists.txt",
        "examples/sdk_consumer/main.cpp",
        "LICENSES/yyjson-LICENSE.txt"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $packageRootPath $requiredPath) -PathType Leaf)) {
            throw "Binary package is incomplete: $requiredPath"
        }
    }
    $packageConfig = Get-Content -Raw -LiteralPath (Join-Path $packageRootPath "lib/cmake/PAE/PAEConfig.cmake")
    $expectedConfigLine = 'set(PAE_PACKAGE_CONFIGURATION "' + $Configuration + '")'
    if (-not $packageConfig.Contains($expectedConfigLine)) {
        throw "Binary package configuration does not match requested Configuration $Configuration"
    }
}

foreach ($metadataName in @("PAE-SDK-README.md", "PROVENANCE.json", "MANIFEST.txt", "SHA256SUMS.txt")) {
    if (Test-Path -LiteralPath (Join-Path $packageRootPath $metadataName)) {
        throw "Package metadata already exists: $metadataName"
    }
}

$head = (& git -C $sourceRootPath rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to read the source Git HEAD"
}
$statusLines = @(& git -C $sourceRootPath status --short --untracked-files=all)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to read the source Git status"
}

$readmeLines = @(
    "# PAE Windows x64 SDK Stage 3 local package",
    "",
    "- Package kind: $Kind",
    "- Configuration: $Configuration",
    "- Consumer target: PAE::pae",
    "- Language level: C++17",
    "- MSVC runtime: /MDd for Debug, /MD for Release",
    "- Status: local review artifact, not a formal release",
    "",
    "The package exposes the current experimental public Compiler/metadata, Codec,",
    "StreamFramer and Host APIs. Start with the Chinese docs/sdk/README.md and",
    "examples/getting_started. Keep examples/sdk_consumer as the comprehensive",
    "reproducible package check; the two entry points have different responsibilities.",
    "",
    "## Configure, build, and run",
    "",
    "Run these PowerShell commands from the package root. BuildRoot must name a fresh",
    "directory outside this package. The three pinned generator selectors are",
    "-G Visual Studio 18 2026, -A x64, and -T v142,version=14.29.30133.",
    "",
    '    $PackageRoot = (Resolve-Path .).Path',
    '    $BuildRoot = Join-Path (Split-Path -Parent $PackageRoot) "pae-sdk-consumer-build"'
)
if ($Kind -eq "source") {
    $readmeLines += @(
        '    cmake -S "$PackageRoot\examples\sdk_consumer" -B $BuildRoot -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" "-DPAE_SOURCE_DIR=$PackageRoot"',
        '    $BinaryConfig = "$PackageRoot\examples\config\synthetic_stream_framing_slice.pae.json"',
        '    $AsciiConfig = "$PackageRoot\examples\config\synthetic_ascii_stream_slice.pae.json"',
        '    foreach ($Configuration in @("Debug", "Release")) {',
        '        cmake --build $BuildRoot --config $Configuration --target pae_sdk_stage3_consumer',
        '        & "$BuildRoot\$Configuration\pae_sdk_stage3_consumer.exe" $BinaryConfig $AsciiConfig',
        '        if ($LASTEXITCODE -ne 0) { throw "PAE SDK consumer failed: $Configuration" }',
        '    }',
        "",
        "PAE_SOURCE_DIR must be this unpacked package root. The source package is",
        "self-contained and does not fall back to the development repository."
    )
} else {
    $readmeLines += @(
        '    cmake -S "$PackageRoot\examples\sdk_consumer" -B $BuildRoot -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" "-DCMAKE_PREFIX_PATH=$PackageRoot"',
        ('    $Configuration = "' + $Configuration + '"'),
        '    $BinaryConfig = "$PackageRoot\share\pae\examples\config\synthetic_stream_framing_slice.pae.json"',
        '    $AsciiConfig = "$PackageRoot\share\pae\examples\config\synthetic_ascii_stream_slice.pae.json"',
        '    cmake --build $BuildRoot --config $Configuration --target pae_sdk_stage3_consumer',
        '    & "$BuildRoot\$Configuration\pae_sdk_stage3_consumer.exe" $BinaryConfig $AsciiConfig',
        '    if ($LASTEXITCODE -ne 0) { throw "PAE SDK consumer failed: $Configuration" }',
        "",
        "CMAKE_PREFIX_PATH must be this package root. Do not use a Debug consumer with",
        "a Release package or a Release consumer with a Debug package."
    )
}
$readmeLines += @(
    "",
    "Shared packages copy pae.dll beside the consumer after build. Do not copy or",
    "redistribute Windows system CRT DLLs; use the matching installed MSVC runtime.",
    "This experimental C++ API requires an ABI-compatible MSVC toolset and does not",
    "promise a stable or cross-toolset ABI. Binary PAE::pae targets reject a mismatched",
    "Debug/Release build at link time with PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE.",
    "This executable gate covers only the package Debug/Release generation. The",
    "PAE_MSVC_RUNTIME_LIBRARY variable is informational and does not automatically",
    "validate an arbitrary consumer CRT or MSVC toolset.",
    "",
    "MANIFEST.txt lists package files and sizes. SHA256SUMS.txt records SHA-256 hashes,",
    "and PROVENANCE.json records the source revision and dirty-snapshot status.",
    "",
    "The repository currently has no project-level PAE license. LICENSES/yyjson-LICENSE.txt",
    "and vendored third_party/yyjson/LICENSE where present apply only to yyjson. Do not",
    "treat this local package as authorized for external redistribution."
)
$readme = $readmeLines -join "`n"
Set-Content -LiteralPath (Join-Path $packageRootPath "PAE-SDK-README.md") -Value $readme -Encoding utf8 -NoNewline

$provenance = [ordered]@{
    schema_version = 1
    package_kind = $Kind
    configuration = $Configuration
    architecture = "x64"
    toolchain = $Toolchain
    msvc_runtime = if ($Configuration -eq "Debug") { "/MDd" } elseif ($Configuration -eq "Release") { "/MD" } else { "/MDd and /MD" }
    project_version = "0.1.0"
    source_head = $head
    source_worktree_dirty = ($statusLines.Count -ne 0)
    source_snapshot_note = "Built from the named Git revision plus preserved unstaged and untracked working-tree changes."
    generated_utc = [DateTime]::UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ")
}
$provenance | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $packageRootPath "PROVENANCE.json") -Encoding utf8

$manifestPath = Join-Path $packageRootPath "MANIFEST.txt"
$hashPath = Join-Path $packageRootPath "SHA256SUMS.txt"
$manifestEntries = Get-ChildItem -LiteralPath $packageRootPath -File -Recurse |
    Where-Object { $_.FullName -ne $manifestPath -and $_.FullName -ne $hashPath } |
    ForEach-Object {
        $relativePath = $_.FullName.Substring($packageRootPath.Length + 1).Replace("\", "/")
        "$($_.Length)  $relativePath"
    } |
    Sort-Object
$manifestEntries | Set-Content -LiteralPath $manifestPath -Encoding utf8

$hashEntries = Get-ChildItem -LiteralPath $packageRootPath -File -Recurse |
    Where-Object { $_.FullName -ne $hashPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = $_.FullName.Substring($packageRootPath.Length + 1).Replace("\", "/")
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    }
$hashEntries | Set-Content -LiteralPath $hashPath -Encoding ascii

Write-Output "PAE_SDK_STAGE3_PACKAGE_READY kind=$Kind configuration=$Configuration root=$packageRootPath files=$($hashEntries.Count)"
