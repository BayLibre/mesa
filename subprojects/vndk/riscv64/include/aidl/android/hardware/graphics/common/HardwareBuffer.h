/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/HardwareBuffer.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/HardwareBuffer.aidl
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
#include <aidl/android/hardware/common/NativeHandle.h>
#include <aidl/android/hardware/graphics/common/HardwareBufferDescription.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::common {
class NativeHandle;
}  // namespace aidl::android::hardware::common
namespace aidl::android::hardware::graphics::common {
class HardwareBufferDescription;
}  // namespace aidl::android::hardware::graphics::common
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
class __attribute__((deprecated(": Use instead android.hardware.HardwareBuffer in frameworks/base"))) HardwareBuffer {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  ::aidl::android::hardware::graphics::common::HardwareBufferDescription description;
  ::aidl::android::hardware::common::NativeHandle handle;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const HardwareBuffer& _rhs) const {
    return std::tie(description, handle) == std::tie(_rhs.description, _rhs.handle);
  }
  inline bool operator<(const HardwareBuffer& _rhs) const {
    return std::tie(description, handle) < std::tie(_rhs.description, _rhs.handle);
  }
  inline bool operator!=(const HardwareBuffer& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const HardwareBuffer& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const HardwareBuffer& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const HardwareBuffer& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "HardwareBuffer{";
    _aidl_os << "description: " << ::android::internal::ToString(description);
    _aidl_os << ", handle: " << ::android::internal::ToString(handle);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
