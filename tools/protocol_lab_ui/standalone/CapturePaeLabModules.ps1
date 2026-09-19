param(
    [Parameter(Mandatory = $true)][string]$DeployRoot,
    [Parameter(Mandatory = $true)][string]$QtInputRoot,
    [Parameter(Mandatory = $true)][ValidateSet('Debug', 'Release')][string]$Configuration,
    [string]$PaeInputDll,
    [Parameter(Mandatory = $true)][string]$OutputLog
)

$ErrorActionPreference = 'Stop'
$deploy = [System.IO.Path]::GetFullPath($DeployRoot)
$qt = [System.IO.Path]::GetFullPath($QtInputRoot)
$output = [System.IO.Path]::GetFullPath($OutputLog)
if (Test-Path -LiteralPath $output) {
    throw "OutputLog already exists; refusing to overwrite: $output"
}

$suffix = if ($Configuration -eq 'Debug') { 'd' } else { '' }
$qtConfig = $Configuration.ToLowerInvariant()
$exe = Join-Path $deploy 'pae_protocol_lab_ui.exe'
$pluginDirectory = Join-Path $deploy 'platforms'
$expected = @(
    [pscustomobject]@{ Name = "Qt5Core${suffix}.dll"; Input = Join-Path $qt "bin\$qtConfig\Qt5Core${suffix}.dll"; Deploy = Join-Path $deploy "Qt5Core${suffix}.dll" },
    [pscustomobject]@{ Name = "Qt5Gui${suffix}.dll"; Input = Join-Path $qt "bin\$qtConfig\Qt5Gui${suffix}.dll"; Deploy = Join-Path $deploy "Qt5Gui${suffix}.dll" },
    [pscustomobject]@{ Name = "Qt5Widgets${suffix}.dll"; Input = Join-Path $qt "bin\$qtConfig\Qt5Widgets${suffix}.dll"; Deploy = Join-Path $deploy "Qt5Widgets${suffix}.dll" },
    [pscustomobject]@{ Name = "qwindows${suffix}.dll"; Input = Join-Path $qt "bin\$qtConfig\platforms\qwindows${suffix}.dll"; Deploy = Join-Path $pluginDirectory "qwindows${suffix}.dll" }
)
if (-not [string]::IsNullOrWhiteSpace($PaeInputDll)) {
    $expected += [pscustomobject]@{
        Name = 'pae.dll'
        Input = [System.IO.Path]::GetFullPath($PaeInputDll)
        Deploy = Join-Path $deploy 'pae.dll'
    }
}
foreach ($path in @($exe, $pluginDirectory) + $expected.Input + $expected.Deploy) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required capture input is missing: $path" }
}
foreach ($item in $expected) {
    $inputHash = (Get-FileHash -LiteralPath $item.Input -Algorithm SHA256).Hash
    $deployHash = (Get-FileHash -LiteralPath $item.Deploy -Algorithm SHA256).Hash
    if ($inputHash -ne $deployHash) {
        throw "Runtime source hash mismatch before launch: $($item.Name)"
    }
}

$oldPath = $env:Path
$oldPluginPath = $env:QT_PLUGIN_PATH
$oldPlatformPluginPath = $env:QT_QPA_PLATFORM_PLUGIN_PATH
$process = $null
try {
    $env:Path = "$deploy;$oldPath"
    $env:QT_PLUGIN_PATH = $pluginDirectory
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $pluginDirectory
    $process = Start-Process -FilePath $exe -WorkingDirectory $deploy -WindowStyle Hidden -PassThru

    $loaded = @{}
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 200
        if ($process.HasExited) { throw "Lab exited before module capture, exit code $($process.ExitCode)" }
        $process.Refresh()
        foreach ($module in $process.Modules) {
            $loaded[$module.ModuleName.ToLowerInvariant()] = [System.IO.Path]::GetFullPath($module.FileName)
        }
    } while (($expected | Where-Object { -not $loaded.ContainsKey($_.Name.ToLowerInvariant()) }).Count -ne 0 -and
             [DateTime]::UtcNow -lt $deadline)

    $records = foreach ($item in $expected) {
        $key = $item.Name.ToLowerInvariant()
        if (-not $loaded.ContainsKey($key)) { throw "Expected Qt module was not loaded: $($item.Name)" }
        $loadedPath = $loaded[$key]
        $loadedHash = (Get-FileHash -LiteralPath $loadedPath -Algorithm SHA256).Hash
        $deployHash = (Get-FileHash -LiteralPath $item.Deploy -Algorithm SHA256).Hash
        $inputHash = (Get-FileHash -LiteralPath $item.Input -Algorithm SHA256).Hash
        [pscustomobject]@{
            name = $item.Name
            loaded_path = $loadedPath
            deploy_path = [System.IO.Path]::GetFullPath($item.Deploy)
            input_path = [System.IO.Path]::GetFullPath($item.Input)
            loaded_sha256 = $loadedHash
            deploy_sha256 = $deployHash
            input_sha256 = $inputHash
            loaded_is_deploy_file = $loadedPath -eq [System.IO.Path]::GetFullPath($item.Deploy)
            hashes_match = $loadedHash -eq $deployHash -and $deployHash -eq $inputHash
        }
    }

    $evidence = [ordered]@{
        captured_utc = [DateTime]::UtcNow.ToString('o')
        configuration = $Configuration
        pid = $process.Id
        executable = [System.IO.Path]::GetFullPath($exe)
        controlled_environment = [ordered]@{
            path_prefix = $deploy
            qt_plugin_path = $pluginDirectory
            qt_qpa_platform_plugin_path = $pluginDirectory
            global_environment_modified = $false
        }
        modules = $records
        all_loaded_from_deploy = ($records.loaded_is_deploy_file -notcontains $false)
        all_hashes_match = ($records.hashes_match -notcontains $false)
    }
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($output)) | Out-Null
    [System.IO.File]::WriteAllText($output, ($evidence | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))
    $output
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $env:Path = $oldPath
    $env:QT_PLUGIN_PATH = $oldPluginPath
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = $oldPlatformPluginPath
}
