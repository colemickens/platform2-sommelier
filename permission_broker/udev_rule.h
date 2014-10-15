// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PERMISSION_BROKER_UDEV_RULE_H_
#define PERMISSION_BROKER_UDEV_RULE_H_

#include <string>

#include "permission_broker/rule.h"

struct udev;
struct udev_device;
struct udev_enumerate;

namespace permission_broker {

class UdevRule : public Rule {
 public:
  static std::string GetDevNodeGroupName(udev_device* device);

  explicit UdevRule(const std::string& name);
  ~UdevRule() override;

  virtual Result ProcessDevice(udev_device* device) = 0;

  Result Process(const std::string& path, int interface_id) override;

 private:
  struct udev* const udev_;
  struct udev_enumerate* const enumerate_;

  DISALLOW_COPY_AND_ASSIGN(UdevRule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_UDEV_RULE_H_
