// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_PROVIDER_
#define SHILL_VPN_PROVIDER_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/refptr_types.h"

namespace shill {

class ControlInterface;
class Error;
class EventDispatcher;
class KeyValueStore;
class Manager;
class Metrics;

class VPNProvider {
 public:
  VPNProvider(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Metrics *metrics,
              Manager *manager);
  ~VPNProvider();

  void Start();
  void Stop();

  VPNServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Offers an unclaimed interface to VPN services.  Returns true if this
  // device has been accepted by a service.
  bool OnDeviceInfoAvailable(const std::string &link_name, int interface_index);

 private:
  FRIEND_TEST(VPNProviderTest, OnDeviceInfoAvailable);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  std::vector<VPNServiceRefPtr> services_;

  DISALLOW_COPY_AND_ASSIGN(VPNProvider);
};

}  // namespace shill

#endif  // SHILL_VPN_PROVIDER_
