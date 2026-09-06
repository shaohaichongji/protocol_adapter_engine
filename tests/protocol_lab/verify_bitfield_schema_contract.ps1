param(
    [Parameter(Mandatory = $true)][string]$LabExecutable,
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$ConfigPath,
    [Parameter(Mandatory = $true)][string]$ValuesPath,
    [Parameter(Mandatory = $true)][string]$TestRoot
)

$ErrorActionPreference = 'Stop'

function Assert-Condition {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Invoke-LabCompile {
    param([string]$Candidate, [int]$ExpectedExit)
    $output = & $LabExecutable encode --config $Candidate --values $ValuesPath --output json
    $actualExit = $LASTEXITCODE
    Assert-Condition ($actualExit -eq $ExpectedExit) "Lab exit mismatch for $Candidate`: expected=$ExpectedExit actual=$actualExit"
    return ($output | Out-String | ConvertFrom-Json)
}

function Write-Candidate {
    param([string]$Name, [string]$Text)
    $path = Join-Path $TestRoot $Name
    [System.IO.File]::WriteAllText($path, $Text, [System.Text.UTF8Encoding]::new($false))
    return $path
}

function Assert-Schema {
    param([string]$Text, [bool]$Expected, [string]$Case)
    $actual = Test-Json -Json $Text -SchemaFile $SchemaPath -ErrorAction SilentlyContinue
    Assert-Condition ($actual -eq $Expected) "JSON Schema mismatch for $Case`: expected=$Expected actual=$actual"
}

[System.IO.Directory]::CreateDirectory($TestRoot) | Out-Null
$config = [System.IO.File]::ReadAllText($ConfigPath)

Assert-Schema $config $true 'single-byte byte_order omitted'
$valid = Invoke-LabCompile $ConfigPath 0
Assert-Condition ($valid.format_version -eq 'pae.lab.result/0.3') 'valid Schema 0.2 config did not execute as Result 0.3'

foreach ($order in @('big_endian', 'little_endian')) {
    $needle = '"container_width": 1, "bit_numbering": "lsb0"'
    $replacement = '"container_width": 1, "byte_order": "' + $order + '", "bit_numbering": "lsb0"'
    $candidateText = $config.Replace($needle, $replacement)
    Assert-Condition ($candidateText -ne $config) "single-byte mutation was not applied for $order"
    Assert-Schema $candidateText $false "single-byte explicit $order"
    $candidate = Write-Candidate "single-byte-$order.pae.json" $candidateText
    $result = Invoke-LabCompile $candidate 4
    Assert-Condition ($result.diagnostic.id -eq 'PAE_LAB_CONFIG_COMPILE_FAILED') "unexpected diagnostic id for single-byte $order"
    Assert-Condition ($result.diagnostic.detail -eq '/messages/0/bit_containers/0/byte_order: single-byte bit container must omit byte_order') "compiler diagnostic lacks stable property path for single-byte $order"
}

$multiMissingText = $config.Replace('"container_width": 2, "byte_order": "big_endian", "bit_numbering": "lsb0"', '"container_width": 2, "bit_numbering": "lsb0"')
Assert-Condition ($multiMissingText -ne $config) 'multi-byte missing-order mutation was not applied'
Assert-Schema $multiMissingText $false 'multi-byte missing byte_order'
$multiMissing = Write-Candidate 'multi-byte-missing-order.pae.json' $multiMissingText
$multiMissingResult = Invoke-LabCompile $multiMissing 4
Assert-Condition ($multiMissingResult.diagnostic.detail -eq '/messages/0/bit_containers/1/byte_order: multi-byte bit container requires an explicit byte order') 'multi-byte missing byte_order did not fail at the expected property'

$invalidOrderText = $config.Replace('"byte_order": "big_endian"', '"byte_order": "network_order"')
Assert-Condition ($invalidOrderText -ne $config) 'invalid-order mutation was not applied'
Assert-Schema $invalidOrderText $false 'invalid multi-byte byte_order'
$invalidOrder = Write-Candidate 'multi-byte-invalid-order.pae.json' $invalidOrderText
$invalidOrderResult = Invoke-LabCompile $invalidOrder 4
Assert-Condition ($invalidOrderResult.diagnostic.detail -match '^/messages/0/bit_containers/1/byte_order:') 'invalid byte_order did not fail at the expected property'

Write-Output 'PAE_DEC040_SCHEMA_PARITY_PASS'
