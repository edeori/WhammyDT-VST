$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
cmake -S . -B build/ci -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
cmake --build build/ci --config Release --target WhirlDT_VST3 WhirlDT_Standalone WhirlDTDSPTests WhirlDTRegressionTests --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
ctest --test-dir build/ci -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Regression tests failed' }
$versionLine = Get-Content build/ci/CMakeCache.txt | Where-Object { $_ -match '^CMAKE_PROJECT_VERSION:STATIC=' }
$version = ($versionLine -split '=', 2)[1]
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'Missing project version' }
$compiler = 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe'
if (!(Test-Path $compiler)) { throw 'Inno Setup 6 is required' }
New-Item -ItemType Directory -Force dist | Out-Null
& $compiler "/DAppVersion=$version" "/DRepoRoot=$PWD" "/O$PWD/dist" Installer/WhirlDT.iss
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup failed' }
if (!(Test-Path dist/installer.exe)) { throw 'Missing installer' }
