#!/bin/bash
# Copyright (C) 2022-2023 Vladislav Nepogodin
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

set -e

cd "`dirname "$0"`"

export LC_MESSAGES=C
export LANG=C

disable_colors(){
    unset ALL_OFF BOLD BLUE GREEN RED YELLOW
}

enable_colors(){
    # prefer terminal safe colored and bold text when tput is supported
    if tput setaf 0 &>/dev/null; then
        ALL_OFF="$(tput sgr0)"
        BOLD="$(tput bold)"
        RED="${BOLD}$(tput setaf 1)"
        GREEN="${BOLD}$(tput setaf 2)"
        YELLOW="${BOLD}$(tput setaf 3)"
        BLUE="${BOLD}$(tput setaf 4)"
    else
        ALL_OFF="\e[0m"
        BOLD="\e[1m"
        RED="${BOLD}\e[31m"
        GREEN="${BOLD}\e[32m"
        YELLOW="${BOLD}\e[33m"
        BLUE="${BOLD}\e[34m"
    fi
    readonly ALL_OFF BOLD BLUE GREEN RED YELLOW
}

if [[ -t 2 ]]; then
    enable_colors
else
    disable_colors
fi

error() {
    local mesg=$1; shift
    printf "${RED}==> ERROR:${ALL_OFF}${BOLD} ${mesg}${ALL_OFF}\n" "$@" >&2
}

die() {
    (( $# )) && error "$@"
    exit 255
}

instrument_binary() {
    local original_binary="$1"
    local instrumentation_file="$2"
    local output_file="$3"

    echo "==== Instrumenting ${original_binary} ===="
    LD_PRELOAD=/usr/lib/libjemalloc.so llvm-bolt         \
        --instrument                                     \
        --instrumentation-file-append-pid                \
        --instrumentation-file="${instrumentation_file}" \
        "${original_binary}"                             \
        -o "${output_file}" || die "Could not create instrumented binary"

    # Backup original file
    cp "${original_binary}" "${output_file}.org"
    cp "${original_binary}" "${original_binary}.org"

    echo "==== Done instrumenting ${original_binary} ===="
}

optimize_binary() {
    local original_binary="$1"
    local merged_profile="$2"
    local output_file="$3"

    echo "==== Optimizing ${original_binary} ===="
    LD_PRELOAD=/usr/lib/libjemalloc.so llvm-bolt  "${original_binary}" \
        --data "${merged_profile}" \
        -o "${output_file}" \
        -reorder-blocks=ext-tsp \
        -reorder-functions=hfsort+ \
        -split-functions \
        -split-all-cold \
        -split-eh \
        -dyno-stats \
        -icf=1 \
        -use-gnu-stack \
        -plt=hot || die "Could not optimize binary"

    echo "==== Done optimizing ${original_binary} ===="
}

merge_instrumented_data() {
    local instrumentation_folder="$1"
    local merged_profile="$2"

    echo "==== Merging generated profiles ===="
    LD_PRELOAD=/usr/lib/libjemalloc.so merge-fdata  \
        "${instrumentation_folder}"/*.fdata > "${merged_profile}" || die "Could not merge fdate"

    # Remove merged instrumented data
    rm -rf "${instrumentation_folder}"/*.fdata

    echo "==== Done merging profiles ===="
}

CURRENT_DIR="$PWD"
BUILD_DIR="$PWD/build/Release"
PGO_DIR="$PWD/pgo-data"
BOLT_DATA_DIR="$PWD/bolt-data"
BOLT_BINARY_DIR="$PWD/bolt-binary"
SAVED_CXXFLAGS="${CXXFLAGS}"

for i in "$@"; do
  case $i in
    --builddir=*)
      BUILD_DIR="${i#*=}"
      ;;
    *)
      # unknown option
      ;;
  esac
done

echo "==== Cleanup folders ===="
rm -rf "${BUILD_DIR}"
rm -rf "${PGO_DIR}"
rm -rf "${BOLT_DATA_DIR}"
rm -rf "${BOLT_BINARY_DIR}"

echo "==== Creating necessary folders ===="
mkdir -p "${PGO_DIR}"
mkdir -p "${BOLT_DATA_DIR}"
mkdir -p "${BOLT_BINARY_DIR}"

echo "==== Starting configuration step ===="
## GCC
#./configure.sh -t=Release              --pgofolder="${PGO_DIR}" --generate_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
## Clang
./configure.sh -t=Release --use_clang --pgofolder="${PGO_DIR}" --generate_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
echo "==== Starting build step ===="
./build.sh || die "build.sh failed!"

cd "${BUILD_DIR}"

echo "==== Launching test suite ===="
./src/tests/test-core || die "core test failed!"
./src/tests/test-utility || die "utility test failed!"
echo "==== Launching benchmarks ===="
./benchmarks/ananicy_cpp_ng_benchmarks || die "failed on ananicy-cpp-ng benchmarks!"

cd "${CURRENT_DIR}"

echo "==== Cleanup build folder before rebuilding with generated profile ===="
rm -rf "${BUILD_DIR}"

## Clang
llvm-profdata merge -output="${PGO_DIR}/default.profdata" "${PGO_DIR}"/*.profraw
# Cleanup
rm -f "${PGO_DIR}"/*.profraw

echo "==== Starting configuration step with generated profiles ===="
## GCC
#./configure.sh -t=Release              --pgofolder="${PGO_DIR}" --use_pgo --enable_relocs --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
## Clang
./configure.sh -t=Release --use_clang --pgofolder="${PGO_DIR}" --use_pgo --enable_relocs --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
echo "==== Starting build step with generated profiles ===="
./build.sh || die "build.sh failed!"

echo "==== Start instrumenting ===="
instrument_binary "${BUILD_DIR}/src/tests/test-core" "${BOLT_DATA_DIR}/test-core.fdata" "${BOLT_BINARY_DIR}/test-core"
instrument_binary "${BUILD_DIR}/src/tests/test-utility" "${BOLT_DATA_DIR}/test-utility.fdata" "${BOLT_BINARY_DIR}/test-utility"
instrument_binary "${BUILD_DIR}/benchmarks/ananicy_cpp_ng_benchmarks" "${BOLT_DATA_DIR}/ananicy_cpp_ng_benchmarks.fdata" "${BOLT_BINARY_DIR}/ananicy_cpp_ng_benchmarks"

echo "==== Launching test suite ===="
"${BOLT_BINARY_DIR}"/test-core || die "core test failed!"
"${BOLT_BINARY_DIR}"/test-utility || die "utility test failed!"
echo "==== Launching benchmarks ===="
"${BOLT_BINARY_DIR}"/ananicy_cpp_ng_benchmarks || die "failed on ananicy-cpp-ng benchmarks!"

echo "==== Finished instrumenting ===="

cd "${CURRENT_DIR}"

MERGED_PROFILE="${RANDOM}-combined.fdata"
merge_instrumented_data "${BOLT_DATA_DIR}" "${MERGED_PROFILE}"

echo "==== Start optimizing binary with merged profile ===="

optimize_binary "${BUILD_DIR}/ananicy-cpp-ng" "${MERGED_PROFILE}" "${BOLT_BINARY_DIR}/ananicy-cpp-ng.bolt"

echo "==== Finished optimizing ===="

echo "The final optimized binary is '${BOLT_BINARY_DIR}/ananicy-cpp-ng.bolt'"
