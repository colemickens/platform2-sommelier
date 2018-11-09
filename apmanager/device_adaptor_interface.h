// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_DEVICE_ADAPTOR_INTERFACE_H_
#define APMANAGER_DEVICE_ADAPTOR_INTERFACE_H_

#include <string>

namespace apmanager {

class DeviceAdaptorInterface {
 public:
  virtual ~DeviceAdaptorInterface() {}

  virtual void SetDeviceName(const std::string& device_name) = 0;
  virtual std::string GetDeviceName() = 0;
  virtual void SetPreferredApInterface(const std::string& interface_name) = 0;
  virtual std::string GetPreferredApInterface() = 0;
  virtual void SetInUse(bool in_use) = 0;
  virtual bool GetInUse() = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_DEVICE_ADAPTOR_INTERFACE_H_
