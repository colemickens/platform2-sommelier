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
  UdevRule(const std::string& name);
  virtual ~UdevRule();

  virtual Result ProcessDevice(struct udev_device *device) = 0;

  virtual Result Process(const std::string& path);

 private:
  struct udev *const udev_;
  struct udev_enumerate *const enumerate_;

  DISALLOW_COPY_AND_ASSIGN(UdevRule);
};

}  // namespace permission_broker

#endif  // PERMISSION_BROKER_UDEV_RULE_H_
