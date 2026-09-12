param(
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$SamplePath,
    [Parameter(Mandatory = $true)][string]$LiteralOnlySamplePath,
    [Parameter(Mandatory = $true)][string]$LegacyBinarySamplePath
)

$ErrorActionPreference = 'Stop'
$schema = Get-Content -LiteralPath $SchemaPath -Raw
$sample = Get-Content -LiteralPath $SamplePath -Raw
$literalOnlySample = Get-Content -LiteralPath $LiteralOnlySamplePath -Raw
$legacyBinarySample = Get-Content -LiteralPath $LegacyBinarySamplePath -Raw

if (-not ($sample | Test-Json -Schema $schema)) {
    throw 'Schema 0.10 public ASCII sample was rejected by pae.schema.json'
}

if (-not ($literalOnlySample | Test-Json -Schema $schema)) {
    throw 'Schema 0.10 public literal-only ASCII sample was rejected by pae.schema.json'
}

$legacyVersion = $sample -replace '"schema_version": "0.10"', '"schema_version": "0.9"'
if ($legacyVersion | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.9 unexpectedly accepted the ASCII layout'
}

$binaryObject = $sample | ConvertFrom-Json
$binaryObject.messages[0] | Add-Member -NotePropertyName frame_length_bytes -NotePropertyValue 16
$binaryMember = $binaryObject | ConvertTo-Json -Depth 32
$binaryMember | ConvertFrom-Json | Out-Null
if ($binaryMember | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.10 unexpectedly accepted a Binary Message member'
}

$secondLiteralSyntax = $sample -replace '"text": "RX "', '"ascii_bytes": "52 58 20"'
if ($secondLiteralSyntax | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.10 unexpectedly accepted a second literal syntax'
}

$legacyBinaryObject = $legacyBinarySample | ConvertFrom-Json
$legacyBinaryObject.messages[0].fields = @()
$legacyBinaryEmptyFields = $legacyBinaryObject | ConvertTo-Json -Depth 32
if ($legacyBinaryEmptyFields | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Legacy Binary Message unexpectedly accepted an empty fields array'
}

Write-Output 'Schema 0.10 ASCII contract checks passed.'
