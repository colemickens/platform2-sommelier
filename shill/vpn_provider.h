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
  virtual ~VPNProvider();

  virtual void Start();
  virtual void Stop();

  VPNServiceRefPtr GetService(const KeyValueStore &args, Error *error);

  // Offers an unclaimed interface to VPN services.  Returns true if this
  // device has been accepted by a service.
  virtual bool OnDeviceInfoAvailable(const std::string &link_name,
                                     int interface_index);

  // Clean up a VPN services that has been unloaded and will be deregistered.
  // This removes the VPN provider's reference to this service in its
  // services_ vector.
  void RemoveService(VPNServiceRefPtr service);

  void CreateServicesFromProfile(ProfileRefPtr profile);

 private:
  FRIEND_TEST(VPNProviderTest, CreateService);
  FRIEND_TEST(VPNProviderTest, OnDeviceInfoAvailable);
  FRIEND_TEST(VPNProviderTest, RemoveService);
  FRIEND_TEST(VPNServiceTest, Unload);

  // Create a service of type |type| and storage identifier |storage_id|
  // and initial parameters |args|.  Returns a service reference pointer
  // to the newly created service, or popuplates |error| with an the error
  // that caused this to fail.
  VPNServiceRefPtr CreateService(const std::string &type,
                                 const std::string &name,
                                 const std::string &storage_id,
                                 Error *error);

  // Find a service of type |type| whose storage identifier is |storage_id|.
  VPNServiceRefPtr FindService(const std::string &type,
                               const std::string &storage_id);

  ControlInterface *control_interface_;
  EventDispatcher *dispatcher_;
  Metrics *metrics_;
  Manager *manager_;
  std::vector<VPNServiceRefPtr> services_;

  DISALLOW_COPY_AND_ASSIGN(VPNProvider);
};

}  // namespace shill

#endif  // SHILL_VPN_PROVIDER_
