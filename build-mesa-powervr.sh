#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MESA_DIR="$SCRIPT_DIR"
BUILD_NATIVE="build-compiler"
BUILD_ANDROID="build-riscv64-linux-android"

echo -e "${GREEN}=== Build Mesa PowerVR pour Android riscv64 ===${NC}"
echo ""

# Vérification des prérequis
echo -e "${YELLOW}Vérification des prérequis...${NC}"
command -v meson >/dev/null 2>&1 || { echo -e "${RED}ERREUR: meson non trouvé.${NC}"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo -e "${RED}ERREUR: ninja non trouvé.${NC}"; exit 1; }

cd "$MESA_DIR"

# Add native tools to PATH for cross-compilation step
export PATH=/tmp/mesa-compiler/bin:$PATH

# Étape 1: Compiler les outils natifs (mesa-clc + pco_clc compilers)
if [ "$1" != "--skip-native" ]; then
    echo -e "${GREEN}=== Étape 1/3: Compilation des outils natifs ===${NC}"

    rm -rf "$BUILD_NATIVE"

    meson setup "$BUILD_NATIVE" \
        -Dprefix=/tmp/mesa-compiler \
        -Dbuildtype=release \
        -Dstrip=true \
        -Dplatforms= \
        -Dgallium-drivers= \
        -Dvulkan-drivers=imagination \
        -Dimagination-srv=true \
        -Dmesa-clc=enabled \
        -Dinstall-mesa-clc=true \
        -Dprecomp-compiler=enabled \
        -Dinstall-precomp-compiler=true

    ninja -C "$BUILD_NATIVE"
    ninja -C "$BUILD_NATIVE" install
    echo -e "${GREEN}✓ Outils natifs compilés${NC}"
else
    echo -e "${YELLOW}Étape 1 ignorée (--skip-native)${NC}"
fi

# Étape 2: Cross-compilation pour Android riscv64
echo -e "${GREEN}=== Étape 2/3: Cross-compilation pour Android riscv64 ===${NC}"

rm -rf "$BUILD_ANDROID"

meson setup "$BUILD_ANDROID" \
  --cross-file "$SCRIPT_DIR/android-riscv64" \
  --prefix=/usr/local \
  -Dplatforms=android \
  -Dandroid-stub=false \
  -Dandroid-libbacktrace=disabled \
  -Dandroid-strict=true \
  -Dgallium-drivers=zink \
  -Dvulkan-drivers=imagination \
  -Dgbm=enabled \
  -Degl=enabled \
  -Dgles1=enabled \
  -Dgles2=enabled \
  -Dglx=disabled \
  -Dshared-glapi=enabled \
  -Dplatform-sdk-version=35 \
  -Dmesa-clc=system \
  -Dprecomp-compiler=system \
  -Dspirv-tools=disabled \
  -Dimagination-srv=true \
  -Dallow-fallback-for=libdrm,perfetto

ninja -C "$BUILD_ANDROID"
echo -e "${GREEN}✓ Mesa Android riscv64 compilé${NC}"

# Étape 3: Afficher les libs
echo -e "${GREEN}=== Étape 3/3: Bibliothèques générées ===${NC}"
echo ""
find "$BUILD_ANDROID" -name "*.so" -type f | while read f; do
    echo -e "${GREEN}✓${NC} $(basename $f) ($(du -h $f | cut -f1))"
done

echo ""
echo -e "${GREEN}=== Build terminé ! ===${NC}"
echo ""
echo "Pour déployer: copier les .so dans vendor/lib64/hw/ et vendor/lib64/"
