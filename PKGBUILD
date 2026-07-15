# Maintainer: Chris McGimpsey-Jones <chrisjones.unixmen@gmail.com>
# Contributor: Antoine Viallon <antoine@lesviallon.fr>

pkgname=ananicy-cpp-ng
_pkgver=2.0.0
pkgver=${_pkgver//-/.}
pkgrel=4
pkgdesc="Modernized, patched x86_64 build of ananicy-cpp."
arch=(x86_64)
url="https://www.freedompublishersunion.net/h-linux.html"
license=('GPL-3.0-or-later')
provides=(ananicy-cpp-ng ananicy-cpp)
conflicts=(ananicy-cpp)
depends=(libfmt.so libspdlog.so systemd libelf zlib libbpf)
makedepends=(cmake ninja clang git nlohmann-json bpf)
optdepends=("ananicy-rules-git: community rules"
            "ananicy-rules: Rules based for ananicy-cpp")
#source=("https://gitlab.com/ananicy-cpp/${pkgname}/-/archive/v${_pkgver}/${pkgname}-v${_pkgver}.tar.gz")
sha512sums=('SKIP')

prepare() {
  cd "${srcdir}/${pkgname}-v1.1.1"

  # [PATCH] Inject missing GCC 16 standard library headers
  sed -i '1i #include <cstring>' src/platform/linux/singleton_process.cpp
  sed -i '1i #include <cstring>' src/utility/argument_parsing/argument.cpp
  sed -i '1i #include <unistd.h>' src/platform/systemd/service.cpp
  sed -i '1i #include <cstdint>' src/platform/linux/backtrace.cpp
  sed -i '1i #include <unistd.h>' src/platform/linux/process.cpp
  sed -i '1i #include <unistd.h>' src/platform/linux/debug.cpp

  # [PATCH] Bypass latency_nice fetch as native glibc struct lacks it
  sed -i 's/info\["latency_nice"\].*/info["latency_nice"] = 0;/g' src/platform/linux/process_info.cpp

  # [PATCH] Namespace colliding sched_attr struct and syscalls
  for file in src/platform/linux/syscalls.h src/platform/linux/priority.cpp src/platform/linux/process_info.cpp; do
    sed -i 's/\bsched_attr\b/ananicy_sched_attr/g' "$file"
    sed -i 's/\bsched_getattr\b/ananicy_sched_getattr/g' "$file"
    sed -i 's/\bsched_setattr\b/ananicy_sched_setattr/g' "$file"
  done

  # [PATCH] Suppress noisy CPM.cmake dev warning
  sed -i 's/message(WARNING "Your project is using an unstable development version/message(DEBUG "/g' cmake/CPM.cmake
}

build() {
  cd "${srcdir}/${pkgname}-v2.0.0"

  cmake -S . -Bbuild \
        -GNinja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DUSE_EXTERNAL_SPDLOG=ON \
        -DUSE_EXTERNAL_JSON=ON \
        -DUSE_EXTERNAL_FMTLIB=ON \
        -DENABLE_SYSTEMD=ON \
        -DUSE_BPF_PROC_IMPL=ON \
        -DBPF_BUILD_LIBBPF=OFF \
        -DVERSION=${_pkgver}

  cmake --build build --target ananicy-cpp-ng
}

package() {
  cd "${srcdir}/${pkgname}-v2.0.0"
  DESTDIR="${pkgdir}" cmake --install build --component Runtime

  install -m755 -d "${pkgdir}/etc/ananicy.d"
}
