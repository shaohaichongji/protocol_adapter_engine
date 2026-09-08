param(
    [Parameter(Mandatory = $true)]
    [string]$BinaryDirRelative
)

$ErrorActionPreference = 'Stop'
$sourceDir = (Get-Location).Path
$binaryDir = (Resolve-Path -LiteralPath $BinaryDirRelative).Path
$lab = Get-ChildItem -LiteralPath $binaryDir -Filter 'pae_protocol_lab.exe' -File -Recurse |
    Where-Object { $_.FullName -match '[\\/]tools[\\/]protocol_lab[\\/]' } |
    Select-Object -First 1
if ($null -eq $lab) {
    throw 'pae_protocol_lab.exe was not found in the C3 build directory'
}

$unicodeDirectory = (-join @([char]0x4E2D, [char]0x6587)) + ' ' +
    (-join @([char]0x8DEF, [char]0x5F84)) + ' ' +
    (-join @([char]0x542B, [char]0x7A7A, [char]0x683C))
$caseRoot = Join-Path $binaryDir (Join-Path 'tests\protocol_lab_c3_cli' $unicodeDirectory)
if (Test-Path -LiteralPath $caseRoot) {
    Remove-Item -LiteralPath $caseRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $caseRoot | Out-Null
$configName = (-join @([char]0x914D, [char]0x7F6E)) + ' ' +
    (-join @([char]0x6587, [char]0x4EF6)) + '.pae.json'
$valuesName = (-join @([char]0x52A8, [char]0x6001)) + ' ' + [char]0x503C +
    '.pae-lab.json'
$runsName = (-join @([char]0x8BB0, [char]0x5F55)) + ' ' + [char]0x6839
$config = Join-Path $caseRoot $configName
$values = Join-Path $caseRoot $valuesName
$runs = Join-Path $caseRoot $runsName
Copy-Item -LiteralPath (Join-Path $sourceDir 'tests\protocol_core\fixtures\decimal_core_contract.pae.json') -Destination $config
Copy-Item -LiteralPath (Join-Path $sourceDir 'tests\protocol_lab_c3_cli\fixtures\valid.values.pae-lab.json') -Destination $values

$utf8 = New-Object System.Text.UTF8Encoding($false, $true)
function Invoke-LabJson {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][int]$ExpectedExit,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )
    $stdout = Join-Path $caseRoot "$Name.stdout.json"
    $stderr = Join-Path $caseRoot "$Name.stderr.txt"
    $quoted = @($Arguments | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' })
    $process = Start-Process -FilePath $lab.FullName -ArgumentList $quoted -NoNewWindow -Wait -PassThru `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if ($process.ExitCode -ne $ExpectedExit) {
        throw "$Name expected exit $ExpectedExit but got $($process.ExitCode)"
    }
    $bytes = [System.IO.File]::ReadAllBytes($stdout)
    $text = $utf8.GetString($bytes)
    $document = $text | ConvertFrom-Json
    if ($document.format_version -ne 'pae.lab.cli/0.1' -or
        $document.process_exit_code -ne $ExpectedExit) {
        throw "$Name did not return a valid C3 JSON envelope"
    }
    [pscustomobject]@{ ExitCode = $process.ExitCode; Document = $document; Stdout = $stdout }
}

$encode = Invoke-LabJson -Name '01-encode' -ExpectedExit 0 -Arguments @(
    'encode', '--config', $config, '--values', $values, '--record-root', $runs, '--output', 'json'
)
$bundle = [string]$encode.Document.published_bundle
if (-not [System.IO.Path]::IsPathRooted($bundle) -or -not (Test-Path -LiteralPath $bundle)) {
    throw 'Unicode Encode published_bundle is not an existing absolute directory'
}
$expectedRoot = [System.IO.Path]::GetFullPath($runs).TrimEnd('\', '/')
$actualRoot = [System.IO.Path]::GetFullPath((Split-Path -Path $bundle -Parent)).TrimEnd('\', '/')
if ($expectedRoot -cne $actualRoot) {
    throw 'Unicode Encode published_bundle changed the record root'
}
$frame = Join-Path $bundle 'frames\000001_frame.bin'
if (-not (Test-Path -LiteralPath $frame)) {
    throw 'Unicode Encode Bundle does not contain its Frame'
}

$inspect = Invoke-LabJson -Name '02-inspect' -ExpectedExit 0 -Arguments @(
    'inspect', '--config', $config, '--frame-bin', $frame, '--record-root', $runs, '--output', 'json'
)
if ($inspect.Document.result.fields[0].decimal64.coefficient -ne 123 -or
    $inspect.Document.result.fields[0].decimal64.scale -ne 1) {
    throw 'Unicode Inspect Decimal output is incorrect'
}
$inspectBundle = [string]$inspect.Document.published_bundle
$replay = Invoke-LabJson -Name '03-replay' -ExpectedExit 0 -Arguments @(
    'replay', '--bundle', $inspectBundle, '--record-root', $runs, '--output', 'json'
)
if ($replay.Document.comparison.status -ne 'EQUAL') {
    throw 'Unicode Replay comparison is not EQUAL'
}
$replayBundle = [string]$replay.Document.published_bundle
$compare = Invoke-LabJson -Name '04-compare' -ExpectedExit 0 -Arguments @(
    'compare', '--left-run', $inspectBundle, '--right-run', $replayBundle, '--output', 'json'
)
if ($compare.Document.comparison.status -ne 'EQUAL') {
    throw 'Unicode Compare comparison is not EQUAL'
}

[ordered]@{
    status = 'PASS'
    path = $caseRoot
    encode_bundle = $bundle
    inspect_bundle = $inspectBundle
    replay_bundle = $replayBundle
    compare = $compare.Document.comparison.status
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $caseRoot 'unicode-chain-result.json') -Encoding UTF8
