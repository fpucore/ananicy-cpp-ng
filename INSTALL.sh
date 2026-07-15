#!/usr/bin/env bash
source /usr/local/lib/h-linux-env.sh

#================================================================================
#Ananicy C++ NG
#Copyright (C) 2026  Chris McGimpsey-Jones
#
#This program is free software: you can redistribute it and/or modify
#it under the terms of the GNU General Public License as published by
#the Free Software Foundation, either version 3 of the License, or
#(at your option) any later version.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#
#You should have received a copy of the GNU General Public License
#along with this program.  If not, see <https://www.gnu.org/licenses/>.
#================================================================================

set -e

RED='\033[0;31m'
BRED='\033[1;31m'
WHITE='\033[1;37m'
NC='\033[0m'

say -e "${BRED}======================================================${NC}"
say -e "${BRED}   ANANICY C++ NG  ::  SYSTEM INSTALLATION ROUTINE    ${NC}"
say -e "${BRED}======================================================${NC}"
say ""

# Verify build env
say -e "${WHITE}[*] Verifying build environment...${NC}"
if [[ ! -f "PKGBUILD" ]]; then
    say -e "${RED}[!] ERROR: PKGBUILD not found in the current directory.${NC}"
    say -e "${RED}    Execution halted. Please run from the package root.${NC}"
    exit 1
fi
say -e "${WHITE}    -> PKGBUILD located.${NC}"

# Pkg compile & install
say ""
say -e "${WHITE}[*] Engaging makepkg (-seC) pipeline...${NC}"
say -e "${WHITE}    Compiling from current source tree.${NC}"
say -e "${WHITE}    (You may be prompted for your elevate password to install dependencies/package)${NC}"
sleep 1

# Exec sync of deps & build
    # Clean old build dirs
    deletedir-dev -f src/ananicy-cpp-ng-v2.0.0/build/
    # Clean old build packages
    delete-dev -f *.zst
    # then build
makepkg --skipchecksums --skippgpcheck -seC

# Exec install
nlp -U *.zst

# Exec systemd integration
say ""
say -e "${WHITE}[*] Integrating daemon with systemd...${NC}"
elevate systemctl daemon-reload
elevate systemctl enable --now ananicy-cpp-ng.service

# Verify status
say ""
say -e "${WHITE}[*] Verifying daemon status...${NC}"
sleep 2 # Patience... allow systemd a moment to spin up the worker threads

if systemctl is-active --quiet ananicy-cpp-ng.service; then
    say -e "${BRED}[+] SUCCESS: ananicy-cpp-ng is active and routing processes.${NC}"
else
    say -e "${RED}[!] WARNING: Service installed but not active.${NC}"
    say -e "${RED}    Check logs via: systemctl status ananicy-cpp-ng.service${NC}"
fi

say ""
say -e "${BRED}======================================================${NC}"
say -e "${WHITE}                  DEPLOYMENT COMPLETE                   ${NC}"
say -e "${BRED}======================================================${NC}"
