#pragma once

#include <android/gui/IWindowInfosReportedListener.h>
#include <binder/IBinder.h>
#include <binder/IInterface.h>
#include <binder/Status.h>
#include <binder/Trace.h>
#include <gui/WindowInfosUpdate.h>
#include <optional>
#include <utils/StrongPointer.h>

namespace android::gui {
class IWindowInfosReportedListener;
}  // namespace android::gui
namespace android {
namespace gui {
class IWindowInfosListenerDelegator;

class IWindowInfosListener : public ::android::IInterface {
public:
  typedef IWindowInfosListenerDelegator DefaultDelegator;
  DECLARE_META_INTERFACE(WindowInfosListener)
  virtual ::android::binder::Status onWindowInfosChanged(const ::android::gui::WindowInfosUpdate& update, const ::android::sp<::android::gui::IWindowInfosReportedListener>& windowInfosReportedListener) = 0;
};  // class IWindowInfosListener

class IWindowInfosListenerDefault : public IWindowInfosListener {
public:
  ::android::IBinder* onAsBinder() override {
    return nullptr;
  }
  ::android::binder::Status onWindowInfosChanged(const ::android::gui::WindowInfosUpdate& /*update*/, const ::android::sp<::android::gui::IWindowInfosReportedListener>& /*windowInfosReportedListener*/) override {
    return ::android::binder::Status::fromStatusT(::android::UNKNOWN_TRANSACTION);
  }
};  // class IWindowInfosListenerDefault
}  // namespace gui
}  // namespace android
