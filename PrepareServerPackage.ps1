param(
    [Parameter(Mandatory = $true)]
    [string]$QtBin,
    [string]$SettingsPath = (Join-Path $PSScriptRoot 'server.local.json'),
    [string]$DestinationPath = (Join-Path $PSScriptRoot 'out/msa-server-debug.zip')
)

$ErrorActionPreference = 'Stop'
$serverPath = Join-Path $PSScriptRoot 'out/build/windows-debug-local/Debug/msa_server.exe'
$deployTool = Join-Path $QtBin 'windeployqt.exe'
$qtRoot = [IO.Path]::GetFullPath((Join-Path $QtBin '..')) + [IO.Path]::DirectorySeparatorChar
$settingsFile = [IO.Path]::GetFullPath($SettingsPath)
$archivePath = [IO.Path]::GetFullPath($DestinationPath)

foreach ($requiredFile in @($serverPath, $deployTool, $settingsFile)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required file is missing: $requiredFile"
    }
}
if (Test-Path -LiteralPath $archivePath) {
    throw 'Destination already exists. Choose another -DestinationPath; existing files are not overwritten.'
}
if (-not (Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($archivePath)) -PathType Container)) {
    throw 'Destination directory is missing. Build the project first or choose an existing directory.'
}

# Берём зависимости проверенного Debug-бинарника; средства разработки на другом ПК уже установлены.
$mapping = & $deployTool --debug --dry-run --no-compiler-runtime --no-translations --no-quick-import --list mapping $serverPath
if ($LASTEXITCODE -ne 0) {
    throw 'windeployqt failed; archive was not created.'
}
$files = [ordered]@{ 'msa_server.exe' = $serverPath; 'server.local.json' = $settingsFile }
foreach ($line in $mapping) {
    if ($line -notmatch '^"([^"]+)"\s+"([^"]+)"$') { continue }
    $sourcePath = [IO.Path]::GetFullPath($Matches[1])
    $relativePath = $Matches[2].Replace('\', '/')
    # Windows DLL остаются системными; сервер использует только SQLite и штатный TLS Windows.
    if (-not $sourcePath.StartsWith($qtRoot, [StringComparison]::OrdinalIgnoreCase)) { continue }
    if ($relativePath.StartsWith('sqldrivers/') -and $relativePath -ne 'sqldrivers/qsqlited.dll') { continue }
    if ($relativePath -eq 'tls/qopensslbackendd.dll') { continue }
    if ([IO.Path]::IsPathRooted($relativePath) -or $relativePath.Split('/') -contains '..') {
        throw 'Unexpected destination path from windeployqt.'
    }
    $files[$relativePath] = $sourcePath
}
foreach ($requiredEntry in @('Qt6Cored.dll', 'Qt6Networkd.dll', 'Qt6Sqld.dll', 'sqldrivers/qsqlited.dll')) {
    if (-not $files.Contains($requiredEntry)) { throw "Qt dependency is missing: $requiredEntry" }
}
foreach ($sourcePath in $files.Values) {
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw 'A deployment source file is missing.' }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($archivePath, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($entry in $files.GetEnumerator()) {
        [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive, $entry.Value, $entry.Key, [IO.Compression.CompressionLevel]::Optimal)
    }
    $texts = [ordered]@{
        'qt.conf' = "[Paths]`nPrefix=.`nPlugins=.`n"
        'StartServer.cmd' = @'
@echo off
chcp 65001 >nul
cd /d "%~dp0"
"%~dp0msa_server.exe" --config "%~dp0server.local.json"
set "serverExitCode=%ERRORLEVEL%"
echo Server stopped. Exit code: %serverExitCode%
pause
exit /b %serverExitCode%
'@
        'README.txt' = @'
Private Debug package for the first test on Windows 11 x64 with Visual Studio C++ installed.
Qt DLLs and the SQLite driver are included. No Qt installation path is required.
Extract the WHOLE archive on the work laptop. Do not run from inside the ZIP.
Start llama-server on that laptop, then double-click StartServer.cmd.
Keep the console open. Ctrl+C stops the server. Startup must report the TCP port.
The phone uses listen_address, port and token from server.local.json.
This archive contains your private token. Do not add it or server.local.json to Git.
The SQLite database is created beside server.local.json. Keep it for subsequent runs.
services: null disables search, weather and routes; chat and planner remain available.
This package is for a first test, not a Release distribution. Actual startup on the work laptop is unverified.
'@
    }
    foreach ($entry in $texts.GetEnumerator()) {
        $writer = [IO.StreamWriter]::new($archive.CreateEntry($entry.Key).Open(), [Text.UTF8Encoding]::new($false))
        try { $writer.Write($entry.Value.Replace("`r`n", "`n").Replace("`n", "`r`n")) } finally { $writer.Dispose() }
    }
} finally { $archive.Dispose() }

Write-Output "Created private Debug package: $archivePath"
