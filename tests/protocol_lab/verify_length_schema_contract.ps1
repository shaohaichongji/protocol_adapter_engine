param(
    [Parameter(Mandatory = $true)][string]$LabExecutable,
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$ConfigPath,
    [Parameter(Mandatory = $true)][string]$ValuesPath,
    [Parameter(Mandatory = $true)][string]$TestRoot
)

$ErrorActionPreference = 'Stop'

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Test-Candidate([string]$Name, [string]$Text, [bool]$SchemaValid,
                        [int]$ExpectedExit, [string]$ExpectedPointer) {
    $path = Join-Path $TestRoot $Name
    [System.IO.File]::WriteAllText($path, $Text, [System.Text.UTF8Encoding]::new($false))
    $schemaResult = Test-Json -Json $Text -SchemaFile $SchemaPath -ErrorAction SilentlyContinue
    Assert-Condition ($schemaResult -eq $SchemaValid) "Schema mismatch for $Name"
    $runRoot = Join-Path $TestRoot "$Name-runs"
    $output = & $LabExecutable encode --config $path --values $ValuesPath `
        --record-root $runRoot --output json
    Assert-Condition ($LASTEXITCODE -eq $ExpectedExit) "Lab exit mismatch for $Name"
    $result = $output | Out-String | ConvertFrom-Json
    if ($ExpectedExit -eq 0) {
        Assert-Condition ($result.result.format_version -eq 'pae.lab.result/0.8') `
            'Valid Schema 0.7 did not select Result 0.8'
        Assert-Condition ($result.result.frame_hex -eq 'AA0006057E55') `
            'Valid length config did not produce the published frame'
    } else {
        Assert-Condition ($result.diagnostic.detail -match [regex]::Escape($ExpectedPointer)) `
            "Unexpected diagnostic location for $Name`: $($result.diagnostic.detail)"
    }
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)
Test-Candidate 'valid.pae.json' $config $true 0 ''
Test-Candidate 'old-schema.pae.json' `
    ($config.Replace('"schema_version": "0.7"', '"schema_version": "0.6"')) `
    $false 4 '/messages/0/fields/0/computed'
Test-Candidate 'invalid-width.pae.json' `
    ($config.Replace('"byte_width": 2, "byte_order": "big_endian"', `
                     '"byte_width": 3, "byte_order": "big_endian"')) `
    $false 4 '/messages/0/fields/0/computed'
Test-Candidate 'missing-multibyte-order.pae.json' `
    ($config.Replace(', "byte_order": "big_endian"', '')) `
    $false 4 '/messages/0/fields/0/wire'
Test-Candidate 'single-byte-order.pae.json' `
    ($config.Replace('"byte_width": 1}', '"byte_width": 1, "byte_order": "little_endian"}')) `
    $false 4 '/messages/1/fields/0/computed'
Test-Candidate 'empty-region.pae.json' `
    ($config.Replace('"range": {"byte_offset": 2, "byte_length": 3}', `
                     '"range": {"byte_offset": 2, "byte_length": 0}')) `
    $false 4 '/messages/1/fields/0/computed/range/byte_length'
Test-Candidate 'unknown-computed-property.pae.json' `
    ($config.Replace('"computed": {"kind": "length", "scope": "frame"}', `
                     '"computed": {"kind": "length", "scope": "frame", "unit": "bytes"}')) `
    $false 4 '/messages/0/fields/0/computed'

Write-Output 'PAE_LENGTH_SCHEMA_PARITY_PASS'
