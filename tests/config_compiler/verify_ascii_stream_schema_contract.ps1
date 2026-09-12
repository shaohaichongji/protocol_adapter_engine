param(
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$SamplePath,
    [Parameter(Mandatory = $true)][string]$V10SamplePath
)

$ErrorActionPreference = 'Stop'
$schema = Get-Content -LiteralPath $SchemaPath -Raw
$sample = Get-Content -LiteralPath $SamplePath -Raw
$v10Sample = Get-Content -LiteralPath $V10SamplePath -Raw

if (-not ($sample | Test-Json -Schema $schema)) {
    throw 'Schema 0.11 public ASCII stream sample was rejected by pae.schema.json'
}
if (-not ($v10Sample | Test-Json -Schema $schema)) {
    throw 'Schema 0.10 ASCII complete-record sample regressed'
}

$oldVersion = $sample -replace '"schema_version": "0.11"', '"schema_version": "0.10"'
if ($oldVersion | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.10 unexpectedly accepted ascii_crlf stream framing'
}

$wrongTerminator = $sample -replace '"terminator_text": "\\r\\n"', '"terminator_text": "\\n"'
if ($wrongTerminator | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.11 unexpectedly accepted a non-CRLF terminator'
}

$tooSmall = $sample -replace '"maximum_frame_length": 12', '"maximum_frame_length": 1'
if ($tooSmall | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.11 unexpectedly accepted maximum_frame_length below CRLF width'
}

$foreignMember = $sample -replace '"maximum_frame_length": 12',
    '"maximum_frame_length": 12, "sync_bytes": "0D 0A"'
if ($foreignMember | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.11 unexpectedly accepted a foreign framing strategy member'
}

$binaryStrategy = $sample -replace '"strategy": "ascii_crlf"', '"strategy": "fixed_length"'
if ($binaryStrategy | Test-Json -Schema $schema -ErrorAction SilentlyContinue) {
    throw 'Schema 0.11 unexpectedly accepted a Schema 0.9 Binary stream strategy'
}

Write-Output 'Schema 0.11 ASCII stream contract checks passed.'
