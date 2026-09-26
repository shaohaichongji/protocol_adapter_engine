param(
  [Parameter(Mandatory = $true)][string]$ProbeExe,
  [Parameter(Mandatory = $true)][string]$RepoRoot
)

$ErrorActionPreference = 'Stop'
$pairs = @(
  @('synthetic_crc_slice', 'synthetic_crc_slice.pae.json'),
  @('synthetic_ascii_literal_only', 'synthetic_ascii_literal_only.pae.json')
)

foreach ($pair in $pairs) {
  $yamlPath = Join-Path $PSScriptRoot "fixtures/$($pair[0]).pae.yaml"
  $jsonPath = Join-Path $RepoRoot "examples/config/$($pair[1])"
  $converted = & $ProbeExe --convert $yamlPath
  if ($LASTEXITCODE -ne 0) { throw "Conversion failed: $($pair[0])" }
  $actual = $converted | ConvertFrom-Json -AsHashtable | ConvertTo-Json -Compress -Depth 100
  $expected = Get-Content -LiteralPath $jsonPath -Raw -Encoding UTF8 |
    ConvertFrom-Json -AsHashtable | ConvertTo-Json -Compress -Depth 100
  if ($actual -cne $expected) { throw "Structural mismatch: $($pair[0])" }
  Write-Output "PASS full public config structural equality: $($pair[0])"
}
