/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/HardwareBufferDescription.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/HardwareBufferDescription.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <android/binder_interface_utils.h>
#include <android/binder_parcelable_utils.h>
#include <android/binder_to_string.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
class HardwareBufferDescription {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  int32_t width = 0;
  int32_t height = 0;
  int32_t layers = 0;
  ::aidl::android::hardware::graphics::common::PixelFormat format = ::aidl::android::hardware::graphics::common::PixelFormat::UNSPECIFIED;
  ::aidl::android::hardware::graphics::common::BufferUsage usage = ::aidl::android::hardware::graphics::common::BufferUsage::CPU_READ_NEVER;
  int32_t stride = 0;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const HardwareBufferDescription& _rhs) const {
    return std::tie(width, height, layers, format, usage, stride) == std::tie(_rhs.width, _rhs.height, _rhs.layers, _rhs.format, _rhs.usage, _rhs.stride);
  }
  inline bool operator<(const HardwareBufferDescription& _rhs) const {
    return std::tie(width, height, layers, format, usage, stride) < std::tie(_rhs.width, _rhs.height, _rhs.layers, _rhs.format, _rhs.usage, _rhs.stride);
  }
  inline bool operator!=(const HardwareBufferDescription& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const HardwareBufferDescription& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const HardwareBufferDescription& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const HardwareBufferDescription& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "HardwareBufferDescription{";
    _aidl_os << "width: " << ::android::internal::ToString(width);
    _aidl_os << ", height: " << ::android::internal::ToString(height);
    _aidl_os << ", layers: " << ::android::internal::ToString(layers);
    _aidl_os << ", format: " << ::android::internal::ToString(format);
    _aidl_os << ", usage: " << ::android::internal::ToString(usage);
    _aidl_os << ", stride: " << ::android::internal::ToString(stride);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
