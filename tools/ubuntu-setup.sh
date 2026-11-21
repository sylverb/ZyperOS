#!/bin/bash
# What CI's Linux jobs install (.github/workflows/check.yml, jobs check-linux
# and devices), for a fresh Ubuntu 24.04 machine: a container, a VM, a cloud
# workspace's setup step. Then make native, make check with check-linux's
# CHECKS_SKIP, and make check-device-compile run there as on CI. Safe to run
# again: it installs only what is missing.
set -euo pipefail

SUDO=
[ "$(id -u)" -eq 0 ] || SUDO=sudo

# check-linux's packages, the devices job's headers, and the tools the
# GitHub runner image has that a plain Ubuntu 24.04 does not
PKGS="build-essential pkg-config git file zip unzip xz-utils curl ca-certificates
lsof sqlite3 python3 python3-pip
libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev libsqlite3-dev liblzma-dev zlib1g-dev
libasound2-dev libcurl4-openssl-dev libssl-dev
libegl-dev libgles-dev libgbm-dev libdrm-dev"

missing=
for p in $PKGS; do
  s=$(dpkg-query -W -f='${Status}' "$p" 2>/dev/null || true)
  [ "$s" = "install ok installed" ] || missing="$missing $p"
done
if [ -n "$missing" ]; then
  echo "ubuntu-setup: installing$missing"
  export DEBIAN_FRONTEND=noninteractive
  $SUDO apt-get update -qq
  # shellcheck disable=SC2086
  $SUDO apt-get install -y -qq $missing
fi

# Pillow for the image checks (CI: pip install Pillow). Ubuntu's own Python
# refuses a plain pip install (PEP 668), hence the second try.
if ! python3 -c 'import PIL' 2>/dev/null; then
  python3 -m pip install -q Pillow 2>/dev/null ||
    $SUDO python3 -m pip install -q --break-system-packages Pillow
fi

echo "ubuntu-setup: ready ($(python3 --version), Pillow $(python3 -c 'import PIL; print(PIL.__version__)'), $(nproc) cores)"
