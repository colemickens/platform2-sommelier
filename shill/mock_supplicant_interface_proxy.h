// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_SUPPLICANT_INTERFACE_PROXY_H_
#define MOCK_SUPPLICANT_INTERFACE_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/refptr_types.h"
#include "shill/supplicant_interface_proxy_interface.h"

namespace shill {

class MockSupplicantInterfaceProxy : public SupplicantInterfaceProxyInterface {
 public:
  explicit MockSupplicantInterfaceProxy(const WiFiRefPtr &wifi);
  virtual ~MockSupplicantInterfaceProxy();

  MOCK_METHOD1(AddNetwork, ::DBus::Path(
      const std::map<std::string, ::DBus::Variant> &args));
  MOCK_METHOD0(ClearCachedCredentials, void());
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD1(FlushBSS, void(const uint32_t &age));
  MOCK_METHOD0(Reassociate, void());
  MOCK_METHOD0(RemoveAllNetworks, void());
  MOCK_METHOD1(RemoveNetwork, void(const ::DBus::Path &network));
  MOCK_METHOD1(Scan,
               void(const std::map<std::string, ::DBus::Variant> &args));
  MOCK_METHOD1(SelectNetwork, void(const ::DBus::Path &network));
  MOCK_METHOD1(SetFastReauth, void(bool enabled));
  MOCK_METHOD1(SetScanInterval, void(int32_t seconds));

 private:
  // wifi_ is not used explicitly but its presence here tests that WiFi::Stop
  // properly removes cyclic references.
  WiFiRefPtr wifi_;

  DISALLOW_COPY_AND_ASSIGN(MockSupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // MOCK_SUPPLICANT_INTERFACE_PROXY_H_
