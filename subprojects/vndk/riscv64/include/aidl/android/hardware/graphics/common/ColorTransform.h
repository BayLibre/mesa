/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/ColorTransform.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/ColorTransform.aidl
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
enum class ColorTransform : int32_t {
  IDENTITY = 0,
  ARBITRARY_MATRIX = 1,
  VALUE_INVERSE = 2,
  GRAYSCALE = 3,
  CORRECT_PROTANOPIA = 4,
  CORRECT_DEUTERANOPIA = 5,
  CORRECT_TRITANOPIA = 6,
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
[[nodiscard]] static inline std::string toString(ColorTransform val) {
  switch(val) {
  case ColorTransform::IDENTITY:
    return "IDENTITY";
  case ColorTransform::ARBITRARY_MATRIX:
    return "ARBITRARY_MATRIX";
  case ColorTransform::VALUE_INVERSE:
    return "VALUE_INVERSE";
  case ColorTransform::GRAYSCALE:
    return "GRAYSCALE";
  case ColorTransform::CORRECT_PROTANOPIA:
    return "CORRECT_PROTANOPIA";
  case ColorTransform::CORRECT_DEUTERANOPIA:
    return "CORRECT_DEUTERANOPIA";
  case ColorTransform::CORRECT_TRITANOPIA:
    return "CORRECT_TRITANOPIA";
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
constexpr inline std::array<aidl::android::hardware::graphics::common::ColorTransform, 7> enum_values<aidl::android::hardware::graphics::common::ColorTransform> = {
  aidl::android::hardware::graphics::common::ColorTransform::IDENTITY,
  aidl::android::hardware::graphics::common::ColorTransform::ARBITRARY_MATRIX,
  aidl::android::hardware::graphics::common::ColorTransform::VALUE_INVERSE,
  aidl::android::hardware::graphics::common::ColorTransform::GRAYSCALE,
  aidl::android::hardware::graphics::common::ColorTransform::CORRECT_PROTANOPIA,
  aidl::android::hardware::graphics::common::ColorTransform::CORRECT_DEUTERANOPIA,
  aidl::android::hardware::graphics::common::ColorTransform::CORRECT_TRITANOPIA,
};
#pragma clang diagnostic pop
}  // namespace internal
}  // namespace ndk
