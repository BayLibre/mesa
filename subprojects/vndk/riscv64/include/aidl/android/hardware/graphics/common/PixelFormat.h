/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/PixelFormat.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/PixelFormat.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_enums.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
enum class PixelFormat : int32_t {
  UNSPECIFIED = 0,
  RGBA_8888 = 1,
  RGBX_8888 = 2,
  RGB_888 = 3,
  RGB_565 = 4,
  BGRA_8888 = 5,
  YCBCR_422_SP = 16,
  YCRCB_420_SP = 17,
  YCBCR_422_I = 20,
  RGBA_FP16 = 22,
  RAW16 = 32,
  BLOB = 33,
  IMPLEMENTATION_DEFINED = 34,
  YCBCR_420_888 = 35,
  RAW_OPAQUE = 36,
  RAW10 = 37,
  RAW12 = 38,
  RGBA_1010102 = 43,
  Y8 = 538982489,
  Y16 = 540422489,
  YV12 = 842094169,
  DEPTH_16 = 48,
  DEPTH_24 = 49,
  DEPTH_24_STENCIL_8 = 50,
  DEPTH_32F = 51,
  DEPTH_32F_STENCIL_8 = 52,
  STENCIL_8 = 53,
  YCBCR_P010 = 54,
  HSV_888 = 55,
  R_8 = 56,
  R_16_UINT = 57,
  RG_1616_UINT = 58,
  RGBA_10101010 = 59,
  YCBCR_P210 = 60,
  R_12_UINT = 61,
  R_14_UINT = 62,
  RG_1212_UINT = 63,
  RG_1414_UINT = 64,
  RGBA_12121212_UINT = 65,
  RGBA_14141414_UINT = 66,
  BGRA_1010102 = 67,
  BGRX_1010102 = 68,
};

}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
[[nodiscard]] static inline std::string toString(PixelFormat val) {
  switch(val) {
  case PixelFormat::UNSPECIFIED:
    return "UNSPECIFIED";
  case PixelFormat::RGBA_8888:
    return "RGBA_8888";
  case PixelFormat::RGBX_8888:
    return "RGBX_8888";
  case PixelFormat::RGB_888:
    return "RGB_888";
  case PixelFormat::RGB_565:
    return "RGB_565";
  case PixelFormat::BGRA_8888:
    return "BGRA_8888";
  case PixelFormat::YCBCR_422_SP:
    return "YCBCR_422_SP";
  case PixelFormat::YCRCB_420_SP:
    return "YCRCB_420_SP";
  case PixelFormat::YCBCR_422_I:
    return "YCBCR_422_I";
  case PixelFormat::RGBA_FP16:
    return "RGBA_FP16";
  case PixelFormat::RAW16:
    return "RAW16";
  case PixelFormat::BLOB:
    return "BLOB";
  case PixelFormat::IMPLEMENTATION_DEFINED:
    return "IMPLEMENTATION_DEFINED";
  case PixelFormat::YCBCR_420_888:
    return "YCBCR_420_888";
  case PixelFormat::RAW_OPAQUE:
    return "RAW_OPAQUE";
  case PixelFormat::RAW10:
    return "RAW10";
  case PixelFormat::RAW12:
    return "RAW12";
  case PixelFormat::RGBA_1010102:
    return "RGBA_1010102";
  case PixelFormat::Y8:
    return "Y8";
  case PixelFormat::Y16:
    return "Y16";
  case PixelFormat::YV12:
    return "YV12";
  case PixelFormat::DEPTH_16:
    return "DEPTH_16";
  case PixelFormat::DEPTH_24:
    return "DEPTH_24";
  case PixelFormat::DEPTH_24_STENCIL_8:
    return "DEPTH_24_STENCIL_8";
  case PixelFormat::DEPTH_32F:
    return "DEPTH_32F";
  case PixelFormat::DEPTH_32F_STENCIL_8:
    return "DEPTH_32F_STENCIL_8";
  case PixelFormat::STENCIL_8:
    return "STENCIL_8";
  case PixelFormat::YCBCR_P010:
    return "YCBCR_P010";
  case PixelFormat::HSV_888:
    return "HSV_888";
  case PixelFormat::R_8:
    return "R_8";
  case PixelFormat::R_16_UINT:
    return "R_16_UINT";
  case PixelFormat::RG_1616_UINT:
    return "RG_1616_UINT";
  case PixelFormat::RGBA_10101010:
    return "RGBA_10101010";
  case PixelFormat::YCBCR_P210:
    return "YCBCR_P210";
  case PixelFormat::R_12_UINT:
    return "R_12_UINT";
  case PixelFormat::R_14_UINT:
    return "R_14_UINT";
  case PixelFormat::RG_1212_UINT:
    return "RG_1212_UINT";
  case PixelFormat::RG_1414_UINT:
    return "RG_1414_UINT";
  case PixelFormat::RGBA_12121212_UINT:
    return "RGBA_12121212_UINT";
  case PixelFormat::RGBA_14141414_UINT:
    return "RGBA_14141414_UINT";
  case PixelFormat::BGRA_1010102:
    return "BGRA_1010102";
  case PixelFormat::BGRX_1010102:
    return "BGRX_1010102";
  default:
    return std::to_string(static_cast<int32_t>(val));
  }
}
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
namespace ndk {
namespace internal {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++17-extensions"
template <>
constexpr inline std::array<aidl::android::hardware::graphics::common::PixelFormat, 42> enum_values<aidl::android::hardware::graphics::common::PixelFormat> = {
  aidl::android::hardware::graphics::common::PixelFormat::UNSPECIFIED,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_8888,
  aidl::android::hardware::graphics::common::PixelFormat::RGBX_8888,
  aidl::android::hardware::graphics::common::PixelFormat::RGB_888,
  aidl::android::hardware::graphics::common::PixelFormat::RGB_565,
  aidl::android::hardware::graphics::common::PixelFormat::BGRA_8888,
  aidl::android::hardware::graphics::common::PixelFormat::YCBCR_422_SP,
  aidl::android::hardware::graphics::common::PixelFormat::YCRCB_420_SP,
  aidl::android::hardware::graphics::common::PixelFormat::YCBCR_422_I,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_FP16,
  aidl::android::hardware::graphics::common::PixelFormat::RAW16,
  aidl::android::hardware::graphics::common::PixelFormat::BLOB,
  aidl::android::hardware::graphics::common::PixelFormat::IMPLEMENTATION_DEFINED,
  aidl::android::hardware::graphics::common::PixelFormat::YCBCR_420_888,
  aidl::android::hardware::graphics::common::PixelFormat::RAW_OPAQUE,
  aidl::android::hardware::graphics::common::PixelFormat::RAW10,
  aidl::android::hardware::graphics::common::PixelFormat::RAW12,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_1010102,
  aidl::android::hardware::graphics::common::PixelFormat::Y8,
  aidl::android::hardware::graphics::common::PixelFormat::Y16,
  aidl::android::hardware::graphics::common::PixelFormat::YV12,
  aidl::android::hardware::graphics::common::PixelFormat::DEPTH_16,
  aidl::android::hardware::graphics::common::PixelFormat::DEPTH_24,
  aidl::android::hardware::graphics::common::PixelFormat::DEPTH_24_STENCIL_8,
  aidl::android::hardware::graphics::common::PixelFormat::DEPTH_32F,
  aidl::android::hardware::graphics::common::PixelFormat::DEPTH_32F_STENCIL_8,
  aidl::android::hardware::graphics::common::PixelFormat::STENCIL_8,
  aidl::android::hardware::graphics::common::PixelFormat::YCBCR_P010,
  aidl::android::hardware::graphics::common::PixelFormat::HSV_888,
  aidl::android::hardware::graphics::common::PixelFormat::R_8,
  aidl::android::hardware::graphics::common::PixelFormat::R_16_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RG_1616_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_10101010,
  aidl::android::hardware::graphics::common::PixelFormat::YCBCR_P210,
  aidl::android::hardware::graphics::common::PixelFormat::R_12_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::R_14_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RG_1212_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RG_1414_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_12121212_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::RGBA_14141414_UINT,
  aidl::android::hardware::graphics::common::PixelFormat::BGRA_1010102,
  aidl::android::hardware::graphics::common::PixelFormat::BGRX_1010102,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
