// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/time/time.h>
#include <brillo/daemons/daemon.h>
#include <brillo/dbus/dbus_object.h>
#include <brillo/dbus/dbus_signal.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>
#include <dbus/u2f/dbus-constants.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>
#include <session_manager/dbus-proxies.h>
#include <sysexits.h>
#include <u2f/proto_bindings/interface.pb.h>

#include "bindings/chrome_device_policy.pb.h"
#include "power_manager-client/power_manager/dbus-proxies.h"
#include "u2fd/tpm_vendor_cmd.h"
#include "u2fd/u2fhid.h"
#include "u2fd/uhid_device.h"
#include "u2fd/user_state.h"

#ifndef VCSID
#define VCSID "<unknown>"
#endif

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

namespace em = enterprise_management;

enum class U2fMode : uint8_t {
  kUnset = em::DeviceSecondFactorAuthenticationProto_U2fMode_UNSET,
  kDisabled = em::DeviceSecondFactorAuthenticationProto_U2fMode_DISABLED,
  kU2f = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F,
  kU2fExtended = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F_EXTENDED,
};

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

U2fMode GetU2fMode(bool force_u2f, bool force_g2f) {
  U2fMode policy_mode = ReadU2fPolicy();

  // Always honor the administrator request to disable even if given
  // contradictory override flags.
  if (policy_mode == U2fMode::kDisabled) {
    return U2fMode::kDisabled;
  }

  if (force_g2f || policy_mode == U2fMode::kU2fExtended) {
    return U2fMode::kU2fExtended;
  }

  if (force_u2f || policy_mode == U2fMode::kU2f) {
    return U2fMode::kU2f;
  }

  return U2fMode::kDisabled;
}

const char* U2fModeToString(U2fMode mode) {
  switch (mode) {
    case U2fMode::kUnset:
      return "disabled (no policy)";
    case U2fMode::kDisabled:
      return "disabled";
    case U2fMode::kU2f:
      return "U2F";
    case U2fMode::kU2fExtended:
      return "U2F+extensions";
  }
  return "unknown";
}

void OnPolicySignalConnected(const std::string& interface,
                             const std::string& signal,
                             bool success) {
  if (!success) {
    LOG(FATAL) << "Could not connect to signal " << signal << " on interface "
               << interface;
  }
}

class U2fDaemon : public brillo::Daemon {
 public:
  U2fDaemon(bool force_u2f,
            bool force_g2f,
            bool user_keys,
            bool legacy_kh_fallback,
            uint32_t vendor_id,
            uint32_t product_id)
      : force_u2f_(force_u2f),
        force_g2f_(force_g2f),
        user_keys_(user_keys),
        legacy_kh_fallback_(legacy_kh_fallback),
        vendor_id_(vendor_id),
        product_id_(product_id) {}

 private:
  int OnInit() override {
    brillo::Daemon::OnInit();

    if (!bus_ && !InitializeDBus())
      return EX_IOERR;

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

  void TryStartService(const std::string& /* unused dbus signal status */) {
    if (!u2fhid_ && U2fPolicyReady()) {
      int status = StartService();
      if (status != EX_OK && status != EX_CONFIG) {
        exit(status);
      }
    }
  }

  int StartService() {
    if (u2fhid_) {
      // Any failures in previous calls to this function would have caused the
      // program to terminate, so we can assume we have successfully started.
      return EX_OK;
    }

    u2f_mode_ = GetU2fMode(force_u2f_, force_g2f_);
    LOG(INFO) << "Mode: " << U2fModeToString(u2f_mode_)
              << " (force_u2f=" << force_u2f_ << " force_g2f=" << force_g2f_
              << ")";
    if (u2f_mode_ == U2fMode::kDisabled) {
      return EX_CONFIG;
    }

    // User keys should always be enabled when a U2F policy is set or G2F mode
    // is enabled, and may additionally be enabled on the command line.
    // User keys may not be disabled if a policy is defined, as non-user keys
    // are legacy and should not be used beyond the initial beta launch.
    if (ReadU2fPolicy() != U2fMode::kUnset || force_g2f_) {
      user_keys_ = true;
    }

    RegisterU2fDBusInterface();

    if (!tpm_proxy_.Init()) {
      LOG(ERROR) << "Failed to initialize D-Bus proxy with trunksd.";
      return EX_IOERR;
    }
    int rc = tpm_proxy_.SetU2fVendorMode(static_cast<uint8_t>(u2f_mode_));
    if (rc == u2f::kVendorRcNoSuchCommand) {
      LOG(WARNING) << "U2F Feature not available in firmware.";
      // Will exit gracefully as we don't want to re-spawn.
      return EX_UNAVAILABLE;
    } else if (rc) {
      LOG(WARNING) << "Error setting U2F Vendor Mode: " << rc;
      return EX_PROTOCOL;
    }

    std::string vendor_sysinfo;
    if (u2f_mode_ == U2fMode::kU2fExtended)
      tpm_proxy_.GetVendorSysInfo(&vendor_sysinfo);

    u2fhid_ = std::make_unique<u2f::U2fHid>(
        std::make_unique<u2f::UHidDevice>(vendor_id_, product_id_, kDeviceName,
                                          "u2fd-tpm-cr50"),
        vendor_sysinfo, u2f_mode_ == U2fMode::kU2fExtended, user_keys_,
        legacy_kh_fallback_,
        base::Bind(&u2f::TpmVendorCommandProxy::SendU2fApdu,
                   base::Unretained(&tpm_proxy_)),
        base::Bind(&u2f::TpmVendorCommandProxy::SendU2fGenerate,
                   base::Unretained(&tpm_proxy_)),
        base::Bind(&u2f::TpmVendorCommandProxy::SendU2fSign,
                   base::Unretained(&tpm_proxy_)),
        base::Bind(&u2f::TpmVendorCommandProxy::SendU2fAttest,
                   base::Unretained(&tpm_proxy_)),
        base::Bind(&u2f::TpmVendorCommandProxy::GetG2fCertificate,
                   base::Unretained(&tpm_proxy_)),
        base::Bind(
            &org::chromium::PowerManagerProxy::IgnoreNextPowerButtonPress,
            base::Unretained(pm_proxy_.get())),
        base::Bind(&U2fDaemon::SendWinkSignal, base::Unretained(this)),
        std::make_unique<u2f::UserState>(
            sm_proxy_.get(), legacy_kh_fallback_ ? kLegacyKhCounterMin : 0));

    return u2fhid_->Init() ? EX_OK : EX_PROTOCOL;
  }

  bool InitializeDBus() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(options);
    if (!bus_->Connect()) {
      LOG(ERROR) << "Cannot connect to D-Bus.";
      return false;
    }

    if (!bus_->RequestOwnershipAndBlock(u2f::kU2FServiceName,
                                        dbus::Bus::REQUIRE_PRIMARY)) {
      LOG(ERROR) << "Cannot acquire dbus ownership for "
                 << u2f::kU2FServiceName;
      return EX_IOERR;
    }

    pm_proxy_ = std::make_unique<org::chromium::PowerManagerProxy>(bus_.get());
    sm_proxy_ = std::make_unique<org::chromium::SessionManagerInterfaceProxy>(
        bus_.get());

    return true;
  }

  void RegisterU2fDBusInterface() {
    dbus_object_.reset(new brillo::dbus_utils::DBusObject(
        nullptr, bus_, dbus::ObjectPath(u2f::kU2FServicePath)));

    auto u2f_interface = dbus_object_->AddOrGetInterface(u2f::kU2FInterface);

    wink_signal_ = u2f_interface->RegisterSignal<u2f::UserNotification>(
        u2f::kU2FUserNotificationSignal);

    dbus_object_->RegisterAndBlock();
  }

  void SendWinkSignal() {
    static base::TimeTicks last_sent;
    base::TimeDelta elapsed = base::TimeTicks::Now() - last_sent;

    if (elapsed.InMilliseconds() > kWinkSignalMinIntervalMs) {
      u2f::UserNotification notification;
      notification.set_event_type(u2f::UserNotification::TOUCH_NEEDED);

      wink_signal_.lock()->Send(notification);

      last_sent = base::TimeTicks::Now();
    }
  }

  bool force_u2f_;
  bool force_g2f_;
  bool user_keys_;
  bool legacy_kh_fallback_;
  uint32_t vendor_id_;
  uint32_t product_id_;
  U2fMode u2f_mode_;
  u2f::TpmVendorCommandProxy tpm_proxy_;
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<org::chromium::PowerManagerProxy> pm_proxy_;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxy> sm_proxy_;
  std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object_;
  std::weak_ptr<brillo::dbus_utils::DBusSignal<u2f::UserNotification>>
      wink_signal_;
  std::unique_ptr<u2f::U2fHid> u2fhid_;

  DISALLOW_COPY_AND_ASSIGN(U2fDaemon);
};

}  // namespace

int main(int argc, char* argv[]) {
  DEFINE_bool(force_u2f, false, "force U2F mode even if disabled by policy");
  DEFINE_bool(
      force_g2f, false, "force U2F mode plus extensions regardless of policy");
  DEFINE_int32(product_id, u2f::kDefaultProductId,
               "Product ID for the HID device");
  DEFINE_int32(vendor_id, u2f::kDefaultVendorId,
               "Vendor ID for the HID device");
  DEFINE_bool(verbose, false, "verbose logging");
  DEFINE_bool(user_keys, false, "Whether to use user-specific keys");
  DEFINE_bool(legacy_kh_fallback, false,
              "Whether to allow auth with legacy keys when user-specific keys "
              "are enabled");

  brillo::FlagHelper::Init(argc, argv, "u2fd, U2FHID emulation daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);
  if (FLAGS_verbose)
    logging::SetMinLogLevel(-1);

  LOG(INFO) << "Daemon version " << VCSID;

  U2fDaemon daemon(FLAGS_force_u2f, FLAGS_force_g2f, FLAGS_user_keys,
                   FLAGS_legacy_kh_fallback, FLAGS_vendor_id, FLAGS_product_id);
  int rc = daemon.Run();

  return rc == EX_UNAVAILABLE ? EX_OK : rc;
}
