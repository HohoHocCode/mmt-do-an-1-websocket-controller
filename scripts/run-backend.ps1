param(
  [string]$Preset = "mingw64",
  [string]$Host = "127.0.0.1",
  [int]$Port = 8080,
  [string]$DbHost = "127.0.0.1",
  [int]$DbPort = 3306,
  [string]$DbUser = "root",
  [string]$DbPassword = "",
  [string]$DbName = "mmt_remote"
)

$root = Resolve-Path (Join-Path $PSScriptRoot "..")
$preferredBinary = Join-Path $root "build\$Preset\mmt_api.exe"
$fallbackBinary = Join-Path $root "build\mmt_api.exe"

if (Test-Path $preferredBinary) {
  $binary = $preferredBinary
} elseif (Test-Path $fallbackBinary) {
  $binary = $fallbackBinary
} else {
  Write-Error "Could not find mmt_api.exe. Build it first (expected $preferredBinary or $fallbackBinary)."
  exit 1
}

$env:HOST = $Host
$env:PORT = "$Port"
$env:API_PORT = "$Port"
$env:DB_HOST = $DbHost
$env:DB_PORT = "$DbPort"
$env:DB_USER = $DbUser
$env:DB_PASSWORD = $DbPassword
$env:DB_NAME = $DbName

Write-Host "Starting C++ API: $binary"
Write-Host "Health check: http://localhost:$Port/health"

& $binary
