#!/usr/bin/env bash
# Build the Boost version used by the Apple Silicon Defcoin Core GUI port.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
BOOST_VERSION="1.84.0"
BOOST_UNDERSCORE="1_84_0"
BOOST_ARCHIVE="boost_${BOOST_UNDERSCORE}.tar.bz2"
BOOST_SHA256="cc4b893acf645c9d4b698e9a0f08ca8846aa5d6c68275c14c3e7949c24109454"
BOOST_URL="https://archives.boost.io/release/${BOOST_VERSION}/source/${BOOST_ARCHIVE}"

WORK_ROOT="${DEFCOIN_DEPS_DIR:-${ROOT_DIR}/.defcoin-build-deps}"
SOURCES_DIR="${WORK_ROOT}/sources"
BUILD_DIR="${WORK_ROOT}/build/boost_${BOOST_UNDERSCORE}"
PREFIX="${DEFCOIN_BOOST_PREFIX:-${WORK_ROOT}/boost-${BOOST_VERSION}-arm64}"

mkdir -p "${SOURCES_DIR}" "${WORK_ROOT}/build"

if [[ ! -f "${SOURCES_DIR}/${BOOST_ARCHIVE}" ]]; then
    echo "Downloading ${BOOST_ARCHIVE}"
    curl -L --fail --output "${SOURCES_DIR}/${BOOST_ARCHIVE}" "${BOOST_URL}"
fi

ACTUAL_SHA256="$(shasum -a 256 "${SOURCES_DIR}/${BOOST_ARCHIVE}" | awk '{print $1}')"
if [[ "${ACTUAL_SHA256}" != "${BOOST_SHA256}" ]]; then
    echo "Checksum mismatch for ${BOOST_ARCHIVE}" >&2
    echo "expected: ${BOOST_SHA256}" >&2
    echo "actual:   ${ACTUAL_SHA256}" >&2
    exit 1
fi

if [[ -d "${PREFIX}/include/boost" && -f "${PREFIX}/lib/libboost_system.a" ]]; then
    echo "Boost ${BOOST_VERSION} already installed at ${PREFIX}"
    exit 0
fi

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
tar -xjf "${SOURCES_DIR}/${BOOST_ARCHIVE}" -C "${WORK_ROOT}/build"

cd "${BUILD_DIR}"
./bootstrap.sh --with-toolset=clang --prefix="${PREFIX}" \
    --with-libraries=atomic,filesystem,system,test,thread

./b2 -j"$(sysctl -n hw.ncpu)" \
    toolset=clang \
    architecture=arm \
    address-model=64 \
    binary-format=mach-o \
    target-os=darwin \
    cxxstd=17 \
    variant=release \
    link=static \
    runtime-link=shared \
    threading=multi \
    cxxflags="-arch arm64" \
    linkflags="-arch arm64" \
    install

echo "Boost ${BOOST_VERSION} installed at ${PREFIX}"
