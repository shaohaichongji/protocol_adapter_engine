[CmdletBinding()]
param(
    [switch]$Execute,
    [switch]$VerifyAfter
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

throw @'
This one-time cleanup script is permanently retired and cannot be executed.
Its original bytes and SHA-256 are retained under the Git-ignored portable-path evidence root.
Do not generalize or reuse the historical deletion scope; create a newly reviewed script for any future cleanup.
'@
