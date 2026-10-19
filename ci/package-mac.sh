#!/usr/bin/env bash
set -euo pipefail
bash ./package.sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release --target WhirlDTDSPTests WhirlDTRegressionTests -j 3
ctest --test-dir build -C Release --output-on-failure
mkdir -p dist
# Support both the committed stable filename and the versioned local packager.
version="$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' build/CMakeCache.txt)"
package="build/WhirlDT-${version}.pkg"
[[ -f "$package" ]] || package=build/WhirlDT.pkg
cp "$package" dist/installer.pkg
pkgutil --check-signature dist/installer.pkg
xcrun stapler validate dist/installer.pkg
