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
    $output = & $LabExecutable encode --config $path --values $ValuesPath --output json
    Assert-Condition ($LASTEXITCODE -eq $ExpectedExit) "Lab exit mismatch for $Name"
    $result = $output | Out-String | ConvertFrom-Json
    if ($ExpectedExit -eq 0) {
        Assert-Condition ($result.format_version -eq 'pae.lab.result/0.5') 'Schema 0.4 did not select Result 0.5'
    } else {
        Assert-Condition ($result.diagnostic.id -eq 'PAE_LAB_CONFIG_COMPILE_FAILED') "Unexpected diagnostic for $Name"
        Assert-Condition ($result.diagnostic.detail -match $ExpectedPointer) "Unexpected location for $Name`: $($result.diagnostic.detail)"
    }
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)
Test-Candidate 'valid.pae.json' $config $true 0 '^$'
Test-Candidate 'old-generation.pae.json' ($config.Replace('"schema_version": "0.4"', '"schema_version": "0.3"')) $false 4 '^/messages/0/fields/0/value_type:'
Test-Candidate 'fractional-constant.pae.json' ($config.Replace('"value":-2', '"value":-2.5')) $false 4 '^/messages/0/fields/2/encode/value:'
Test-Candidate 'unsupported-scale.pae.json' ($config.Replace('"value_type":"INT64"', '"value_type":"INT64","scale":1')) $false 4 '^/messages/0/fields/0/scale:'
Test-Candidate 'signed-bitfield.pae.json' ($config.Replace('{"codec":"unsigned_integer","byte_offset":0,"byte_width":3,"byte_order":"big_endian"}', '{"codec":"bitfield","container_id":"flags","bit_offset":1,"bit_width":2}')) $false 4 '^/messages/0/fields/0/wire:'
Test-Candidate 'wire-overflow.pae.json' ($config.Replace('"value":-2', '"value":549755813888')) $true 4 '^/messages/0/fields/2/encode/value:'
Test-Candidate 'signed-negative-zero.pae.json' ($config.Replace('"value":-2', '"value":-0')) $true 0 '^$'

Write-Output 'PAE_DEC042A_SCHEMA_PARITY_PASS'
