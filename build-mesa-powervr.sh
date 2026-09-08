#!/bin/bash
set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MESA_DIR="$SCRIPT_DIR"
AOSP_DIR="$(dirname "$SCRIPT_DIR")/aosp"
BUILD_NATIVE="build-compiler"
BUILD_ANDROID="build-riscv64-linux-android"
SPIRV_HOST_PREFIX="$SCRIPT_DIR/spirv-tools-host"
MESA_COMPILER_PREFIX="/tmp/mesa-compiler"
SKIP_NATIVE=0
BOARD="k1"

usage() {
    cat <<EOF
Usage: $(basename "$0") [--aosp=<path>] [--board=<k1|a210>] [--skip-native]

  --aosp=<path>  AOSP tree to build against and deploy into.
                 Default: $(dirname "$SCRIPT_DIR")/aosp
  --board=<b>    Target board: k1 (SpaceMit K1/X60, default) or a210
                 (Zhihe A210 EVB, Imagination PowerVR Rogue).
  --skip-native  Reuse the native tools already installed in
                 $MESA_COMPILER_PREFIX instead of rebuilding them.
  -h, --help     Affiche cette aide.
EOF
}

for arg in "$@"; do
    case "$arg" in
        --aosp=*) AOSP_DIR="${arg#*=}" ;;
        --board=*) BOARD="${arg#*=}" ;;
        --skip-native) SKIP_NATIVE=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo -e "${RED}ERREUR: option inconnue: $arg${NC}" >&2; usage >&2; exit 1 ;;
    esac
done

# Both the SPIRV-Tools sources and the Mesa prebuilt destination live in the
# same tree, so an --aosp override has to apply to both or the blobs get built
# against one tree and installed into another.
[ -d "$AOSP_DIR" ] || { echo -e "${RED}ERREUR: arbre AOSP introuvable: $AOSP_DIR${NC}" >&2; exit 1; }
AOSP_DIR="$(cd "$AOSP_DIR" && pwd)"

case "$BOARD" in
    k1)
        CROSS_FILE="$SCRIPT_DIR/android-riscv64"
        DEVICE_MESA="$AOSP_DIR/device/spacemit/k1/mesa/lib64"
        LUNCH_TARGET="aosp_bananapi_f3-trunk_staging-userdebug"
        ;;
    a210)
        CROSS_FILE="$SCRIPT_DIR/android-riscv64-a210"
        DEVICE_MESA="$AOSP_DIR/device/alibaba/a210/mesa/lib64"
        LUNCH_TARGET="aosp_a210_evb-trunk_staging-userdebug"
        ;;
    *)
        echo -e "${RED}ERREUR: board inconnu: $BOARD (attendu: k1, a210)${NC}" >&2
        exit 1
        ;;
esac

echo -e "${GREEN}=== Build Mesa PowerVR pour Android riscv64 ($BOARD) ===${NC}"
echo ""

# ---------------------------------------------------------------------------
# Vérification des prérequis de base
# ---------------------------------------------------------------------------
echo -e "${YELLOW}Vérification des prérequis...${NC}"
for tool in meson ninja cmake; do
    command -v "$tool" >/dev/null 2>&1 || { echo -e "${RED}ERREUR: $tool non trouvé.${NC}"; exit 1; }
done

# ---------------------------------------------------------------------------
# Détection de LLVM >= 15
# ---------------------------------------------------------------------------
detect_llvm() {
    for ver in 20 19 18 17 16 15; do
        if command -v "llvm-config-$ver" >/dev/null 2>&1; then
            echo "llvm-config-$ver"
            return 0
        fi
    done
    # fallback: llvm-config par défaut, vérifier version
    if command -v llvm-config >/dev/null 2>&1; then
        local v
        v=$(llvm-config --version | cut -d. -f1)
        if [ "$v" -ge 15 ] 2>/dev/null; then
            echo "llvm-config"
            return 0
        fi
    fi
    echo -e "${RED}ERREUR: llvm-config >= 15 requis. Installer: sudo apt install llvm-15-dev${NC}" >&2
    exit 1
}
LLVM_CONFIG=$(detect_llvm)
LLVM_VER=$($LLVM_CONFIG --version)
echo -e "${GREEN}✓ LLVM $LLVM_VER ($LLVM_CONFIG)${NC}"
export LLVM_CONFIG

# ---------------------------------------------------------------------------
# SPIRV-Tools : vérifier version système, builder depuis AOSP si trop vieux
# ---------------------------------------------------------------------------
SPIRV_MIN_VERSION="2024.1"

spirv_version_ok() {
    local v
    v=$(pkg-config --modversion SPIRV-Tools 2>/dev/null) || return 1
    python3 -c "
v='$v'; req='$SPIRV_MIN_VERSION'
def parse(s):
    p = s.split('.')
    year = int(p[0]); sub = int(p[1]) if len(p) > 1 else 0
    return (year, sub)
exit(0 if parse(v) >= parse(req) else 1)
" 2>/dev/null
}

setup_spirv_tools() {
    if spirv_version_ok; then
        # Vérifier que les libs C++ (opt/link) sont aussi présentes en système
        if pkg-config --exists SPIRV-Tools-opt 2>/dev/null || \
           [ -f "$(pkg-config --variable=libdir SPIRV-Tools 2>/dev/null)/libSPIRV-Tools-opt.a" ] || \
           [ -f "$(pkg-config --variable=libdir SPIRV-Tools 2>/dev/null)/libSPIRV-Tools-opt.so" ]; then
            echo -e "${GREEN}✓ SPIRV-Tools $(pkg-config --modversion SPIRV-Tools) (système, avec opt/link)${NC}"
            return 0
        fi
        # Système a SPIRV-Tools mais pas opt/link → besoin de builder quand même
        echo -e "${YELLOW}SPIRV-Tools système présent mais libs C++ (opt/link) manquantes — build local...${NC}"
    fi

    # Déjà buildé localement et valide (avec opt/link) ?
    if [ -f "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools.pc" ] && \
       [ -f "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools-opt.a" ] && \
       [ -f "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools-link.a" ]; then
        local cached_ver
        cached_ver=$(grep '^Version' "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools.pc" | awk '{print $2}')
        export PKG_CONFIG_PATH="$SPIRV_HOST_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
        if spirv_version_ok; then
            echo -e "${GREEN}✓ SPIRV-Tools $cached_ver (cache local, avec opt/link)${NC}"
            return 0
        fi
    fi

    local sys_ver
    sys_ver=$(pkg-config --modversion SPIRV-Tools 2>/dev/null || echo "absent")
    echo -e "${YELLOW}SPIRV-Tools système ($sys_ver) trop vieux ou incomplet (besoin >= $SPIRV_MIN_VERSION + opt/link) — build depuis AOSP...${NC}"

    local spirv_src="$AOSP_DIR/external/SPIRV-Tools"
    local spirv_headers="$AOSP_DIR/external/SPIRV-Headers"
    [ -f "$spirv_src/CMakeLists.txt" ] || { echo -e "${RED}ERREUR: $spirv_src introuvable${NC}"; exit 1; }

    local build_dir
    build_dir=$(mktemp -d)

    # BUILD_SHARED_LIBS=OFF pour que SPIRV-Tools-opt et SPIRV-Tools-link
    # soient compilés en .a (libs statiques C++) — mesa_clc en a besoin pour
    # spvtools::Optimizer et spvtools::Link (clc_helpers.cpp).
    # SPIRV-Tools-shared reste toujours une .so (target explicitement SHARED).
    cmake \
        -S "$spirv_src" \
        -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSPIRV-Headers_SOURCE_DIR="$spirv_headers" \
        -DSPIRV_SKIP_TESTS=ON \
        -DSPIRV_SKIP_EXECUTABLES=ON \
        -DSPIRV_WERROR=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -DSKIP_SPIRV_TOOLS_INSTALL=ON

    # Compiler : shared C-API + static C++ optimizer + static C++ linker
    cmake --build "$build_dir" \
          --target SPIRV-Tools-shared \
          --target SPIRV-Tools-opt \
          --target SPIRV-Tools-link \
          -j"$(nproc)"

    # Extraire la version depuis CHANGES
    local ver
    ver=$(grep -m1 '^v[0-9]' "$spirv_src/CHANGES" | awk '{print $1}' | tr -d 'v')

    mkdir -p "$SPIRV_HOST_PREFIX/lib/pkgconfig"

    # Shared C-API (.so)
    install -m755 "$build_dir/source/libSPIRV-Tools-shared.so" \
                  "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools-shared.so"

    # Static C++ libs pour mesa_clc (.a)
    install -m644 "$build_dir/source/opt/libSPIRV-Tools-opt.a" \
                  "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools-opt.a"
    install -m644 "$build_dir/source/link/libSPIRV-Tools-link.a" \
                  "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools-link.a"
    # libSPIRV-Tools.a (nécessaire comme dépendance de opt/link)
    install -m644 "$build_dir/source/libSPIRV-Tools.a" \
                  "$SPIRV_HOST_PREFIX/lib/libSPIRV-Tools.a"

    # Copier les headers publics
    find "$spirv_src/include" \( -name "*.h" -o -name "*.hpp" \) | while read h; do
        install -Dm644 "$h" "$SPIRV_HOST_PREFIX/${h#$spirv_src/}"
    done

    # pkgconfig principal : inclut opt + link pour le linkage de mesa_clc
    cat > "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools.pc" <<EOF
prefix=$SPIRV_HOST_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: SPIRV-Tools
Description: Tools for SPIR-V
Version: $ver
Libs: -L\${libdir} -lSPIRV-Tools-shared -lSPIRV-Tools-opt -lSPIRV-Tools-link -lSPIRV-Tools
Cflags: -I\${includedir}
EOF

    # Alias SPIRV-Tools-shared.pc (attendu par Mesa pour -Dspirv-tools=enabled)
    \cp -f "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools.pc" \
           "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools-shared.pc"

    # pkgconfig SPIRV-Tools-opt séparé (Mesa peut le chercher directement)
    cat > "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools-opt.pc" <<EOF
prefix=$SPIRV_HOST_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: SPIRV-Tools-opt
Description: SPIR-V optimizer
Version: $ver
Requires: SPIRV-Tools
Libs: -L\${libdir} -lSPIRV-Tools-opt -lSPIRV-Tools
Cflags: -I\${includedir}
EOF

    # pkgconfig SPIRV-Tools-link
    cat > "$SPIRV_HOST_PREFIX/lib/pkgconfig/SPIRV-Tools-link.pc" <<EOF
prefix=$SPIRV_HOST_PREFIX
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: SPIRV-Tools-link
Description: SPIR-V linker
Version: $ver
Requires: SPIRV-Tools-opt
Libs: -L\${libdir} -lSPIRV-Tools-link -lSPIRV-Tools-opt -lSPIRV-Tools
Cflags: -I\${includedir}
EOF

    rm -rf "$build_dir"

    export PKG_CONFIG_PATH="$SPIRV_HOST_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
    echo -e "${GREEN}✓ SPIRV-Tools $ver (buildé depuis AOSP — shared + opt + link)${NC}"
}

# Ajouter le préfixe local au PKG_CONFIG_PATH si présent
[ -d "$SPIRV_HOST_PREFIX/lib/pkgconfig" ] && \
    export PKG_CONFIG_PATH="$SPIRV_HOST_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"

setup_spirv_tools

# Re-exporter après install éventuelle
[ -d "$SPIRV_HOST_PREFIX/lib/pkgconfig" ] && \
    export PKG_CONFIG_PATH="$SPIRV_HOST_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"

# ---------------------------------------------------------------------------
# Étape 1: Compiler les outils natifs (mesa_clc + pco_clc)
# ---------------------------------------------------------------------------
export PATH="$MESA_COMPILER_PREFIX/bin:$PATH"
# mesa_clc est lié dynamiquement à libSPIRV-Tools-shared.so depuis notre préfixe local.
# Sans LD_LIBRARY_PATH, le linker dynamique ne trouve pas la .so au runtime.
export LD_LIBRARY_PATH="$SPIRV_HOST_PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [ "$SKIP_NATIVE" -eq 0 ]; then
    echo -e "${GREEN}=== Étape 1/3: Compilation des outils natifs ===${NC}"

    cd "$MESA_DIR"
    rm -rf "$BUILD_NATIVE"

    meson setup "$BUILD_NATIVE" \
        -Dprefix="$MESA_COMPILER_PREFIX" \
        -Dbuildtype=release \
        -Dstrip=true \
        -Dplatforms= \
        -Dgallium-drivers= \
        -Dvulkan-drivers=imagination \
        -Dimagination-srv=true \
        -Dmesa-clc=enabled \
        -Dinstall-mesa-clc=true \
        -Dprecomp-compiler=enabled \
        -Dinstall-precomp-compiler=true \
        -Dmesa-clc-bundle-headers=enabled

    ninja -C "$BUILD_NATIVE" -j"$(nproc)"
    ninja -C "$BUILD_NATIVE" install
    echo -e "${GREEN}✓ Outils natifs compilés et installés dans $MESA_COMPILER_PREFIX${NC}"
else
    echo -e "${YELLOW}Étape 1 ignorée (--skip-native)${NC}"
    command -v mesa_clc >/dev/null 2>&1 || {
        echo -e "${RED}ERREUR: mesa_clc introuvable dans PATH. Lance sans --skip-native d'abord.${NC}"
        exit 1
    }
fi

# ---------------------------------------------------------------------------
# Étape 2: Cross-compilation pour Android riscv64
# ---------------------------------------------------------------------------
echo -e "${GREEN}=== Étape 2/3: Cross-compilation pour Android riscv64 ===${NC}"

cd "$MESA_DIR"
rm -rf "$BUILD_ANDROID"

if [ "$BOARD" = "a210" ]; then
    # Sandbox pkg-config away from the HOST's native x86_64 packages
    # entirely (empty PKG_CONFIG_LIBDIR, not unset — unset lets pkg-config
    # fall back to its compiled-in system search path, which is how a
    # plain rerun picked up the host's own libglvnd.pc for this riscv64
    # cross build and pulled in glvnd EGL dispatch code that needs headers
    # we don't have: "fatal error: 'glvnd/libeglabi.h' file not found").
    # cutils/hardware/log/sync/nativewindow/libdrm/
    # android.hardware.graphics.mapper/math/android-hwvulkan-headers/
    # cpp_stdlib all come from the subprojects/vndk wrap Mesa already ships
    # (subprojects/vndk.wrap's [provide] section) once pkg-config reports
    # them not found; zlib/expat fall back to their own vendored wraps the
    # same way.
    export PKG_CONFIG_LIBDIR=
    unset PKG_CONFIG_PATH
fi

meson setup "$BUILD_ANDROID" \
  --cross-file "$CROSS_FILE" \
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
  -Dllvm=disabled \
  -Dallow-fallback-for=libdrm,perfetto \
  -Dgbm-backends-path=/vendor/lib64/gbm

ninja -C "$BUILD_ANDROID" -j"$(nproc)"
echo -e "${GREEN}✓ Mesa Android riscv64 compilé${NC}"

# ---------------------------------------------------------------------------
# Étape 3: Deploy dans l'AOSP device tree
# ---------------------------------------------------------------------------
echo -e "${GREEN}=== Étape 3/3: Deploy dans $DEVICE_MESA ===${NC}"

[ -d "$DEVICE_MESA" ] || { echo -e "${RED}ERREUR: $DEVICE_MESA introuvable${NC}"; exit 1; }

B="$MESA_DIR/$BUILD_ANDROID"

deploy() {
    local src="$1" dst="$2"
    [ -f "$src" ] || { echo -e "${RED}ERREUR: $src introuvable${NC}"; exit 1; }
    mkdir -p "$(dirname "$dst")"
    \cp -f "$src" "$dst"
    echo -e "${GREEN}✓${NC} $(basename "$src") → $(realpath --relative-to="$DEVICE_MESA/.." "$dst")"
}

# Vulkan PowerVR
deploy "$B/src/imagination/vulkan/libvulkan_powervr_mesa.so" \
       "$DEVICE_MESA/hw/vulkan.mesa.so"

# Gallium DRI (copié — même binaire)
deploy "$B/src/gallium/targets/dri/libgallium_dri.so" \
       "$DEVICE_MESA/egl/libgallium_dri.so"
deploy "$B/src/gallium/targets/dri/libgallium_dri.so" \
       "$DEVICE_MESA/dri/zink_dri.so"
deploy "$B/src/gallium/targets/dri/libgallium_dri.so" \
       "$DEVICE_MESA/dri/powervr_dri.so"
# a210's Android.bp only references egl/libgallium_dri.so, dri/zink_dri.so
# and dri/powervr_dri.so — no spacemit_dri module on that device.
if [ "$BOARD" = "k1" ]; then
    deploy "$B/src/gallium/targets/dri/libgallium_dri.so" \
           "$DEVICE_MESA/dri/spacemit_dri.so"
fi

# GBM
deploy "$B/src/gbm/backends/dri/dri_gbm.so"  "$DEVICE_MESA/gbm/dri_gbm.so"
deploy "$B/src/gbm/libgbm_mesa.so.1.0.0"     "$DEVICE_MESA/libgbm_mesa.so"

# Mesa build produit libgbm_mesa.so avec SONAME "libgbm_mesa.so.1".
# Le linker Android compile libgbm_mesa_wrapper.so avec shared_libs:["libgbm_mesa"],
# lit la SONAME dans le prebuilt, et l'écrit dans DT_NEEDED → à runtime il cherche
# "libgbm_mesa.so.1" qui n'existe pas sur le device.
# Fix : patcher la SONAME en "libgbm_mesa.so" dans le prebuilt, pour que le wrapper
# soit linké contre "libgbm_mesa.so" et trouve bien le fichier déployé.
if ! command -v patchelf >/dev/null 2>&1; then
    echo -e "${YELLOW}patchelf absent — installation...${NC}"
    sudo apt-get install -y patchelf >/dev/null 2>&1 || {
        echo -e "${RED}ERREUR: impossible d'installer patchelf. Lance: sudo apt install patchelf${NC}"
        exit 1
    }
fi
patchelf --set-soname libgbm_mesa.so "$DEVICE_MESA/libgbm_mesa.so"
echo -e "${GREEN}✓${NC} SONAME libgbm_mesa.so.1 → libgbm_mesa.so (patchelf)"

# EGL / GLES
deploy "$B/src/egl/libEGL.so.1.0.0"                      "$DEVICE_MESA/egl/libGLES_mesa.so"
deploy "$B/src/mesa/glapi/es1api/libGLESv1_CM.so.1.1.0"  "$DEVICE_MESA/egl/libGLESv1_CM.so"
deploy "$B/src/mesa/glapi/es2api/libGLESv2.so.2.0.0"     "$DEVICE_MESA/egl/libGLESv2.so"

echo ""
echo -e "${GREEN}=== Build + deploy terminé ! ===${NC}"
echo ""
echo -e "Pour rebuilder l'AOSP avec les nouvelles libs :"
echo -e "  ${YELLOW}cd $AOSP_DIR && source build/envsetup.sh && lunch $LUNCH_TARGET && m${NC}"
