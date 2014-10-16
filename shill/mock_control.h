// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONTROL_H_
#define SHILL_MOCK_CONTROL_H_

#include <base/macros.h>

#include "shill/control_interface.h"

namespace shill {
// An implementation of the Shill RPC-channel-interface-factory interface that
// returns mocks.
class MockControl : public ControlInterface {
 public:
  MockControl();
  ~MockControl() override;

  // Each of these can be called once.  Ownership of the appropriate
  // interface pointer is given up upon call.
  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device);
  virtual IPConfigAdaptorInterface *CreateIPConfigAdaptor(IPConfig *config);
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager);
  virtual ProfileAdaptorInterface *CreateProfileAdaptor(Profile *profile);
  virtual RPCTaskAdaptorInterface *CreateRPCTaskAdaptor(RPCTask *task);
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service);
#ifndef DISABLE_VPN
  virtual ThirdPartyVpnAdaptorInterface *CreateThirdPartyVpnAdaptor(
      ThirdPartyVpnDriver *driver);
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(MockControl);
};

}  // namespace shill

#endif  // SHILL_MOCK_CONTROL_H_
