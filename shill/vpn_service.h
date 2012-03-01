// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_VPN_SERVICE_
#define SHILL_VPN_SERVICE_

#include <base/memory/scoped_ptr.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST

#include "shill/service.h"

namespace shill {

class VPNDriver;

class VPNService : public Service {
 public:
  VPNService(ControlInterface *control_interface,
             EventDispatcher *dispatcher,
             Metrics *metrics,
             Manager *manager,
             VPNDriver *driver);  // Takes ownership of |driver|.
  virtual ~VPNService();

  // Inherited from Service.
  virtual void Connect(Error *error);
  virtual std::string GetStorageIdentifier() const;
  VPNDriver *driver() { return driver_.get(); }

 private:
  FRIEND_TEST(VPNServiceTest, GetDeviceRpcId);

  virtual std::string GetDeviceRpcId(Error *error);

  scoped_ptr<VPNDriver> driver_;

  DISALLOW_COPY_AND_ASSIGN(VPNService);
};

}  // namespace shill

#endif  // SHILL_VPN_SERVICE_
