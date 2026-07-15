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

if [[ $1 == "--help" ]]; then
cat<<EOF
Usage: configure.sh [options]
Options:
  --help                       Display this information.
  --                           Pass options to the build system.
  --buildtype=, -t=            Specify build type.
  --prefix=                    Specify install prefix.
  --libdir=                    Specify lib directory.
  --path=, -p=                 Specify build path.
  --builddir=                  Specify build directory.
  --use_clang                  Use Clang compiler.
  --use_bpf_proc               Use BPF as a proc listener.
  --enable_systemd             Enable systemd support.
  --enable_relocs              Build AnanicyCpp with relocs.
  --enable_tests               Enable AnanicyCpp tests.
  --enable_fuzz_tests          Enable AnanicyCpp Fuzz tests.
  --enable_benchmarks          Enable AnanicyCpp benchmarks.
  --generate_pgo               Generate PGO profile.
  --use_pgo                    Use generated PGO profile.
  --pgofolder=                 Specify PGO directory.
  --enable_sanitizer_address   Enable ASAN.
  --enable_sanitizer_ub        Enable UBSAN.
  --enable_sanitizer_leak      Enable LEAKSAN.
EOF
exit 0
fi


_buildpath="build"
_prefix="/usr/local"
_libdir="lib"
_buildtype="RelWithDebInfo"
_buildfolder="${_buildpath}/${_buildtype}"
_external_flags=false
_use_clang=false
_use_bpf_proc=false
_enable_systemd=false
_enable_relocs=false
_enable_tests=false
_enable_fuzz_tests=false
_enable_benchmarks=false
_generate_pgo=false
_use_pgo=false
_pgofolder="$PWD/pgo-data"
_sanitizer_address=OFF
_sanitizer_UB=OFF
_sanitizer_leak=OFF
for i in "$@"; do
  case $i in
    -t=*|--buildtype=*)
      _buildtype="${i#*=}"
      _buildfolder="${_buildpath}/${_buildtype}"
      shift # past argument=value
      ;;
    --prefix=*)
      _prefix="${i#*=}"
      shift # past argument=value
      ;;
    --libdir=*)
      _libdir="${i#*=}"
      shift # past argument=value
      ;;
    -p=*|--path=*)
      _buildpath="${i#*=}"
      _buildfolder="${_buildpath}/${_buildtype}"
      shift # past argument=value
      ;;
    --builddir=*)
      _buildfolder="${i#*=}"
      shift # past argument=value
      ;;
    --use_clang)
      _use_clang=true
      shift # past argument=value
      ;;
    --use_bpf_proc)
      _use_bpf_proc=true
      shift # past argument=value
      ;;
    --enable_systemd)
      _enable_systemd=true
      shift # past argument=value
      ;;
    --enable_relocs)
      _enable_relocs=true
      shift # past argument=value
      ;;
    --enable_tests)
      _enable_tests=true
      shift # past argument=value
      ;;
    --enable_fuzz_tests)
      _enable_fuzz_tests=true
      shift # past argument=value
      ;;
    --enable_benchmarks)
      _enable_benchmarks=true
      shift # past argument=value
      ;;
    --generate_pgo)
      _generate_pgo=true
      shift # past argument=value
      ;;
    --use_pgo)
      _use_pgo=true
      shift # past argument=value
      ;;
    --pgofolder=*)
      _pgofolder="${i#*=}"
      shift # past argument=value
      ;;
    --enable_sanitizer_address)
      _sanitizer_address=ON
      shift # past argument=value
      ;;
    --enable_sanitizer_ub)
      _sanitizer_UB=ON
      shift # past argument=value
      ;;
    --enable_sanitizer_leak)
      _sanitizer_leak=ON
      shift # past argument=value
      ;;
    --)
      _external_flags=true
      shift # past argument=value
      ;;
    *)
      # unknown option
      if [ "${_external_flags}" = false ]; then
          echo "Unknown option [$i]"
          exit 255
      fi
      # option to be passed to the build system
      ;;
  esac
done

if ${_use_clang}; then
  export AR=llvm-ar
  export CC=clang
  export CXX=clang++
  export NM=llvm-nm
  export RANLIB=llvm-ranlib
fi

if ${_use_bpf_proc}; then
  _configure_flags+=('-DUSE_BPF_PROC_IMPL=ON')
fi

if ${_enable_systemd}; then
  _configure_flags+=('-DENABLE_SYSTEMD=ON')
fi

if ${_enable_relocs}; then
  _configure_flags+=('-DBUILD_WITH_RELOCS=ON')
fi

if ${_enable_tests}; then
  _configure_flags+=('-DENABLE_ANANICY_TESTS=ON')
fi

if ${_enable_fuzz_tests}; then
  _configure_flags+=('-DENABLE_ANANICY_FUZZ_TESTS=ON')
fi

if ${_enable_benchmarks}; then
  _configure_flags+=('-DENABLE_ANANICY_BENCHMARKS=ON')
fi

if ${_generate_pgo}; then
  _configure_flags+=('-DENABLE_PGO_BUILD=instrument')
fi
if ${_use_pgo}; then
  _configure_flags+=('-DENABLE_PGO_BUILD=use')
fi

if command -v ninja &> /dev/null; then
  _configure_flags+=('-GNinja')
fi

if command -v mold &> /dev/null; then
  _configure_flags+=('-DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=mold"')
fi

if ${_external_flags}; then
  _configure_flags+=("${@}")
fi

cmake -S . -B ${_buildfolder} \
    -DENABLE_SANITIZER_ADDRESS=${_sanitizer_address} \
    -DENABLE_SANITIZER_UNDEFINED_BEHAVIOR=${_sanitizer_UB} \
    -DENABLE_SANITIZER_LEAK=${_sanitizer_leak} \
    -DCMAKE_BUILD_TYPE=${_buildtype} \
    -DCMAKE_INSTALL_PREFIX=${_prefix} \
    -DCMAKE_INSTALL_LIBDIR=${_libdir} \
    -DPROFILES_PGO_DIR=${_pgofolder} \
    "${_configure_flags[@]}"


cat > build.sh <<EOF
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

cd "\`dirname "\$0"\`"

cmake --build ${_buildfolder} --parallel $(nproc)
EOF

chmod +x ./build.sh
