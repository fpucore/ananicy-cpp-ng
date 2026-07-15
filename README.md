![Ananicy Cpp logo](assets/ananicy_logo.png)

Modernized, patched x86_64 build of ananicy-cpp.

# Ananicy C++ NG (Next Generation)

**Ananicy C++ NG (Next Generation)** is a modernized, heavily patched fork (v2.0.0+) designed to compile and run seamlessly on the GNU Operating System / H-Linux environment, and x86_64 Linux environments, specifically addressing toolchain strictness in GCC 16+ and native header collisions in modern glibc.

Ananicy C++ NG is a complete drop-in replacement for Ananicy and Ananicy Cpp.

It remains fully compatible with pre-made community rules.

---

## What's new?

This fork introduces critical compatibility patches over the original `ananicy-cpp` source code to support modern build toolchains:
* **Modern Compiler Support:** Explicitly patched to satisfy `GCC` 16+ standard library requirements (`<cstring>`, `<unistd.h>`, `<cstdint>`).
* **glibc Namespace Isolation:** The custom `sched_attr` structures and `sched_getattr` syscalls have been safely namespaced (`ananicy_sched_attr`) to prevent build failures when colliding with modern native `glibc` kernel headers.
* **eBPF Submodule Stability:** Surgical `CMake` (re)targeting to ensure the internal `eBPF` submodules (re)link correctly under new *-ng calls without breaking the internals.
* **Streamlined Build Process:** Deliberate suppression of `CPM.cmake` development warnings for a clean, noise-free configuration and compilation procedure.

---

## What works?

- [X] Rule and configuration loading
- [X] Process detection and scanning
- [X] Renicing
- [X] Changing CPU scheduler
- [X] Changing IO class and IO nice
- [X] Setting OOM score
- [X] Change default configuration file and rules directory using environment variables (`ANANICY_CPP_{CONF,CONFDIR}`)
- [X] Cgroups
  - [ ] ~~V1~~
  - [X] ~~V2 (_Limited support_)~~ Native systemd DBus integration (Cgroups V2 exclusive)
- [X] Autogroup
- [X] Improved systemd integration
  - [X] Hardened unit file
  - [X] Working sd_notify
- [X] Continued compatibility with legacy Ananicy and Ananicy Cpp
- [X] Modern build toolchains (GCC 16+) compatibility
- [X] Modern glibc compatibility
- [X] Corrected CMake targeting for eBPF submodule
- [X] Streamlined configuration and compilation

---

## Note on Cgroups V2 and systemd

Unlike the original Ananicy Cpp, the NG fork handles Cgroups exclusively through native `sd-bus` systemd DBus calls (`StartTransientUnit` and `SetUnitProperties`) rather than fragile, manual filesystem manipulation.

Because systemd version 258 officially dropped all support for Cgroups V1, its support has been permanently purged from the NG codebase.

Your system must be running a unified Cgroups V2 hierarchy, which is now the modern Linux standard.

You no longer need to worry about filesystem permissions for `/sys/fs/cgroup/`; the daemon will automatically instruct systemd to spawn dynamic `.scope` units for your processes and nest them under the appropriate `.slice`.

---

## How Ananicy C++ NG compares to its predecessor(s)

The following table summarizes the key differences between the original Ananicy, Ananicy Cpp, and this actively maintained fork.

## Feature Comparison

| Feature                      | ananicy | ananicy-cpp | ananicy-cpp-ng |
| ---------------------------- | :-----: | :---------: | :------------: |
| Modern GCC (16+) support     |    ❌    |      ❌      |        ✅       |
| Modern glibc compatibility   |    ❌    |      ❌      |        ✅       |
| Native Cgroups V2            |    ❌    |  ⚠️ Limited |        ✅       |
| systemd DBus integration     |    ❌    |  ⚠️ Partial |        ✅       |
| systemd v258+ compatible     |    ❌    |      ❌      |        ✅       |
| Modern eBPF build fixes      |    ❌    |      ❌      |        ✅       |
| Community rule compatibility |    ✅    |      ✅      |        ✅       |
| Active maintenance           |    ❌    |      ⚠️     |        ✅       |

---

## Installation

#### Dependencies

This package is intended for deep integration with the GNU Operating System / H-Linux environment, and the Blackbox-hwm workspace.

- GNU Operating System / H-Linux
- H-Linux Standard Library
- goto (Optional, but recommended)
- H-Linux Human Command Layer
- CMake
- fmtlib
- spdlog
- nlohmann_json
- C++ compiler (g++ or clang++) (Fully tested against g++ 16.1.1)
- systemd / libsystemd
- _Optional (required for bpf implementation)_:
    - bpf
    - libbpf
    - libelf
    - clang

#### Build & Install

To build and install the `ananicy-cpp-ng` package:

```bash
git clone https://www.github.com/fpucore/ananicy-cpp-ng.git
cd ananicy-cpp-ng
./INSTALL.sh
```

---

## License

Licensed under GPLv3 or later. See https://www.gnu.org/licenses/gpl-3.0.html

---

## Maintainer

Chris McGimpsey-Jones (2026)

chrisjones.unixmen@gmail.com

---

## Contributor

Antoine Viallon (2021-2026)

antoine@lesviallon.fr
