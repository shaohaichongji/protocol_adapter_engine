param(
    [Parameter(Mandatory = $true)][string]$SchemaPath,
    [Parameter(Mandatory = $true)][string]$SamplePath,
    [Parameter(Mandatory = $true)][string]$InheritedPath
)

$ErrorActionPreference = 'Stop'

function Test-Schema([string]$Text) {
    return Test-Json -Json $Text -SchemaFile $SchemaPath -ErrorAction SilentlyContinue
}

function Replace-Once([string]$Text, [string]$Needle, [string]$Replacement) {
    $index = $Text.IndexOf($Needle, [System.StringComparison]::Ordinal)
    if ($index -lt 0) {
        throw "Mutation token not found: $Needle"
    }
    return $Text.Substring(0, $index) + $Replacement + $Text.Substring($index + $Needle.Length)
}

$sample = Get-Content -Raw -LiteralPath $SamplePath
$inherited = Get-Content -Raw -LiteralPath $InheritedPath
$inheritedV05 = Replace-Once $inherited '"schema_version": "0.1"' '"schema_version": "0.5"'

$checks = [System.Collections.Generic.List[object]]::new()
$checks.Add(@('valid_conversion_sample', $sample, $true))
$checks.Add(@('valid_inherited_v01', $inherited, $true))
$checks.Add(@('valid_inherited_v05', $inheritedV05, $true))
$checks.Add(@(
        'reject_unknown_conversion_property',
        (Replace-Once $sample '"kind": "linear"' '"kind": "linear", "unexpected": 1'),
        $false
    ))
$checks.Add(@(
        'reject_constant_conversion',
        (Replace-Once $sample '"encode": {"source": "input"}' `
            '"encode": {"source": "constant", "value": 1}'),
        $false
    ))
$checks.Add(@(
        'reject_bitfield_conversion',
        (Replace-Once $sample '"value_type": "BOOL"' `
            '"value_type": "BOOL", "conversion": {"kind": "linear", "output_type": "DECIMAL64", "scale": {"numerator": 1, "denominator": 1}, "bias": {"numerator": 0, "denominator": 1}}'),
        $false
    ))
$checks.Add(@(
        'reject_author_denominator_over_limit',
        (Replace-Once $sample '"denominator": 6' '"denominator": 1000000000000000001'),
        $false
    ))

foreach ($check in $checks) {
    $actual = Test-Schema $check[1]
    if ($actual -ne $check[2]) {
        throw "Schema contract failed: $($check[0]) expected=$($check[2]) actual=$actual"
    }
    Write-Output "PASS case=$($check[0])"
}

Write-Output "DECIMAL_COMPILER_SCHEMA_TEST_SUMMARY passed=$($checks.Count) failed=0"
