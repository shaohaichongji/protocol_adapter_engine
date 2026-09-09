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
        Assert-Condition ($result.result.format_version -eq 'pae.lab.result/0.7') `
            'Valid Schema 0.6 did not select Result 0.7'
        Assert-Condition ($result.result.frame_hex -eq '31323334353637383929B1AA') `
            'Valid CRC config did not produce the published check frame'
    } else {
        Assert-Condition ($result.diagnostic.detail -match [regex]::Escape($ExpectedPointer)) `
            "Unexpected diagnostic location for $Name`: $($result.diagnostic.detail)"
    }
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)
Test-Candidate 'valid.pae.json' $config $true 0 ''
Test-Candidate 'old-schema.pae.json' ($config.Replace('"schema_version": "0.6"', '"schema_version": "0.5"')) `
    $false 4 '/messages/0/integrity/parameters'
Test-Candidate 'width.pae.json' ($config.Replace('"width": 16', '"width": 24')) `
    $false 4 '/messages/0/integrity/parameters/width'
Test-Candidate 'lowercase.pae.json' ($config.Replace('"poly": "1021"', '"poly": "102a"')) `
    $false 4 '/messages/0/integrity/parameters/poly'
Test-Candidate 'short-poly.pae.json' ($config.Replace('"poly": "1021"', '"poly": "021"')) `
    $false 4 '/messages/0/integrity/parameters/poly'
Test-Candidate 'zero-poly.pae.json' ($config.Replace('"poly": "1021"', '"poly": "0000"')) `
    $true 4 '/messages/0/integrity/parameters/poly'
Test-Candidate 'even-poly.pae.json' ($config.Replace('"poly": "1021"', '"poly": "1020"')) `
    $true 4 '/messages/0/integrity/parameters/poly'
Test-Candidate 'invalid-refin-type.pae.json' ($config.Replace('"refin": false', '"refin": "false"')) `
    $false 4 '/messages/0/integrity/parameters/refin'
Test-Candidate 'missing-refout.pae.json' ($config.Replace('          "refout": false,', '')) `
    $false 4 '/messages/0/integrity/parameters/refout'
Test-Candidate 'unknown-parameter.pae.json' ($config.Replace('"xorout": "0000"', '"xorout": "0000", "seed": "0000"')) `
    $false 4 '/messages/0/integrity/parameters'
Test-Candidate 'storage-order.pae.json' ($config.Replace(', "byte_order": "big_endian"', '')) `
    $false 4 '/messages/0/integrity/storage/byte_order'

Write-Output 'PAE_CRC_SCHEMA_PARITY_PASS'
