[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$name = 'PAE-Lab-Windows-x64-adeae30'
$source = [IO.Path]::GetFullPath((Join-Path $repo "deliverables/sdk/adeae30-experience/$name"))
$destination = Join-Path $repo 'deliverables/sdk/adeae30-tutorial-v2'
if (Test-Path -LiteralPath $destination) { throw 'Target exists; do not overwrite an earlier bundle.' }
$identity = Get-Content -LiteralPath "$source/BUNDLE-IDENTITY.json" -Raw -Encoding UTF8 | ConvertFrom-Json
if ($identity.product_source_head -ne 'adeae30d942ba42cc518c244c220730b7e462d2c') { throw 'Unexpected base identity' }
foreach ($line in Get-Content -LiteralPath "$source/SHA256SUMS.txt") {
  if ($line.StartsWith('#') -or -not $line.Trim()) { continue }
  if ($line -notmatch '^([a-fA-F0-9]{64})  (.+)$') { throw 'Invalid base hash line' }
  $expected = $Matches[1]; $relative = $Matches[2]
  $path = [IO.Path]::GetFullPath((Join-Path $source $relative))
  if (-not $path.StartsWith($source + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe base path' }
  if ((Get-FileHash -LiteralPath $path).Hash -ne $expected) { throw "Base hash mismatch: $relative" }
}
New-Item -ItemType Directory -Path $destination | Out-Null
$target = [IO.Path]::GetFullPath((Join-Path $destination $name))
Copy-Item -LiteralPath $source -Destination $target -Recurse
Copy-Item -LiteralPath (Join-Path $repo 'docs/experience/tutorials') -Destination "$target/tutorials" -Recurse
foreach ($doc in @('README.md', '03-配置与边界.md')) {
  Copy-Item -LiteralPath (Join-Path $repo "docs/experience/$doc") -Destination "$target/$doc" -Force
}
$identity | Add-Member -NotePropertyName tutorial_overlay -NotePropertyValue 'chinese-config-tutorial/2'
$identity | Add-Member -NotePropertyName tutorial_note -NotePropertyValue 'Additive teaching configs and documentation; SDK and Lab payload unchanged; not a rebuild.'
$encoding = [Text.UTF8Encoding]::new($false)
[IO.File]::WriteAllText("$target/BUNDLE-IDENTITY.json", ($identity | ConvertTo-Json -Depth 12), $encoding)
$files = @(Get-ChildItem -LiteralPath $target -File -Recurse | Where-Object { $_.FullName -notin @("$target/MANIFEST.txt".Replace('/', '\'), "$target/SHA256SUMS.txt".Replace('/', '\')) } | Sort-Object FullName)
$hashes = @(); $manifest = @()
foreach ($file in $files) {
  $relative = $file.FullName.Substring($target.Length + 1).Replace('\', '/')
  $hashes += "$((Get-FileHash -LiteralPath $file.FullName).Hash.ToLowerInvariant())  $relative"
  $manifest += "$($file.Length)  $relative"
}
[IO.File]::WriteAllLines("$target/SHA256SUMS.txt", $hashes, $encoding)
[IO.File]::WriteAllLines("$target/MANIFEST.txt", $manifest, $encoding)
Compress-Archive -LiteralPath $target -DestinationPath "$destination/$name.zip"
[IO.File]::WriteAllText("$destination/ZIP.sha256", "$((Get-FileHash -LiteralPath "$destination/$name.zip").Hash.ToLowerInvariant())  $name.zip`n", $encoding)
Write-Output "Created $destination"
