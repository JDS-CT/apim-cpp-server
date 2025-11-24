$ErrorActionPreference = 'Stop'

param(
  [ValidateSet("smoke", "all")]
  [string]$Label = "smoke",
  [string]$BuildDir = "build"
)

function Ensure-Build {
  param($Dir)
  if (-not (Test-Path "$Dir/CMakeCache.txt")) {
    Write-Host "Configuring with CMake (Ninja)..."
    cmake -S . -B $Dir -G Ninja | Write-Host
  }
}

Ensure-Build -Dir $BuildDir

Write-Host "Building..."
cmake --build $BuildDir | Write-Host

Write-Host "Running ctest (label=$Label)..."
ctest --test-dir $BuildDir --output-on-failure --label-regex $Label
