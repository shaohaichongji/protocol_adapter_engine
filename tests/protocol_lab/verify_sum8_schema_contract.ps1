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
                        [int]$ExpectedExit, [string]$ExpectedDetailPattern) {
    $path = Join-Path $TestRoot $Name
    [System.IO.File]::WriteAllText($path, $Text, [System.Text.UTF8Encoding]::new($false))
    $schemaResult = Test-Json -Json $Text -SchemaFile $SchemaPath -ErrorAction SilentlyContinue
    Assert-Condition ($schemaResult -eq $SchemaValid) "Schema mismatch for $Name"
    $output = & $LabExecutable encode --config $path --values $ValuesPath --output json
    Assert-Condition ($LASTEXITCODE -eq $ExpectedExit) "Lab exit mismatch for $Name"
    $result = $output | Out-String | ConvertFrom-Json
    if ($ExpectedExit -eq 0) {
        Assert-Condition ($result.format_version -eq 'pae.lab.result/0.4') "Valid Schema 0.3 did not select Result 0.4"
    } else {
        Assert-Condition ($result.diagnostic.id -eq 'PAE_LAB_CONFIG_COMPILE_FAILED') "Unexpected diagnostic for $Name"
        Assert-Condition ($result.diagnostic.detail -match $ExpectedDetailPattern) "Unexpected diagnostic location for $Name`: $($result.diagnostic.detail)"
    }
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)
Test-Candidate 'valid.pae.json' $config $true 0 '^$'

$old = $config.Replace('"schema_version": "0.3"', '"schema_version": "0.2"')
Test-Candidate 'schema-v02.pae.json' $old $false 4 '^/messages/0/integrity:'

$nullRule = $config -replace '(?s)"integrity": \{.*?"storage": \{"byte_offset": 5\}\n      \}', '"integrity": null'
Test-Candidate 'null.pae.json' $nullRule $false 4 '^/messages/0/integrity:'

$unknown = $config.Replace('"algorithm": "sum8"', '"algorithm": "sum8", "width": 1')
Test-Candidate 'unknown-property.pae.json' $unknown $false 4 '^/messages/0/integrity/width:'

$unknownAlgorithm = $config.Replace('"algorithm": "sum8"', '"algorithm": "crc"')
Test-Candidate 'unknown-algorithm.pae.json' $unknownAlgorithm $false 4 '^/messages/0/integrity/algorithm:'

$floatOffset = $config.Replace('"range": {"byte_offset": 1,', '"range": {"byte_offset": 1.5,')
Test-Candidate 'float-offset.pae.json' $floatOffset $false 4 '^/messages/0/integrity/range/byte_offset:'

Write-Output 'PAE_DEC041_SCHEMA_PARITY_PASS'
