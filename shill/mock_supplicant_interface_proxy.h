// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_SUPPLICANT_INTERFACE_PROXY_H_
#define MOCK_SUPPLICANT_INTERFACE_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/dbus_variant_gmock_printer.h"
#include "shill/refptr_types.h"
#include "shill/supplicant_interface_proxy_interface.h"

namespace shill {

class MockSupplicantInterfaceProxy : public SupplicantInterfaceProxyInterface {
 public:
  explicit MockSupplicantInterfaceProxy();
  virtual ~MockSupplicantInterfaceProxy();

  MOCK_METHOD1(AddNetwork, ::DBus::Path(
      const std::map<std::string, ::DBus::Variant> &args));
  MOCK_METHOD0(EnableHighBitrates, void());
  MOCK_METHOD0(EAPLogoff, void());
  MOCK_METHOD0(EAPLogon, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD1(FlushBSS, void(const uint32_t &age));
  MOCK_METHOD3(NetworkReply, void(const ::DBus::Path &network,
                                  const std::string &field,
                                  const std::string &value));
  MOCK_METHOD0(Reassociate, void());
  MOCK_METHOD0(RemoveAllNetworks, void());
  MOCK_METHOD1(RemoveNetwork, void(const ::DBus::Path &network));
  MOCK_METHOD1(Scan,
               void(const std::map<std::string, ::DBus::Variant> &args));
  MOCK_METHOD1(SelectNetwork, void(const ::DBus::Path &network));
  MOCK_METHOD1(SetFastReauth, void(bool enabled));
  MOCK_METHOD1(SetScanInterval, void(int32_t seconds));
  MOCK_METHOD1(SetDisableHighBitrates, void(bool disable_high_bitrates));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // MOCK_SUPPLICANT_INTERFACE_PROXY_H_
