#!/bin/bash
set -e

# Couleurs pour les messages
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Détection automatique du répertoire Mesa
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MESA_DIR="${MESA_DIR:-$SCRIPT_DIR}"
BUILD_NATIVE="build-compiler"
BUILD_ANDROID="build-aarch64-linux-android"

echo -e "${GREEN}=== Build Mesa Panfrost pour Android ===${NC}"
echo ""

# Vérification des prérequis
echo -e "${YELLOW}Vérification des prérequis...${NC}"
command -v llvm-config-19 >/dev/null 2>&1 || { echo -e "${RED}ERREUR: llvm-config-19 non trouvé. Installer LLVM 19.${NC}"; exit 1; }
command -v pkg-config >/dev/null 2>&1 || { echo -e "${RED}ERREUR: pkg-config non trouvé.${NC}"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo -e "${RED}ERREUR: ninja non trouvé.${NC}"; exit 1; }

# Vérifier LLVMSPIRVLib
PKG_CONFIG_PATH=/usr/lib/pkgconfig:$PKG_CONFIG_PATH pkg-config --exists LLVMSPIRVLib || {
    echo -e "${RED}ERREUR: LLVMSPIRVLib non trouvé. Compiler SPIRV-LLVM-Translator d'abord.${NC}"
    exit 1
}

SPIRVLIB_VERSION=$(PKG_CONFIG_PATH=/usr/lib/pkgconfig:$PKG_CONFIG_PATH pkg-config --modversion LLVMSPIRVLib)
echo -e "${GREEN}✓${NC} LLVM 19: $(llvm-config-19 --version)"
echo -e "${GREEN}✓${NC} LLVMSPIRVLib: $SPIRVLIB_VERSION"
echo ""

cd "$MESA_DIR"

# Étape 1: Compiler les outils natifs
if [ "$1" != "--skip-native" ]; then
    echo -e "${GREEN}=== Étape 1/3: Compilation des outils natifs ===${NC}"
    echo "Build directory: $BUILD_NATIVE"

    rm -rf "$BUILD_NATIVE"

    PKG_CONFIG_PATH=/usr/lib/pkgconfig:$PKG_CONFIG_PATH \
    LLVM_CONFIG=llvm-config-19 \
    meson setup "$BUILD_NATIVE" \
        -Dprefix=/tmp/mesa-compiler \
        -Dbuildtype=release \
        -Dstrip=true \
        -Dplatforms= \
        -Dgallium-drivers= \
        -Dvulkan-drivers= \
        -Dmesa-clc=enabled \
        -Dinstall-mesa-clc=true \
        -Dtools=panfrost \
        -Dinstall-precomp-compiler=true

    echo -e "${YELLOW}Compilation en cours...${NC}"
    ninja -C "$BUILD_NATIVE"

    echo -e "${YELLOW}Installation...${NC}"
    ninja -C "$BUILD_NATIVE" install

    echo -e "${GREEN}✓ Outils natifs compilés et installés${NC}"
    echo ""
else
    echo -e "${YELLOW}Étape 1 ignorée (--skip-native)${NC}"
    echo ""
fi

# Étape 2: Cross-compilation pour Android
echo -e "${GREEN}=== Étape 2/3: Cross-compilation pour Android ===${NC}"
echo "Build directory: $BUILD_ANDROID"
echo "Target: aarch64-linux-android35"

rm -rf "$BUILD_ANDROID"

# Détecter le fichier de cross-compilation
CROSS_FILE="${CROSS_FILE:-android-aarch64}"
[ -f "$CROSS_FILE" ] || CROSS_FILE="../android-aarch64"

meson setup "$BUILD_ANDROID" \
  --cross-file "$CROSS_FILE" \
  --prefix=/usr/local \
  -Dplatforms=android \
  -Dandroid-stub=false \
  -Dandroid-libbacktrace=disabled \
  -Dandroid-strict=true \
  -Dgallium-drivers=panfrost \
  -Dvulkan-drivers=panfrost \
  -Dgbm=enabled \
  -Degl=enabled \
  -Dgles1=enabled \
  -Dgles2=enabled \
  -Dglx=disabled \
  -Dshared-glapi=enabled \
  -Dplatform-sdk-version=35 \
  -Dmesa-clc=system \
  -Dprecomp-compiler=system \
  -Dspirv-tools=disabled

echo -e "${YELLOW}Compilation en cours...${NC}"
ninja -C "$BUILD_ANDROID"

echo -e "${GREEN}✓ Mesa Android compilé${NC}"
echo ""

# Étape 3: Afficher les bibliothèques générées
echo -e "${GREEN}=== Étape 3/3: Bibliothèques générées ===${NC}"
echo ""
echo "Bibliothèques compilées:"
ls -lh "$BUILD_ANDROID/src/panfrost/vulkan/libvulkan_panfrost.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libvulkan_panfrost.so (Vulkan)"
ls -lh "$BUILD_ANDROID/src/egl/libEGL.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libEGL.so (EGL/GLES wrapper)"
ls -lh "$BUILD_ANDROID/src/gallium/targets/dri/libgallium_dri.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libgallium_dri.so (Gallium Panfrost)"
ls -lh "$BUILD_ANDROID/src/mesa/glapi/es2api/libGLESv2.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libGLESv2.so (OpenGL ES 2/3)"
ls -lh "$BUILD_ANDROID/src/mesa/glapi/es1api/libGLESv1_CM.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libGLESv1_CM.so (OpenGL ES 1)"
ls -lh "$BUILD_ANDROID/src/gbm/libgbm_mesa.so" 2>/dev/null && echo -e "${GREEN}✓${NC} libgbm_mesa.so (GBM)"

echo ""
echo -e "${GREEN}=== Build terminé avec succès ! ===${NC}"
echo ""
echo "Pour déployer dans Android:"
echo -e "  ${YELLOW}./update-mesa-android.sh [architecture]${NC}"
echo ""
echo "Variables d'environnement optionnelles:"
echo "  MESA_DIR      - Répertoire Mesa (défaut: répertoire du script)"
echo "  CROSS_FILE    - Fichier cross-compilation (défaut: android-aarch64)"
echo ""
