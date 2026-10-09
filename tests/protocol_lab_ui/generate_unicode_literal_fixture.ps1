param([Parameter(Mandatory=$true)][string]$Source, [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$strictUtf8 = [Text.UTF8Encoding]::new($false, $true)
$sourceText = $strictUtf8.GetString([IO.File]::ReadAllBytes($Source))
$literalPattern = '(?:u)?"(?:\\.|[^"\\])*"'
$macroPattern = 'QStringLiteral\(\s*(?<argument>(?:(?:u)?"(?:\\.|[^"\\])*"\s*)+)\)'
$macros = [regex]::Matches($sourceText, $macroPattern)
if ($macros.Count -ne [regex]::Matches($sourceText, '\bQStringLiteral\s*\(').Count) {
  throw 'Unsupported QStringLiteral argument shape: update the scanner explicitly'
}
$lines = [Collections.Generic.List[string]]::new()
$count = 0
foreach ($macro in $macros) {
  $argument = $macro.Groups['argument'].Value
  if ($argument -notmatch '[^\x00-\x7F]') { continue }
  $decoded = ''
  foreach ($literal in [regex]::Matches($argument, $literalPattern)) {
    $token = $literal.Value -replace '^u', ''
    $body = $token.Substring(1, $token.Length - 2)
    $decoded += [regex]::Replace($body, '\\(.)', [Text.RegularExpressions.MatchEvaluator]{
      param($escape)
      switch -CaseSensitive ($escape.Groups[1].Value) {
        'n' { return "`n" }
        'r' { return "`r" }
        't' { return "`t" }
        '"' { return '"' }
        '\' { return '\' }
        default { throw "Unsupported literal escape: $($escape.Value)" }
      }
    })
  }
  # Expected integers come from source bytes decoded by .NET, not from an MSVC literal.
  $units = ($decoded.ToCharArray() | ForEach-Object { '0x{0:X4}U' -f [int]$_ }) -join ', '
  $bytes = ($strictUtf8.GetBytes($decoded) | ForEach-Object { '0x{0:X2}U' -f $_ }) -join ', '
  ++$count
  $lines.Add("CheckProduct($count, $($macro.Value), {$units}, {$bytes});")
}
if ($count -eq 0) { throw 'No product Unicode literals found' }
$lines.Add("std::printf(`"PRODUCT_COUNT=$count\n`");")
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Output)) | Out-Null
[IO.File]::WriteAllText($Output, ($lines -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
"Extracted $count product calls from $Source"
