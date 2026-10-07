#!/usr/bin/env bash
set -euo pipefail

packages=(
  cmake ninja-build g++ pkg-config
  libgl1-mesa-dev libegl1-mesa-dev
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev
  libxfixes-dev libxss-dev libxtst-dev
  libwayland-dev libxkbcommon-dev libdrm-dev libgbm-dev
  libasound2-dev libpulse-dev libudev-dev
)
apt_options=(
  -o Acquire::Retries=1
  -o Acquire::http::Timeout=15
  -o Acquire::https::Timeout=15
)

# Standard GitHub Ubuntu runners already carry package indexes. Avoid a full
# update unless installation proves that their cached indexes are unusable.
if sudo timeout 180s apt-get "${apt_options[@]}" install --yes "${packages[@]}"; then
  exit 0
fi

echo '::warning::APT install from cached indexes failed; refreshing from the public Ubuntu mirror.'
sources=/etc/apt/sources.list.d/ubuntu.sources
test -f "$sources"
sudo sed -i \
  -e 's@mirror+file:/etc/apt/apt-mirrors.txt@https://archive.ubuntu.com/ubuntu/@g' \
  -e 's@http://azure.archive.ubuntu.com/ubuntu/@https://archive.ubuntu.com/ubuntu/@g' \
  "$sources"
sudo timeout 300s apt-get "${apt_options[@]}" update
sudo timeout 180s apt-get "${apt_options[@]}" install --yes "${packages[@]}"
