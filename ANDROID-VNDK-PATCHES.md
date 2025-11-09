# Patches VNDK nécessaires pour Android cross-compilation

Les fichiers dans `subprojects/` sont ignorés par Git Mesa, mais nécessitent des modifications pour la cross-compilation Android.

## Modifications requises dans subprojects/packagefiles/vndk/meson.build

### 1. Ajouter dependency overrides sans préfixe 'lib'

Après chaque `meson.override_dependency('libXXX', ...)`, ajouter:
```python
meson.override_dependency('XXX', ...)
```

Exemples:
```python
# Ligne 47
meson.override_dependency('log', liblog_dep)

# Ligne 55
meson.override_dependency('cutils', cutils_dep)

# Ligne 74
meson.override_dependency('hardware', libhardware_dep)

# Ligne 99
meson.override_dependency('ui', libui_dep)

# Ligne 115
meson.override_dependency('sync', libsync_dep)

# Ligne 124
meson.override_dependency('nativewindow', libnativewindow_dep)
```

### 2. Ajouter version à libdrm_dep

```python
# Ligne 31
libdrm_dep = declare_dependency(
    version : '2.4.120',  # Declare a version that satisfies Mesa requirements
    dependencies : cxx.find_library('drm', dirs : prebuild_libraries),
    ...
)
```

### 3. Ajouter hwvulkan.h include pour libvulkan

```python
# Lignes 154-157
libvulkan_dep = declare_dependency(
    include_directories : [
        include_directories(include_base / 'frameworks/native/vulkan/include'),
        include_directories(include_base / 'hardware/libhardware/include'),
    ])
```

## Modifications requises dans subprojects/vndk/arm64/include/.../ChromaSiting.h

Fichier: `subprojects/vndk/arm64/include/generated-headers/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V4-ndk-source/gen/include/aidl/android/hardware/graphics/common/ChromaSiting.h`

### Ajouter constantes API 35

```cpp
// Ligne 24-25
enum class ChromaSiting : int64_t {
  NONE = 0L,
  UNKNOWN = 1L,
  SITED_INTERSTITIAL = 2L,
  COSITED_HORIZONTAL = 3L,
  COSITED_VERTICAL = 4L,      // Ajouté
  COSITED_BOTH = 5L,          // Ajouté
};
```

### Mettre à jour toString()

```cpp
// Lignes 48-51
  case ChromaSiting::COSITED_VERTICAL:
    return "COSITED_VERTICAL";
  case ChromaSiting::COSITED_BOTH:
    return "COSITED_BOTH";
```

### Mettre à jour enum_values

```cpp
// Ligne 66
constexpr inline std::array<aidl::android::hardware::graphics::common::ChromaSiting, 6> enum_values<...> = {
  ...
  aidl::android::hardware::graphics::common::ChromaSiting::COSITED_VERTICAL,
  aidl::android::hardware::graphics::common::ChromaSiting::COSITED_BOTH,
};
```

## Script d'application automatique des patches

Pour faciliter l'application de ces patches sur un nouveau VNDK, voir le script `apply-vndk-patches.sh` (à créer).

## Notes

- Ces modifications sont nécessaires car VNDK v34 ne contient pas toutes les définitions de l'API Android 35
- Les subprojects Mesa sont exclus du dépôt Git et gérés séparément
- Ces patches doivent être réappliqués après chaque mise à jour du subproject VNDK
