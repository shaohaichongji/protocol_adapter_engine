param(
    [Parameter(Mandatory = $true)][string]$LabExecutable,
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$ConfigPath,
    [Parameter(Mandatory = $true)][string]$ValuesPath,
    [Parameter(Mandatory = $true)][string]$FixedConfigPath,
    [Parameter(Mandatory = $true)][string]$FixedValuesPath,
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
    $output = & $LabExecutable encode --config $path --values $ValuesPath `
        --record-root (Join-Path $TestRoot "$Name-runs") --output json
    Assert-Condition ($LASTEXITCODE -eq $ExpectedExit) "Lab exit mismatch for $Name"
    $result = $output | Out-String | ConvertFrom-Json
    if ($ExpectedExit -eq 0) {
        Assert-Condition ($result.result.format_version -eq 'pae.lab.result/0.9') `
            'Valid Schema 0.8 did not select Result 0.9'
        Assert-Condition ($result.result.frame_hex -eq 'A5051020DA') `
            'Valid bounded config did not produce the public vector'
    } else {
        Assert-Condition ($result.diagnostic.detail -match [regex]::Escape($ExpectedPointer)) `
            "Unexpected diagnostic location for $Name`: $($result.diagnostic.detail)"
    }
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)
Test-Candidate 'valid.pae.json' $config $true 0 ''
Test-Candidate 'old-schema.pae.json' `
    ($config.Replace('"schema_version": "0.8"', '"schema_version": "0.7"')) `
    $false 4 '/messages/0'
Test-Candidate 'bounded-with-fixed-length.pae.json' `
    ($config.Replace('"layout": {', '"frame_length_bytes": 6, "layout": {')) `
    $false 4 '/messages/0/frame_length_bytes'
Test-Candidate 'payload-with-byte-length.pae.json' `
    ($config.Replace('"wire": {"codec": "bytes", "byte_offset": 2}', `
                     '"wire": {"codec": "bytes", "byte_offset": 2, "byte_length": 3}')) `
    $true 4 '/messages/0/fields/1/wire'
Test-Candidate 'sum8-with-byte-order.pae.json' `
    ($config.Replace('"storage": {"anchor": "payload_end"}', `
                     '"storage": {"anchor": "payload_end", "byte_order": "big_endian"}')) `
    $false 4 '/messages/0/integrity/storage/byte_order'

$fixedConfig = [System.IO.File]::ReadAllText($FixedConfigPath).Replace(
    '"schema_version": "0.7"', '"schema_version": "0.8"')
$fixedPath = Join-Path $TestRoot 'fixed-message-v08.pae.json'
[System.IO.File]::WriteAllText($fixedPath, $fixedConfig, [System.Text.UTF8Encoding]::new($false))
Assert-Condition (Test-Json -Json $fixedConfig -SchemaFile $SchemaPath) `
    'Schema 0.8 fixed Message must remain structurally valid'
$fixedOutput = & $LabExecutable encode --config $fixedPath --values $FixedValuesPath `
    --record-root (Join-Path $TestRoot 'fixed-message-v08-runs') --output json
Assert-Condition ($LASTEXITCODE -eq 0) 'Schema 0.8 fixed Message execution failed'
$fixedResult = $fixedOutput | Out-String | ConvertFrom-Json
Assert-Condition ($fixedResult.result.format_version -eq 'pae.lab.result/0.9') `
    'Schema 0.8 fixed Message did not select Result 0.9'
Assert-Condition ($fixedResult.result.operation_status -eq 'OK') `
    'Schema 0.8 fixed Message did not complete successfully'

Write-Output 'PAE_VARIABLE_RECORD_SCHEMA_PARITY_PASS'
