// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOCK_SUPPLICANT_PROCESS_PROXY_H_
#define MOCK_SUPPLICANT_PROCESS_PROXY_H_

#include <map>
#include <string>

#include <base/basictypes.h>
#include <gmock/gmock.h>

#include "shill/supplicant_process_proxy_interface.h"

namespace shill {

class MockSupplicantProcessProxy : public SupplicantProcessProxyInterface {
 public:
  MockSupplicantProcessProxy();
  virtual ~MockSupplicantProcessProxy();

  MOCK_METHOD1(CreateInterface,
               ::DBus::Path(
                   const std::map<std::string, ::DBus::Variant> &args));
  MOCK_METHOD1(GetInterface, ::DBus::Path(const std::string &ifname));
  MOCK_METHOD1(RemoveInterface, void(const ::DBus::Path &path));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantProcessProxy);
};

}  // namespace shill

#endif  // MOCK_SUPPLICANT_PROCESS_PROXY_H_
