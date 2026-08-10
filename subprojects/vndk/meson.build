# SPDX-License-Identifier: CC0-1.0

project('vndk', 'cpp',
        version : 'vndk-v35')

cpu_family = host_machine.cpu_family()
if cpu_family == 'x86_64'
  cpu = 'x86_64'
  prebuilt_path = cpu / 'arch-x86_64/shared'
elif cpu_family == 'aarch64'
  cpu = 'arm64'
  prebuilt_path = cpu / 'arch-arm64-armv8-a/shared'
elif cpu_family == 'arm'
  cpu = 'arm'
  prebuilt_path = cpu / 'arch-arm-armv7-a-neon/shared'
elif cpu_family == 'riscv64'
  cpu = 'riscv64'
  prebuilt_path = cpu / 'arch-riscv64/shared'
else
    error('Architecture:  ' + cpu_family + ' not supported')
endif

prebuild_libraries = [
    meson.current_source_dir() / prebuilt_path / 'llndk-stub',
    meson.current_source_dir() / prebuilt_path / 'vndk-sp',
    meson.current_source_dir() / prebuilt_path / 'vndk-core',
]

include_base = cpu / 'include'

cxx = meson.get_compiler('cpp')

libdrm_dep = declare_dependency(
    version : '2.4.124',
    dependencies : cxx.find_library('drm', dirs : prebuild_libraries),
    include_directories : [
        include_directories(include_base / 'external/libdrm'),
        include_directories(include_base / 'external/libdrm/include/drm'),
    ])
meson.override_dependency('libdrm', libdrm_dep)
meson.override_dependency('drm', libdrm_dep)

libbase_dep = declare_dependency(
    dependencies : cxx.find_library('base', dirs : prebuild_libraries),
    include_directories : include_directories(include_base / 'system/libbase/include'))
meson.override_dependency('libbase', libbase_dep)

liblog_dep = declare_dependency(
    dependencies : cxx.find_library('log', dirs : prebuild_libraries),
    include_directories : include_directories(include_base / 'system/logging/liblog/include_vndk'))
meson.override_dependency('liblog', liblog_dep)
meson.override_dependency('log', liblog_dep)

cutils_dep = declare_dependency(
    dependencies : [
        cxx.find_library('cutils', dirs : prebuild_libraries),
        liblog_dep,
    ],
    include_directories : include_directories(include_base / 'system/core/libcutils/include_outside_system'))
meson.override_dependency('libcutils', cutils_dep)
meson.override_dependency('cutils', cutils_dep)

libutils_dep = declare_dependency(
    dependencies : cxx.find_library('utils', dirs : prebuild_libraries),
    include_directories : include_directories(include_base / 'system/core/libutils/include'))
meson.override_dependency('libutils', libutils_dep)

libsystem_dep = declare_dependency(
    include_directories : include_directories(include_base / 'system/core/libsystem/include'))
meson.override_dependency('libsystem', libsystem_dep)

libhardware_dep = declare_dependency(
    dependencies : [
      cxx.find_library('hardware', dirs : prebuild_libraries),
      libsystem_dep,
    ],
    include_directories : include_directories(include_base / 'hardware/libhardware/include'))
meson.override_dependency('libhardware', libhardware_dep)
meson.override_dependency('hardware', libhardware_dep)

libmedia_dep = declare_dependency(
    include_directories : include_directories(include_base / 'system/media/camera/include'))
meson.override_dependency('libmedia', libmedia_dep)

libmath_dep = declare_dependency(
    dependencies : cxx.find_library('m', dirs : prebuild_libraries),
    include_directories : include_directories(include_base / 'frameworks/native/libs/math/include'),
)
meson.override_dependency('libmath', libmath_dep)

# libui/ui is overridden further down, after libnativewindow_dep exists.

vndk_libcpp_dep = declare_dependency(
    dependencies : [
        cxx.find_library('c++', dirs : prebuild_libraries),
        cxx.find_library('c', dirs : prebuild_libraries),
    ],
    include_directories : [
        include_directories(include_base / 'external/libcxx/include'),
        include_directories(include_base / 'external/libcxxabi/include'),
    ])
meson.override_dependency('cpp_stdlib', vndk_libcpp_dep)

libsync_dep = declare_dependency(
    dependencies : cxx.find_library('sync', dirs : prebuild_libraries),
    include_directories : include_directories(include_base / 'system/core/libsync/include'))
meson.override_dependency('libsync', libsync_dep)
meson.override_dependency('sync', libsync_dep)

libnativewindow_dep = declare_dependency(
    dependencies : cxx.find_library('nativewindow', dirs : prebuild_libraries),
    include_directories : [
      include_directories(include_base / 'frameworks/native/libs/nativewindow/include'),
      include_directories(include_base / 'frameworks/native/libs/nativebase/include'),
    ])
meson.override_dependency('libnativewindow', libnativewindow_dep)
meson.override_dependency('nativewindow', libnativewindow_dep)

# Overriding 'ui' is what selects u_gralloc's IMapper5 backend over the IMapper4
# one. It is required on devices that ship only the stable-C mapper: A17 has no
# HIDL @4.0 mapper service, so the IMapper4 backend never activates and Mesa
# silently degrades to the fallback gralloc, which cannot resolve a DRM fourcc for
# YUV buffers. The AIDL and binder-NDK headers this needs are in the snapshot
# already — the same ones graphics_mapper_dep below uses.
libui_dep = declare_dependency(
    dependencies : [
      cxx.find_library('ui', dirs : prebuild_libraries),
      cxx.find_library('libhidlbase', dirs : prebuild_libraries),
      cxx.find_library('libgralloctypes', dirs : prebuild_libraries),
      libutils_dep,
      libbase_dep,
      libnativewindow_dep,
      libmath_dep,
    ],
    include_directories : [
      include_directories(include_base / 'frameworks/native/libs/ui/include'),
      include_directories(include_base / 'frameworks/native/libs/arect/include'),
      include_directories(include_base / 'frameworks/native/libs/gralloc/types/include'),
      include_directories(include_base / 'frameworks/native/libs/binder/ndk/include_cpp'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V4-ndk-source/gen/include'),
      # ui/GraphicTypes.h pulls the HIDL graphics.common types in
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen'),
      include_directories(include_base / 'system/libhidl/base/include'),
      include_directories(include_base / 'system/libfmq/base'),
    ])
meson.override_dependency('libui', libui_dep)
meson.override_dependency('ui', libui_dep)

graphics_mapper_dep = declare_dependency(
    version : '4.0',
    dependencies : [
      libutils_dep,
      cxx.find_library('libgralloctypes', dirs : prebuild_libraries),
      cxx.find_library('libhidlbase', dirs : prebuild_libraries),
      cxx.find_library('android.hardware.graphics.mapper@4.0', dirs : prebuild_libraries),
    ],
    include_directories : [
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V4-ndk-source/gen/include'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/mapper/4.0/android.hardware.graphics.mapper@4.0_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen'),
      include_directories(include_base / 'generated-headers/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen'),
      include_directories(include_base / 'frameworks/native/libs/binder/ndk/include_cpp'),
      include_directories(include_base / 'frameworks/native/libs/gralloc/types/include'),
      include_directories(include_base / 'system/libhidl/base/include'),
      include_directories(include_base / 'system/core/libutils/include'),
      include_directories(include_base / 'system/libfmq/base'),
      include_directories(include_base / 'frameworks/native/libs/nativebase/include'),
    ])
meson.override_dependency('android.hardware.graphics.mapper', graphics_mapper_dep)

libvulkan_dep = declare_dependency(
    include_directories : (include_directories(include_base / 'frameworks/native/vulkan/include')))
meson.override_dependency('libvulkan', libvulkan_dep)

# hwvulkan.h lives under frameworks/native/vulkan/include/hardware/
hwvulkan_headers_dep = declare_dependency(
    include_directories : (include_directories(include_base / 'frameworks/native/vulkan/include')))
meson.override_dependency('android-hwvulkan-headers', hwvulkan_headers_dep)
