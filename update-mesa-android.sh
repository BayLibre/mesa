#!/bin/bash
set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MESA_BUILD="${MESA_BUILD:-$SCRIPT_DIR/build-aarch64-linux-android}"
ARCH="${1:-a55}"

# Auto-détection des chemins
AOSP_BASE="${AOSP_BASE:-$(realpath "$SCRIPT_DIR/../../aosp" 2>/dev/null || echo "")}"
VENDOR_MESA="${VENDOR_MESA:-$AOSP_BASE/vendor/amlogic/yukawa/gpu/mesa/$ARCH}"

# Auto-détection du NDK
NDK_PATH="${NDK_PATH:-$(find "$(dirname "$SCRIPT_DIR")" -maxdepth 2 -name "android-ndk-r*" -type d 2>/dev/null | head -1)}"
STRIP="${STRIP:-$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip}"

# Vérifications
if [ ! -d "$VENDOR_MESA" ]; then
    echo "ERREUR: Le répertoire $VENDOR_MESA n'existe pas"
    echo ""
    echo "Usage: $0 [architecture]"
    echo "  Exemple: $0 a55"
    echo ""
    echo "Variables d'environnement:"
    echo "  VENDOR_MESA   - Chemin destination (défaut: auto-détecté)"
    echo "  AOSP_BASE     - Racine AOSP (défaut: ../../aosp)"
    echo "  NDK_PATH      - Chemin NDK (défaut: auto-détecté)"
    echo "  MESA_BUILD    - Répertoire build Mesa (défaut: build-aarch64-linux-android)"
    exit 1
fi

if [ ! -x "$STRIP" ]; then
    echo "ERREUR: llvm-strip non trouvé à $STRIP"
    echo "Définir NDK_PATH ou STRIP manuellement."
    exit 1
fi

VENDOR_LIB64="$VENDOR_MESA/lib64"
VENDOR_HW="$VENDOR_LIB64/hw"
VENDOR_EGL="$VENDOR_LIB64/egl"

echo "=== Mise à jour des bibliothèques Mesa pour Android ==="
echo "Mesa build:    $MESA_BUILD"
echo "Architecture:  $ARCH"
echo "Destination:   $VENDOR_MESA"
echo "Strip tool:    $STRIP"
echo ""

# Créer les répertoires de destination
mkdir -p "$VENDOR_EGL"
mkdir -p "$VENDOR_HW"
mkdir -p "$VENDOR_LIB64/gbm"

# Sauvegarder les anciennes versions
BACKUP_DIR="mesa-backup-$(date +%Y%m%d-%H%M%S)"
echo "Sauvegarde des anciennes bibliothèques dans $BACKUP_DIR..."
mkdir -p "$BACKUP_DIR"
cp -a "$VENDOR_HW/vulkan.mesa.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_EGL/libGLES_mesa.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_EGL/libgallium_dri.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_EGL/libGLESv2.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_EGL/libGLESv1_CM.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_LIB64/libgbm_mesa.so" "$BACKUP_DIR/" 2>/dev/null || true
cp -a "$VENDOR_LIB64/gbm/dri_gbm.so" "$BACKUP_DIR/" 2>/dev/null || true

echo ""
echo "=== Installation des nouvelles bibliothèques ==="

# 1. Vulkan Panfrost -> vulkan.mesa.so
echo "1. Installation de vulkan.mesa.so (Panfrost)..."
cp "$MESA_BUILD/src/panfrost/vulkan/libvulkan_panfrost.so" "$VENDOR_HW/vulkan.mesa.so"
patchelf --set-soname "vulkan.mesa.so" "$VENDOR_HW/vulkan.mesa.so"
$STRIP "$VENDOR_HW/vulkan.mesa.so"

# 2. libEGL -> libGLES_mesa.so (wrapper principal)
echo "2. Installation de libGLES_mesa.so (EGL/GLES wrapper)..."
cp "$MESA_BUILD/src/egl/libEGL.so" "$VENDOR_EGL/libGLES_mesa.so"
patchelf --set-soname "libGLES_mesa.so" "$VENDOR_EGL/libGLES_mesa.so"
$STRIP "$VENDOR_EGL/libGLES_mesa.so"

# 3. libgallium_dri.so (driver Gallium Panfrost)
echo "3. Installation de libgallium_dri.so (Gallium driver)..."
cp "$MESA_BUILD/src/gallium/targets/dri/libgallium_dri.so" "$VENDOR_EGL/"
patchelf --set-soname "libgallium_dri.so" "$VENDOR_EGL/libgallium_dri.so"
$STRIP "$VENDOR_EGL/libgallium_dri.so"

# 4. libGLESv2.so
echo "4. Installation de libGLESv2.so..."
cp "$MESA_BUILD/src/mesa/glapi/es2api/libGLESv2.so" "$VENDOR_EGL/"
patchelf --set-soname "libGLESv2.so" "$VENDOR_EGL/libGLESv2.so"
$STRIP "$VENDOR_EGL/libGLESv2.so"

# 5. libGLESv1_CM.so
echo "5. Installation de libGLESv1_CM.so..."
cp "$MESA_BUILD/src/mesa/glapi/es1api/libGLESv1_CM.so" "$VENDOR_EGL/"
patchelf --set-soname "libGLESv1_CM.so" "$VENDOR_EGL/libGLESv1_CM.so"
$STRIP "$VENDOR_EGL/libGLESv1_CM.so"

# 6. libgbm_mesa.so
echo "6. Installation de libgbm_mesa.so..."
cp "$MESA_BUILD/src/gbm/libgbm_mesa.so" "$VENDOR_LIB64/"
patchelf --set-soname "libgbm_mesa.so" "$VENDOR_LIB64/libgbm_mesa.so"
$STRIP "$VENDOR_LIB64/libgbm_mesa.so"

# 7. dri_gbm.so (backend GBM pour DRI)
echo "7. Installation de dri_gbm.so (GBM backend)..."
cp "$MESA_BUILD/src/gbm/backends/dri/dri_gbm.so" "$VENDOR_LIB64/gbm/"
patchelf --set-soname "dri_gbm.so" "$VENDOR_LIB64/gbm/dri_gbm.so"
$STRIP "$VENDOR_LIB64/gbm/dri_gbm.so"

echo ""
echo "=== Vérification des SONAME ==="
echo "vulkan.mesa.so:"
readelf -d "$VENDOR_HW/vulkan.mesa.so" | grep SONAME

echo "libGLES_mesa.so:"
readelf -d "$VENDOR_EGL/libGLES_mesa.so" | grep SONAME

echo "libgallium_dri.so:"
readelf -d "$VENDOR_EGL/libgallium_dri.so" | grep SONAME

echo ""
echo "=== Tailles des bibliothèques ==="
ls -lh "$VENDOR_HW/vulkan.mesa.so"
ls -lh "$VENDOR_EGL/libGLES_mesa.so"
ls -lh "$VENDOR_EGL/libgallium_dri.so"
ls -lh "$VENDOR_EGL/libGLESv2.so"
ls -lh "$VENDOR_EGL/libGLESv1_CM.so"
ls -lh "$VENDOR_LIB64/libgbm_mesa.so"
ls -lh "$VENDOR_LIB64/gbm/dri_gbm.so"

echo ""
echo "✅ Mise à jour terminée avec succès !"
echo "📁 Destination: $VENDOR_MESA"
echo "📁 Sauvegarde: $BACKUP_DIR"
