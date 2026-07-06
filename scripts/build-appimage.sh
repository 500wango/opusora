#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-appimage"
APPDIR="${ROOT_DIR}/AppDir"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" -j"$(nproc)"
rm -rf "${APPDIR}"
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

if ! command -v linuxdeploy-x86_64.AppImage >/dev/null 2>&1; then
    echo "linuxdeploy-x86_64.AppImage is required on PATH" >&2
    exit 1
fi

linuxdeploy-x86_64.AppImage \
    --appdir "${APPDIR}" \
    --desktop-file "${ROOT_DIR}/packaging/linux/opusora.desktop" \
    --icon-file "${ROOT_DIR}/packaging/linux/icons/hicolor/scalable/apps/opusora.svg" \
    --output appimage
