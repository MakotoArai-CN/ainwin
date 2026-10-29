# Builds suixin.exe with Zig (0.17+). On a machine with MSVC, CMake works too.
param([switch]$Clean)

$ErrorActionPreference = "Stop"
Push-Location $PSScriptRoot
try {
    if ($Clean) {
        Remove-Item -Recurse -Force "zig-out", ".zig-cache" -ErrorAction SilentlyContinue
    }
    if (-not (Get-Command zig -ErrorAction SilentlyContinue)) {
        throw "Zig was not found. Install it from https://ziglang.org/download/ and run this again."
    }
    zig build -Doptimize=fast
    if (-not (Test-Path "suixin.exe")) {
        throw "zig build finished but suixin.exe is missing."
    }
    Write-Host ("suixin.exe built: " + (Get-Item "suixin.exe").Length + " bytes")
} finally {
    Pop-Location
}
