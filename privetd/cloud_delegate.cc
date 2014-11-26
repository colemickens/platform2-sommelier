// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/cloud_delegate.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>
#include <chromeos/errors/error.h>
#include <chromeos/variant_dictionary.h>
#include <dbus/bus.h>

#include "buffet/dbus-proxies.h"
#include "privetd/device_delegate.h"
#include "privetd/peerd_client.h"

namespace privetd {

namespace {

using chromeos::VariantDictionary;
using chromeos::ErrorPtr;

class CloudDelegateImpl : public CloudDelegate {
 public:
  CloudDelegateImpl(const scoped_refptr<dbus::Bus>& bus,
                    DeviceDelegate* device,
                    const base::Closure& on_changed)
      : manager_proxy_{bus}, device_{device}, on_changed_{on_changed} {}

  ~CloudDelegateImpl() override = default;

  bool Init() {
    // TODO(vitalybuka): check if buffet available and return false if missing.
    ErrorPtr error;
    // TODO(vitalybuka): monitor registration status.
    if (!manager_proxy_.CheckDeviceRegistered(&cloud_id_, &error)) {
      LOG(ERROR) << "CheckDeviceRegistered failed:" << error->GetMessage();
      state_ = ConnectionState(ConnectionState::kUnconfigured);
      return true;
    }

    std::string json;
    // TODO(vitalybuka): monitor device status using state of GCD notification
    // channel.
    if (!manager_proxy_.GetDeviceInfo(&json, &error)) {
      LOG(ERROR) << "GetDeviceInfo failed:" << error->GetMessage();
      state_ = ConnectionState(ConnectionState::kOffline);
      return true;
    }

    LOG(ERROR) << "GetDeviceInfo:" << json;

    state_ = ConnectionState(ConnectionState::kOnline);
    return true;
  }

  // CloudDelegate methods
  bool IsRequired() const override { return false; }

  ConnectionState GetConnectionState() const override { return state_; }

  SetupState GetSetupState() const override { return setup_state_; }

  bool Setup(const std::string& ticket_id, const std::string& user) override {
    if (setup_state_.status == SetupState::kInProgress)
      return false;
    VLOG(1) << "GCD Setup started. ticket_id: " << ticket_id
            << ", user:" << user;
    setup_state_ = SetupState(SetupState::kInProgress);
    cloud_id_.clear();
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&CloudDelegateImpl::CallManagerRegisterDevice,
                              weak_factory_.GetWeakPtr(), ticket_id));
    on_changed_.Run();
    // Return true because we tried setup.
    return true;
  }

  std::string GetCloudId() const override { return cloud_id_; }

 private:
  void CallManagerRegisterDevice(const std::string& ticket_id) {
    VariantDictionary params{
        {"ticket_id", ticket_id},
        {"display_name", device_->GetName()},
        {"description", device_->GetDescription()},
        {"location", device_->GetLocation()},
    };

    ErrorPtr error;
    // TODO(vitalybuka): async call with updating setup_state_ from result.
    if (!manager_proxy_.RegisterDevice(params, &cloud_id_, &error)) {
      LOG(ERROR) << "Failed to receive a response:" << error->GetMessage();
      setup_state_ = SetupState(Error::kServerError);
      return;
    }
    VLOG(1) << "Device registered: " << cloud_id_ << std::endl;
    setup_state_ = SetupState(SetupState::kSuccess);

    on_changed_.Run();
  }

  org::chromium::Buffet::ManagerProxy manager_proxy_;

  DeviceDelegate* device_;

  base::Closure on_changed_;

  // Primary state of GCD.
  ConnectionState state_{ConnectionState::kUnconfigured};

  // State of the current or last setup.
  SetupState setup_state_{SetupState::kNone};

  // Cloud ID if device is registered.
  std::string cloud_id_;

  base::WeakPtrFactory<CloudDelegateImpl> weak_factory_{this};
};

}  // namespace

CloudDelegate::CloudDelegate() {
}

CloudDelegate::~CloudDelegate() {
}

// static
std::unique_ptr<CloudDelegate> CloudDelegate::CreateDefault(
    const scoped_refptr<dbus::Bus>& bus,
    DeviceDelegate* device,
    const base::Closure& on_changed) {
  std::unique_ptr<CloudDelegateImpl> gcd(
      new CloudDelegateImpl(bus, device, on_changed));
  if (!gcd->Init())
    gcd.reset();
  return std::move(gcd);
}

}  // namespace privetd
