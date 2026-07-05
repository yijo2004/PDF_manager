#!/bin/sh
set -eu

repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir="${repo_dir}/build-macos"

if ! command -v cmake >/dev/null 2>&1; then
    echo "CMake is required. Install it with: brew install cmake" >&2
    exit 1
fi

cmake -S "${repo_dir}" -B "${build_dir}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build "${build_dir}" --config Release

echo "Built: ${build_dir}/PDF Manager.app"
