/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/PlaneLayoutComponent.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/PlaneLayoutComponent.aidl
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
#include <aidl/android/hardware/graphics/common/ExtendableType.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::graphics::common {
class ExtendableType;
}  // namespace aidl::android::hardware::graphics::common
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
class PlaneLayoutComponent {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  ::aidl::android::hardware::graphics::common::ExtendableType type;
  int64_t offsetInBits = 0L;
  int64_t sizeInBits = 0L;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const PlaneLayoutComponent& _rhs) const {
    return std::tie(type, offsetInBits, sizeInBits) == std::tie(_rhs.type, _rhs.offsetInBits, _rhs.sizeInBits);
  }
  inline bool operator<(const PlaneLayoutComponent& _rhs) const {
    return std::tie(type, offsetInBits, sizeInBits) < std::tie(_rhs.type, _rhs.offsetInBits, _rhs.sizeInBits);
  }
  inline bool operator!=(const PlaneLayoutComponent& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const PlaneLayoutComponent& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const PlaneLayoutComponent& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const PlaneLayoutComponent& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "PlaneLayoutComponent{";
    _aidl_os << "type: " << ::android::internal::ToString(type);
    _aidl_os << ", offsetInBits: " << ::android::internal::ToString(offsetInBits);
    _aidl_os << ", sizeInBits: " << ::android::internal::ToString(sizeInBits);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
