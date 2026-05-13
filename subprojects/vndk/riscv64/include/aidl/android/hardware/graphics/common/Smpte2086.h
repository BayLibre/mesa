/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: out/host/linux-x86/bin/aidl --lang=ndk -Weverything -Wno-missing-permission-annotation --structured --version 7 --hash notfrozen -t --stability vintf --min_sdk_version 29 -pout/soong/.intermediates/hardware/interfaces/common/aidl/android.hardware.common_interface/2/preprocessed.aidl --ninja -d out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging/android/hardware/graphics/common/Smpte2086.cpp.d -h out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/include/staging -o out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-V7-ndk-source/gen/staging -Nhardware/interfaces/graphics/common/aidl hardware/interfaces/graphics/common/aidl/android/hardware/graphics/common/Smpte2086.aidl
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
#include <aidl/android/hardware/graphics/common/XyColor.h>
#ifdef BINDER_STABILITY_SUPPORT
#include <android/binder_stability.h>
#endif  // BINDER_STABILITY_SUPPORT

namespace aidl::android::hardware::graphics::common {
class XyColor;
}  // namespace aidl::android::hardware::graphics::common
namespace aidl {
namespace android {
namespace hardware {
namespace graphics {
namespace common {
class Smpte2086 {
public:
  typedef std::false_type fixed_size;
  static const char* descriptor;

  ::aidl::android::hardware::graphics::common::XyColor primaryRed;
  ::aidl::android::hardware::graphics::common::XyColor primaryGreen;
  ::aidl::android::hardware::graphics::common::XyColor primaryBlue;
  ::aidl::android::hardware::graphics::common::XyColor whitePoint;
  float maxLuminance = 0.000000f;
  float minLuminance = 0.000000f;

  binder_status_t readFromParcel(const AParcel* parcel);
  binder_status_t writeToParcel(AParcel* parcel) const;

  inline bool operator==(const Smpte2086& _rhs) const {
    return std::tie(primaryRed, primaryGreen, primaryBlue, whitePoint, maxLuminance, minLuminance) == std::tie(_rhs.primaryRed, _rhs.primaryGreen, _rhs.primaryBlue, _rhs.whitePoint, _rhs.maxLuminance, _rhs.minLuminance);
  }
  inline bool operator<(const Smpte2086& _rhs) const {
    return std::tie(primaryRed, primaryGreen, primaryBlue, whitePoint, maxLuminance, minLuminance) < std::tie(_rhs.primaryRed, _rhs.primaryGreen, _rhs.primaryBlue, _rhs.whitePoint, _rhs.maxLuminance, _rhs.minLuminance);
  }
  inline bool operator!=(const Smpte2086& _rhs) const {
    return !(*this == _rhs);
  }
  inline bool operator>(const Smpte2086& _rhs) const {
    return _rhs < *this;
  }
  inline bool operator>=(const Smpte2086& _rhs) const {
    return !(*this < _rhs);
  }
  inline bool operator<=(const Smpte2086& _rhs) const {
    return !(_rhs < *this);
  }

  static const ::ndk::parcelable_stability_t _aidl_stability = ::ndk::STABILITY_VINTF;
  inline std::string toString() const {
    std::ostringstream _aidl_os;
    _aidl_os << "Smpte2086{";
    _aidl_os << "primaryRed: " << ::android::internal::ToString(primaryRed);
    _aidl_os << ", primaryGreen: " << ::android::internal::ToString(primaryGreen);
    _aidl_os << ", primaryBlue: " << ::android::internal::ToString(primaryBlue);
    _aidl_os << ", whitePoint: " << ::android::internal::ToString(whitePoint);
    _aidl_os << ", maxLuminance: " << ::android::internal::ToString(maxLuminance);
    _aidl_os << ", minLuminance: " << ::android::internal::ToString(minLuminance);
    _aidl_os << "}";
    return _aidl_os.str();
  }
};
}  // namespace common
}  // namespace graphics
}  // namespace hardware
}  // namespace android
}  // namespace aidl
