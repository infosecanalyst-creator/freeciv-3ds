#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 xPsycho999 (original Freeciv 3DS port)
# Copyright (C) 2026 the freeciv-3ds authors
set -e

echo "Importing devkitPro GPG keys..."
sudo pacman-key --recv BC26F752D25B92CE272E0F44F7FD5492264BB9D0 --keyserver keyserver.ubuntu.com
sudo pacman-key --lsign BC26F752D25B92CE272E0F44F7FD5492264BB9D0
sudo pacman-key --recv 62C7609ADA219C60 --keyserver keyserver.ubuntu.com
sudo pacman-key --lsign 62C7609ADA219C60

echo "Adding devkitPro repositories to /etc/pacman.conf..."
if ! grep -q "\[dkp-libs\]" /etc/pacman.conf; then
  sudo tee -a /etc/pacman.conf << 'EOF'

[dkp-libs]
Server = https://pkg.devkitpro.org/packages

[dkp-linux]
Server = https://pkg.devkitpro.org/packages/linux/$arch/
EOF
  echo "Repositories added successfully."
else
  echo "Repositories already present in /etc/pacman.conf."
fi

echo "Updating package databases and installing 3ds-dev..."
sudo pacman -Sy --noconfirm 3ds-dev
