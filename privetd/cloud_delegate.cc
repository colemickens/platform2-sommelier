// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/cloud_delegate.h"

#include <base/logging.h>

namespace privetd {

namespace {

class CloudDelegateImpl : public CloudDelegate {
 public:
  CloudDelegateImpl() {}
  ~CloudDelegateImpl() override {}

  // CloudDelegate methods
  bool IsRequired() const override { return false; }

  ConnectionState GetState() const override {
    return ConnectionState{ConnectionState::kUnconfigured};
  }

  SetupState GetSetupState() const override {
    return SetupState{SetupState::kNone};
  }

  bool Setup(const std::string& ticket_id, const std::string& user) override {
    NOTIMPLEMENTED();
    return false;
  }

  std::string GetCloudId() const override { return std::string(); }
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
