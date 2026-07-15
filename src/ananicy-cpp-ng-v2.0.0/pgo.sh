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

CURRENT_DIR="$PWD"
BUILD_DIR="$PWD/build/Release"
PGO_DIR="$PWD/pgo-data"
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

echo "==== Creating necessary folders ===="
mkdir -p "${PGO_DIR}"

echo "==== Starting configuration step ===="
## GCC
./configure.sh -t=Release              --pgofolder="${PGO_DIR}" --generate_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
## Clang
#./configure.sh -t=Release --use_clang --pgofolder="${PGO_DIR}" --generate_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
echo "==== Starting build step ===="
./build.sh || die "build.sh failed!"

cd "${BUILD_DIR}"

echo "==== Launching test suite ===="
./src/tests/test-core || die "core test failed!"
./src/tests/test-utility || die "utility test failed!"
echo "==== Launching benchmarks ===="
./benchmarks/ananicy_cpp_benchmarks || die "failed on ananicy-cpp benchmarks!"

cd "${CURRENT_DIR}"

echo "==== Cleanup build folder before rebuilding with generated profile ===="
rm -rf "${BUILD_DIR}"

## Clang
#llvm-profdata merge -output="${PGO_DIR}/default.profdata" "${PGO_DIR}"/*.profraw

echo "==== Starting configuration step with generated profiles ===="
## GCC
./configure.sh -t=Release              --pgofolder="${PGO_DIR}" --use_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
## Clang
#./configure.sh -t=Release --use_clang --pgofolder="${PGO_DIR}" --use_pgo --enable_benchmarks --enable_tests "${@}" || die "configure.sh failed!"
echo "==== Starting build step with generated profiles ===="
./build.sh || die "build.sh failed!"
