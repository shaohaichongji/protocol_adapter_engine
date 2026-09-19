param(
    [Parameter(Mandatory = $true)][string]$RepositoryRoot,
    [Parameter(Mandatory = $true)][string]$DestinationRoot,
    [Parameter(Mandatory = $true)][string]$SdkCandidateRoot,
    [ValidateSet('static', 'shared')][string]$PackageKind = 'static'
)

$ErrorActionPreference = 'Stop'
$repo = [System.IO.Path]::GetFullPath($RepositoryRoot)
$destination = [System.IO.Path]::GetFullPath($DestinationRoot)
$candidate = [System.IO.Path]::GetFullPath($SdkCandidateRoot)
if (Test-Path -LiteralPath $destination) {
    throw "DestinationRoot already exists; refusing to overwrite: $destination"
}
foreach ($required in @($repo, $candidate)) {
    if (-not (Test-Path -LiteralPath $required -PathType Container)) {
        throw "Required input root is missing: $required"
    }
}

New-Item -ItemType Directory -Path $destination | Out-Null
$labRoot = Join-Path $destination 'inputs\lab'
$sdkRoot = Join-Path $destination 'inputs\sdk'
$qtRoot = Join-Path $destination 'inputs\qt'
$dependencyRoot = Join-Path $destination 'inputs\yyjson'
$configRoot = Join-Path $labRoot 'standalone-configs'
foreach ($path in @($labRoot, $sdkRoot, $qtRoot, $dependencyRoot, $configRoot, (Join-Path $destination 'logs'))) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

function Copy-RelativeFile([string]$relative) {
    $source = Join-Path $repo $relative
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) { throw "Missing whitelist file: $source" }
    $target = Join-Path $labRoot $relative
    New-Item -ItemType Directory -Path ([System.IO.Path]::GetDirectoryName($target)) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $target
}

$labFiles = @(
    'tools\protocol_lab\sha256.cpp', 'tools\protocol_lab\sha256.h',
    'tools\protocol_lab_binary\public_binary_decode.cpp', 'tools\protocol_lab_binary\public_binary_decode.h',
    'tools\protocol_lab_ascii\public_ascii_offline_adapter.cpp', 'tools\protocol_lab_ascii\public_ascii_offline_adapter.h',
    'tools\protocol_lab_ascii\public_ascii_host_adapter.cpp', 'tools\protocol_lab_ascii\public_ascii_host_adapter.h',
    'tools\protocol_lab_ui\application_window.cpp', 'tools\protocol_lab_ui\application_window.h',
    'tools\protocol_lab_ui\ascii_escaped_input.cpp', 'tools\protocol_lab_ui\ascii_escaped_input.h',
    'tools\protocol_lab_ui\ascii_host_adapter.cpp', 'tools\protocol_lab_ui\ascii_host_adapter.h',
    'tools\protocol_lab_ui\ascii_host_types.cpp', 'tools\protocol_lab_ui\ascii_host_types.h',
    'tools\protocol_lab_ui\binary_host_adapter.h',
    'tools\protocol_lab_ui\binary_host_adapter_public.cpp', 'tools\protocol_lab_ui\binary_host_adapter_public.h',
    'tools\protocol_lab_ui\canonical_input.cpp', 'tools\protocol_lab_ui\canonical_input.h',
    'tools\protocol_lab_ui\compile_worker.cpp', 'tools\protocol_lab_ui\compile_worker.h',
    'tools\protocol_lab_ui\description_mapping.cpp', 'tools\protocol_lab_ui\description_mapping.h',
    'tools\protocol_lab_ui\document_session.cpp', 'tools\protocol_lab_ui\document_session.h',
    'tools\protocol_lab_ui\document_tab.cpp', 'tools\protocol_lab_ui\document_tab.h',
    'tools\protocol_lab_ui\exact_value_delegate.cpp', 'tools\protocol_lab_ui\exact_value_delegate.h',
    'tools\protocol_lab_ui\field_table_model.cpp', 'tools\protocol_lab_ui\field_table_model.h',
    'tools\protocol_lab_ui\hex_view.cpp', 'tools\protocol_lab_ui\hex_view.h',
    'tools\protocol_lab_ui\inspect_hex_input.cpp', 'tools\protocol_lab_ui\inspect_hex_input.h',
    'tools\protocol_lab_ui\lab_execution_observer.h', 'tools\protocol_lab_ui\main.cpp',
    'tools\protocol_lab_ui\owned_presentation_types.h',
    'tools\protocol_lab_ui\public_binary_description.cpp', 'tools\protocol_lab_ui\public_binary_description.h',
    'tools\protocol_lab_ui\public_legacy_complete_adapter.cpp', 'tools\protocol_lab_ui\public_legacy_complete_adapter.h',
    'tools\protocol_lab_ui\schema_dispatch.cpp', 'tools\protocol_lab_ui\schema_dispatch.h',
    'tools\protocol_lab_ui\smoke_editor_target.h', 'tools\protocol_lab_ui\ui_field_result.h',
    'tools\protocol_lab_ui\ui_physical_types.h',
    'tests\protocol_lab_ui\CMakeLists.txt', 'tests\protocol_lab_ui\test_support.h',
    'tests\protocol_lab_binary\CMakeLists.txt', 'tests\protocol_lab_binary\public_binary_decode_tests.cpp',
    'tests\protocol_lab_ascii\CMakeLists.txt', 'tests\protocol_lab_ascii\public_ascii_offline_adapter_tests.cpp',
    'tests\protocol_lab_ascii\public_ascii_stream_adapter_tests.cpp'
)
$labFiles += Get-ChildItem -LiteralPath (Join-Path $repo 'tools\protocol_lab_ui\standalone') -File -Recurse |
    ForEach-Object { $_.FullName.Substring($repo.Length + 1) }
$labFiles += Get-ChildItem -LiteralPath (Join-Path $repo 'tests\protocol_lab_ui') -File -Filter '*.cpp' |
    ForEach-Object { $_.FullName.Substring($repo.Length + 1) }
foreach ($relative in ($labFiles | Sort-Object -Unique)) { Copy-RelativeFile $relative }

$configs = @(
    'tests\protocol_lab_ui\fixtures\synthetic_ui_v05.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ui_v06.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ui_v07.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ui_v08.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ui_inspect_multi_v05.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_binary_ui_stage1.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ascii_decode_only.pae.json',
    'tests\protocol_lab_ui\fixtures\synthetic_ascii_encode_only.pae.json',
    'examples\config\synthetic_ascii_text_slice.pae.json',
    'examples\config\synthetic_ascii_literal_only.pae.json',
    'examples\config\synthetic_ascii_stream_slice.pae.json',
    'examples\config\synthetic_bounded_variable_record.pae.json',
    'examples\config\synthetic_int64_slice.pae.json',
    'examples\config\synthetic_crc_slice.pae.json'
)
foreach ($relative in $configs) {
    Copy-RelativeFile $relative
    Copy-Item -LiteralPath (Join-Path $repo $relative) -Destination (Join-Path $configRoot ([System.IO.Path]::GetFileName($relative)))
}
Copy-RelativeFile 'cmake\GenerateProtocolLabUiMaxFixture.cmake'
& cmake "-DPAE_OUTPUT_FILE=$(Join-Path $configRoot 'synthetic_ui_max.pae.json')" -P (Join-Path $repo 'cmake\GenerateProtocolLabUiMaxFixture.cmake')
if ($LASTEXITCODE -ne 0) { throw 'Failed to generate synthetic_ui_max.pae.json' }

foreach ($package in @("pae-sdk-$PackageKind-debug", "pae-sdk-$PackageKind-release")) {
    $source = Join-Path $candidate $package
    if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw "Missing SDK package: $source" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $sdkRoot $package) -Recurse
}

$qtSource = Join-Path $repo 'third_party\qt'
Copy-Item -LiteralPath (Join-Path $qtSource 'include') -Destination (Join-Path $qtRoot 'include') -Recurse
foreach ($configuration in @('debug', 'release')) {
    New-Item -ItemType Directory -Path (Join-Path $qtRoot "lib\$configuration") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $qtRoot "bin\$configuration\platforms") -Force | Out-Null
}
foreach ($module in @('Core', 'Gui', 'Widgets')) {
    Copy-Item -LiteralPath (Join-Path $qtSource "lib\debug\Qt5${module}d.lib") -Destination (Join-Path $qtRoot 'lib\debug')
    Copy-Item -LiteralPath (Join-Path $qtSource "lib\release\Qt5${module}.lib") -Destination (Join-Path $qtRoot 'lib\release')
    Copy-Item -LiteralPath (Join-Path $qtSource "bin\debug\Qt5${module}d.dll") -Destination (Join-Path $qtRoot 'bin\debug')
    Copy-Item -LiteralPath (Join-Path $qtSource "bin\release\Qt5${module}.dll") -Destination (Join-Path $qtRoot 'bin\release')
}
Copy-Item -LiteralPath (Join-Path $qtSource 'bin\debug\platforms\qwindowsd.dll') -Destination (Join-Path $qtRoot 'bin\debug\platforms')
Copy-Item -LiteralPath (Join-Path $qtSource 'bin\release\platforms\qwindows.dll') -Destination (Join-Path $qtRoot 'bin\release\platforms')

$yyjsonSource = Join-Path $repo 'third_party\yyjson'
New-Item -ItemType Directory -Path (Join-Path $dependencyRoot 'src') -Force | Out-Null
foreach ($file in @('src\yyjson.h', 'src\yyjson.c', 'LICENSE', 'dependency.lock.json')) {
    $target = if ($file.StartsWith('src')) { Join-Path $dependencyRoot $file } else { Join-Path $dependencyRoot ([System.IO.Path]::GetFileName($file)) }
    Copy-Item -LiteralPath (Join-Path $yyjsonSource $file) -Destination $target
}

$identity = [ordered]@{
    schema_version = 1
    repository_head = (git -C $repo rev-parse HEAD).Trim()
    repository_status = @(git -C $repo status --short)
    sdk_candidate_root = $candidate
    package_kind = $PackageKind
    sdk_debug_provenance = Get-Content -Raw -LiteralPath (Join-Path $sdkRoot "pae-sdk-$PackageKind-debug\PROVENANCE.json") | ConvertFrom-Json
    sdk_release_provenance = Get-Content -Raw -LiteralPath (Join-Path $sdkRoot "pae-sdk-$PackageKind-release\PROVENANCE.json") | ConvertFrom-Json
    note = "Local $PackageKind closure validation input; not a clean release or distribution package."
}
$identity | ConvertTo-Json -Depth 8 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $destination 'INPUT_PROVENANCE.json')
Get-ChildItem -LiteralPath (Join-Path $destination 'inputs') -File -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        [pscustomobject]@{
            path = $_.FullName.Substring($destination.Length + 1).Replace('\', '/')
            bytes = $_.Length
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $_.FullName).Hash
        }
    } | ConvertTo-Json -Depth 3 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $destination 'INPUT_SHA256.json')

Write-Output $destination
