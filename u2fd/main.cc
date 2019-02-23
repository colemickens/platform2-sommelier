// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include <base/bind.h>
#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <brillo/daemons/daemon.h>
#include <brillo/flag_helper.h>
#include <brillo/syslog_logging.h>
#include <dbus/bus.h>
#include <policy/device_policy.h>
#include <policy/libpolicy.h>
#include <sysexits.h>

#include "bindings/chrome_device_policy.pb.h"
#include "power_manager-client/power_manager/dbus-proxies.h"
#include "u2fd/tpm_vendor_cmd.h"
#include "u2fd/u2fhid.h"
#include "u2fd/uhid_device.h"

#ifndef VCSID
#define VCSID "<unknown>"
#endif

namespace {

constexpr char kDeviceName[] = "Integrated U2F";

namespace em = enterprise_management;

enum class U2fMode : uint8_t {
  kUnset = em::DeviceSecondFactorAuthenticationProto_U2fMode_UNSET,
  kDisabled = em::DeviceSecondFactorAuthenticationProto_U2fMode_DISABLED,
  kU2f = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F,
  kU2fExtended = em::DeviceSecondFactorAuthenticationProto_U2fMode_U2F_EXTENDED,
};

U2fMode ReadU2fPolicy() {
  policy::PolicyProvider policy_provider;

  // No available policy.
  if (!policy_provider.Reload())
    return U2fMode::kUnset;

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

class U2fDaemon : public brillo::Daemon {
 public:
  U2fDaemon(U2fMode mode,
            bool user_keys,
            uint32_t vendor_id,
            uint32_t product_id)
      : u2f_mode_(mode),
        user_keys_(user_keys),
        vendor_id_(vendor_id),
        product_id_(product_id) {}

 private:
  int OnInit() override {
    std::string version;
    if (!tpm_proxy_.Init()) {
      LOG(ERROR) << "Failed to initialize D-Bus proxy with trunksd.";
      return EX_IOERR;
    }
    int rc = tpm_proxy_.SetU2fVendorMode(static_cast<uint8_t>(u2f_mode_));
    if (!rc)
      rc = tpm_proxy_.GetU2fVersion(&version);
    if (rc == u2f::kVendorRcNoSuchCommand) {
      LOG(WARNING) << "U2F Feature not available in firmware.";
      // Will exit gracefully as we don't want to re-spawn.
      return EX_UNAVAILABLE;
    }
    if (rc != 0) {
      LOG(ERROR) << "Cannot get U2F version from TPM.";
      return EX_PROTOCOL;
    }
    LOG(INFO) << "version " << version;

    std::string vendor_sysinfo;
    if (u2f_mode_ == U2fMode::kU2fExtended)
      tpm_proxy_.GetVendorSysInfo(&vendor_sysinfo);

    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(options);
    if (!bus_->Connect()) {
      LOG(ERROR) << "Cannot connect to D-Bus.";
      return EX_IOERR;
    }
    pm_proxy_ = std::make_unique<org::chromium::PowerManagerProxy>(bus_.get());

    u2fhid_ = std::make_unique<u2f::U2fHid>(
        std::make_unique<u2f::UHidDevice>(vendor_id_, product_id_, kDeviceName,
                                          "u2fd-tpm-cr50"),
        vendor_sysinfo, u2f_mode_ == U2fMode::kU2fExtended, user_keys_,
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
            base::Unretained(pm_proxy_.get())));

    return u2fhid_->Init() ? EX_OK : EX_PROTOCOL;
  }

  U2fMode u2f_mode_;
  bool user_keys_;
  uint32_t vendor_id_;
  uint32_t product_id_;
  u2f::TpmVendorCommandProxy tpm_proxy_;
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<org::chromium::PowerManagerProxy> pm_proxy_;
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

  brillo::FlagHelper::Init(argc, argv, "u2fd, U2FHID emulation daemon.");

  brillo::InitLog(brillo::kLogToSyslog | brillo::kLogHeader |
                  brillo::kLogToStderrIfTty);
  if (FLAGS_verbose)
    logging::SetMinLogLevel(-1);

  LOG(INFO) << "Daemon version " << VCSID;

  U2fMode mode = GetU2fMode(FLAGS_force_u2f, FLAGS_force_g2f);
  LOG(INFO) << "Mode: " << U2fModeToString(mode)
            << " (force_u2f=" << FLAGS_force_u2f
            << " force_g2f=" << FLAGS_force_g2f << ")";
  if (mode == U2fMode::kDisabled) {
    LOG(INFO) << "U2F disabled, exiting...";
    // Exit gracefully as we don't want to re-spawn.
    return EX_OK;
  }

  U2fDaemon daemon(mode, FLAGS_user_keys, FLAGS_vendor_id, FLAGS_product_id);
  int rc = daemon.Run();

  return rc == EX_UNAVAILABLE ? EX_OK : rc;
}
