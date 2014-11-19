// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/cloud_delegate.h"

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/weak_ptr.h>
#include <base/message_loop/message_loop.h>

namespace privetd {

namespace {

class CloudDelegateImpl : public CloudDelegate {
 public:
  CloudDelegateImpl() {}
  ~CloudDelegateImpl() override {}

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
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloudDelegateImpl::OnSetupDone, weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(5));
    return true;
  }

  std::string GetCloudId() const override { return cloud_id_; }

 private:
  void OnSetupDone() {
    VLOG(1) << "GCD Setup done";
    setup_state_ = SetupState(SetupState::kSuccess);
    state_ = ConnectionState(ConnectionState::kOnline);
    cloud_id_ = "FakeCloudId";
  }

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
std::unique_ptr<CloudDelegate> CloudDelegate::CreateDefault() {
  return std::unique_ptr<CloudDelegate>(new CloudDelegateImpl);
}

}  // namespace privetd
