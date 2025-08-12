# Copyright 2022 Google
# SPDX-License-Identifier: MIT

files_libplatform_virtgpu_drm = files(
  'DrmVirtGpuDevice.cpp',
  'DrmVirtGpuBlobMapping.cpp',
  'DrmVirtGpuBlob.cpp',
  'DrmSync.cpp',
)

deps_virtgpu_drm = [dep_libdrm]
if host_machine.system() == 'android'
  deps_virtgpu_drm += dep_android
endif

libplatform_virtgpu_drm = static_library(
   'platform_virtgpu_drm',
   files_libplatform_virtgpu_drm,
   cpp_args: gfxstream_guest_args,
   include_directories: [inc_platform_virtgpu, inc_src, inc_include],
   dependencies: dep_libdrm,
)
