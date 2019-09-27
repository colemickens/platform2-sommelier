// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "u2fd/u2f_daemon.h"

#include <string>
#include <sysexits.h>

#include <bindings/chrome_device_policy.pb.h>
#include <dbus/u2f/dbus-constants.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>

#include "u2fd/u2fhid.h"
#include "u2fd/uhid_device.h"

namespace u2f {

namespace em = enterprise_management;

enum class U2fMode : uint8_t {
  kUnset = em::DeviceSecondFactorAuthenticationProto_U2fMode_UNSET,
  kDisabled = em::DeviceSecondFactorAuthenticationProto_U2fMode_DISABLED,
  kU2f = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F,
  kU2fExtended = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F_EXTENDED,
};

namespace {

constexpr char kDeviceName[] = "Integrated U2F";
constexpr int kWinkSignalMinIntervalMs = 1000;

// The U2F counter stored in cr50 is stored in a format resistant to rollbacks,
// and that guarantees monotonicity even in the presence of partial writes.
// See //platform/ec/include/nvcounter.h
//
// The counter is stored across 2 pages of flash - a high page and a low page,
// with each page containing 512 4-byte words. The counter increments using
// 'strikes', with each strike occupying 4 bits. The high page can represent
// numbers 0-2048, and the low page can represent numbers 0-4096.
// The pages are interpreted as two digits of a base-4097 number, giving us
// the maximum value below.
// See //platform/ec/common/nvcounter.c for more details.
constexpr uint32_t kMaxCr50U2fCounterValue = (2048 * 4097) + 4096;
// If we are supporting legacy key handles, we initialize the counter such that
// it is always larger than the maximum possible value cr50 could have returned,
// and therefore guarantee that we provide a monotonically increasing counter
// value for migrated key handles.
constexpr uint32_t kLegacyKhCounterMin = kMaxCr50U2fCounterValue + 1;

bool U2fPolicyReady() {
  policy::PolicyProvider policy_provider;

  return policy_provider.Reload();
}

U2fMode ReadU2fPolicy() {
  policy::PolicyProvider policy_provider;

  if (!policy_provider.Reload()) {
    LOG(DFATAL) << "Failed to load device policy";
  }

  int mode = 0;
  const policy::DevicePolicy* policy = &policy_provider.GetDevicePolicy();
  if (!policy->GetSecondFactorAuthenticationMode(&mode))
    return U2fMode::kUnset;

  return static_cast<U2fMode>(mode);
}

const char* U2fModeToString(U2fMode mode) {
  switch (mode) {
    case U2fMode::kUnset:
      return "unset";
    case U2fMode::kDisabled:
      return "disabled";
    case U2fMode::kU2f:
      return "U2F";
    case U2fMode::kU2fExtended:
      return "U2F+extensions";
  }
  return "unknown";
}

U2fMode GetU2fMode(bool force_u2f, bool force_g2f) {
  U2fMode policy_mode = ReadU2fPolicy();

  LOG(INFO) << "Requested Mode: Policy[" << U2fModeToString(policy_mode)
            << "], force_u2f[" << force_u2f << "], force_g2f[" << force_g2f
            << "]";

  // Always honor the administrator request to disable even if given
  // contradictory override flags.
  if (policy_mode == U2fMode::kDisabled) {
    LOG(INFO) << "Mode: Disabled (explicitly by policy)";
    return U2fMode::kDisabled;
  }

  if (force_g2f || policy_mode == U2fMode::kU2fExtended) {
    LOG(INFO) << "Mode: U2F+extensions";
    return U2fMode::kU2fExtended;
  }

  if (force_u2f || policy_mode == U2fMode::kU2f) {
    LOG(INFO) << "Mode:U2F";
    return U2fMode::kU2f;
  }

  LOG(INFO) << "Mode: Disabled";
  return U2fMode::kDisabled;
}

void OnPolicySignalConnected(const std::string& interface,
                             const std::string& signal,
                             bool success) {
  if (!success) {
    LOG(FATAL) << "Could not connect to signal " << signal << " on interface "
               << interface;
  }
}

}  // namespace

U2fDaemon::U2fDaemon(bool force_u2f,
                     bool force_g2f,
                     bool legacy_kh_fallback,
                     uint32_t vendor_id,
                     uint32_t product_id)
    : force_u2f_(force_u2f),
      force_g2f_(force_g2f),
      legacy_kh_fallback_(legacy_kh_fallback),
      vendor_id_(vendor_id),
      product_id_(product_id) {}

int U2fDaemon::OnInit() {
  brillo::Daemon::OnInit();

  if (bus_) {
    LOG(ERROR) << "OnInit unexpectedly called twice";
    return EX_SOFTWARE;
  }

  if (!InitializeDBus()) {
    LOG(ERROR) << "Failed to initialize DBus Connection";
    return EX_IOERR;
  }

  if (!InitializeDBusProxies()) {
    return EX_IOERR;
  }

  RegisterDBusU2fInterface();

  sm_proxy_->RegisterPropertyChangeCompleteSignalHandler(
      base::Bind(&U2fDaemon::TryStartService, base::Unretained(this)),
      base::Bind(&OnPolicySignalConnected));

  bool policy_ready = U2fPolicyReady();

  if (policy_ready) {
    int status = StartService();

    // If U2F is not currently enabled, we'll wait for policy updates
    // that may enable it. We don't ever disable U2F on policy updates.
    // TODO(louiscollard): Fix the above.
    if (status != EX_CONFIG)
      return status;
  }

  if (policy_ready) {
    LOG(INFO) << "U2F currently disabled, waiting for policy updates...";
  } else {
    LOG(INFO) << "Policy not available, waiting...";
  }

  return EX_OK;
}

void U2fDaemon::TryStartService(
    const std::string& /* unused dbus signal status */) {
  if (!u2fhid_ && U2fPolicyReady()) {
    int status = StartService();
    if (status != EX_OK && status != EX_CONFIG) {
      exit(status);
    }
  }
}

int U2fDaemon::StartService() {
  if (u2fhid_) {
    // Any failures in previous calls to this function would have caused the
    // program to terminate, so we can assume we have successfully started.
    return EX_OK;
  }

  U2fMode u2f_mode = GetU2fMode(force_u2f_, force_g2f_);
  if (u2f_mode == U2fMode::kDisabled) {
    return EX_CONFIG;
  }

  uint32_t vendor_mode_rc =
      tpm_proxy_.SetU2fVendorMode(static_cast<uint8_t>(u2f_mode));
  if (vendor_mode_rc == u2f::kVendorRcNoSuchCommand) {
    LOG(WARNING) << "U2F Vendor Mode not supported in firmware, ignoring.";
  } else if (vendor_mode_rc) {
    LOG(ERROR) << "Failed to set U2F Vendor Mode.";
    return EX_PROTOCOL;
  }

  u2f_msg_handler_ = std::make_unique<u2f::U2fMessageHandler>(
      std::make_unique<u2f::UserState>(
          sm_proxy_.get(), legacy_kh_fallback_ ? kLegacyKhCounterMin : 0),
      [this]() {
        IgnorePowerButtonPress();
        SendWinkSignal();
      },
      &tpm_proxy_, &metrics_library_, u2f_mode == U2fMode::kU2fExtended,
      legacy_kh_fallback_);

  u2fhid_ = std::make_unique<u2f::U2fHid>(
      std::make_unique<u2f::UHidDevice>(vendor_id_, product_id_, kDeviceName,
                                        "u2fd-tpm-cr50"),
      [this]() { SendWinkSignal(); }, u2f_msg_handler_.get());

  return u2fhid_->Init() ? EX_OK : EX_PROTOCOL;
}

bool U2fDaemon::InitializeDBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  if (!bus_->Connect()) {
    LOG(ERROR) << "Cannot connect to D-Bus.";
    return false;
  }

  if (!bus_->RequestOwnershipAndBlock(u2f::kU2FServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Cannot acquire dbus ownership for " << u2f::kU2FServiceName;
    return EX_IOERR;
  }

  return true;
}

bool U2fDaemon::InitializeDBusProxies() {
  if (!tpm_proxy_.Init()) {
    LOG(ERROR) << "Failed to initialize D-Bus proxy with trunksd.";
    return false;
  }

  pm_proxy_ = std::make_unique<org::chromium::PowerManagerProxy>(bus_.get());
  sm_proxy_ =
      std::make_unique<org::chromium::SessionManagerInterfaceProxy>(bus_.get());

  return true;
}

void U2fDaemon::RegisterDBusU2fInterface() {
  dbus_object_.reset(new brillo::dbus_utils::DBusObject(
      nullptr, bus_, dbus::ObjectPath(u2f::kU2FServicePath)));

  auto u2f_interface = dbus_object_->AddOrGetInterface(u2f::kU2FInterface);

  wink_signal_ = u2f_interface->RegisterSignal<u2f::UserNotification>(
      u2f::kU2FUserNotificationSignal);

  dbus_object_->RegisterAndBlock();
}

void U2fDaemon::SendWinkSignal() {
  static base::TimeTicks last_sent;
  base::TimeDelta elapsed = base::TimeTicks::Now() - last_sent;

  if (elapsed.InMilliseconds() > kWinkSignalMinIntervalMs) {
    u2f::UserNotification notification;
    notification.set_event_type(u2f::UserNotification::TOUCH_NEEDED);

    wink_signal_.lock()->Send(notification);

    last_sent = base::TimeTicks::Now();
  }
}

void U2fDaemon::IgnorePowerButtonPress() {
  // Duration of the user presence persistence on the firmware side.
  const base::TimeDelta kPresenceTimeout = base::TimeDelta::FromSeconds(10);

  brillo::ErrorPtr err;
  // Mask the next power button press for the UI
  pm_proxy_->IgnoreNextPowerButtonPress(kPresenceTimeout.ToInternalValue(),
                                        &err, -1);
}

}  // namespace u2f
