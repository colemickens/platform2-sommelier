// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_CONTROL_
#define SHILL_MOCK_CONTROL_

#include <base/basictypes.h>

#include "shill/control_interface.h"

namespace shill {
// An implementation of the Shill RPC-channel-interface-factory interface that
// returns mocks.
class MockControl : public ControlInterface {
 public:
  MockControl();
  virtual ~MockControl();

  // Each of these can be called once.  Ownership of the appropriate
  // interface pointer is given up upon call.
  virtual DeviceAdaptorInterface *CreateDeviceAdaptor(Device *device);
  virtual IPConfigAdaptorInterface *CreateIPConfigAdaptor(IPConfig *config);
  virtual ManagerAdaptorInterface *CreateManagerAdaptor(Manager *manager);
  virtual ProfileAdaptorInterface *CreateProfileAdaptor(Profile *profile);
  virtual RPCTaskAdaptorInterface *CreateRPCTaskAdaptor(RPCTask *task);
  virtual ServiceAdaptorInterface *CreateServiceAdaptor(Service *service);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockControl);
};

}  // namespace shill

#endif  // SHILL_MOCK_CONTROL_
