// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WIMAX_MANAGER_DEVICE_H_
#define WIMAX_MANAGER_DEVICE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <base/values.h>

#include "wimax_manager/dbus_adaptable.h"
#include "wimax_manager/network.h"

namespace wimax_manager {

class DeviceDBusAdaptor;

class Device : public DBusAdaptable<Device, DeviceDBusAdaptor> {
 public:
  Device(uint8 index, const std::string &name);
  virtual ~Device();

  virtual bool Enable() = 0;
  virtual bool Disable() = 0;
  virtual bool ScanNetworks() = 0;
  virtual bool Connect(const Network &network,
                       const base::DictionaryValue &parameters) = 0;
  virtual bool Disconnect() = 0;

  uint8 index() const { return index_; }
  const std::string &name() const { return name_; }
  const std::vector<Network *> &networks() const { return networks_.get(); }

 protected:
  void UpdateNetworks();

  ScopedVector<Network> *mutable_networks() { return &networks_; }

 private:
  uint8 index_;
  std::string name_;
  ScopedVector<Network> networks_;

  DISALLOW_COPY_AND_ASSIGN(Device);
};

}  // namespace wimax_manager

#endif  // WIMAX_MANAGER_DEVICE_H_
