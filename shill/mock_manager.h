// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_MANAGER_
#define SHILL_MOCK_MANAGER_

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/manager.h"

namespace shill {

class MockManager : public Manager {
 public:
  MockManager(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Metrics *metrics,
              GLib *glib);
  virtual ~MockManager();

  MOCK_METHOD0(device_info, DeviceInfo *());
  MOCK_METHOD0(modem_info, ModemInfo *());
  MOCK_METHOD0(vpn_provider, VPNProvider *());
  MOCK_METHOD0(mutable_store, PropertyStore *());
  MOCK_CONST_METHOD0(store, const PropertyStore &());
  MOCK_CONST_METHOD0(run_path, const FilePath &());
  MOCK_METHOD0(Start, void());
  MOCK_METHOD1(RegisterDevice, void(const DeviceRefPtr &to_manage));
  MOCK_METHOD1(DeregisterDevice, void(const DeviceRefPtr &to_forget));
  MOCK_METHOD1(HasService, bool(const ServiceRefPtr &to_manage));
  MOCK_METHOD1(RegisterService, void(const ServiceRefPtr &to_manage));
  MOCK_METHOD1(UpdateService, void(const ServiceRefPtr &to_update));
  MOCK_METHOD1(DeregisterService, void(const ServiceRefPtr &to_forget));
  MOCK_METHOD1(RecheckPortalOnService, void(const ServiceRefPtr &service));
  MOCK_METHOD2(HandleProfileEntryDeletion,
               bool (const ProfileRefPtr &profile,
                     const std::string &entry_name));
  MOCK_CONST_METHOD0(GetDefaultService, ServiceRefPtr());
  MOCK_METHOD0(UpdateEnabledTechnologies, void());
  MOCK_METHOD1(IsPortalDetectionEnabled, bool(Technology::Identifier tech));
  MOCK_CONST_METHOD1(IsServiceEphemeral,
                     bool(const ServiceConstRefPtr &service));
  MOCK_CONST_METHOD0(GetPortalCheckURL, const std::string &());
  MOCK_CONST_METHOD0(GetPortalCheckInterval, int());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockManager);
};

}  // namespace shill

#endif  // SHILL_MOCK_MANAGER_
