// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_FAKE_DEVICE_ADAPTOR_H_
#define APMANAGER_FAKE_DEVICE_ADAPTOR_H_

#include <string>

#include <base/macros.h>

#include "apmanager/device_adaptor_interface.h"

namespace apmanager {

class FakeDeviceAdaptor : public DeviceAdaptorInterface {
 public:
  FakeDeviceAdaptor();
  ~FakeDeviceAdaptor() override;

  void SetDeviceName(const std::string& device_name) override;
  std::string GetDeviceName() override;
  void SetPreferredApInterface(const std::string& interface_name) override;
  std::string GetPreferredApInterface() override;
  void SetInUse(bool in_use) override;
  bool GetInUse() override;

private:
  std::string device_name_;
  std::string preferred_ap_interface_;
  bool in_use_;

  DISALLOW_COPY_AND_ASSIGN(FakeDeviceAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_FAKE_DEVICE_ADAPTOR_H_
