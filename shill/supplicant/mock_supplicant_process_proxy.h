// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_MOCK_SUPPLICANT_PROCESS_PROXY_H_
#define SHILL_SUPPLICANT_MOCK_SUPPLICANT_PROCESS_PROXY_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/supplicant/supplicant_process_proxy_interface.h"

namespace shill {

class MockSupplicantProcessProxy : public SupplicantProcessProxyInterface {
 public:
  MockSupplicantProcessProxy();
  ~MockSupplicantProcessProxy() override;

  MOCK_METHOD2(CreateInterface,
               bool(const KeyValueStore& args, RpcIdentifier* rpc_identifier));
  MOCK_METHOD2(GetInterface,
               bool(const std::string& ifname, RpcIdentifier* rpc_identifier));
  MOCK_METHOD1(RemoveInterface, bool(const RpcIdentifier& rpc_identifier));
  MOCK_METHOD1(GetDebugLevel, bool(std::string* level));
  MOCK_METHOD1(SetDebugLevel, bool(const std::string& level));
  MOCK_METHOD0(ExpectDisconnect, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSupplicantProcessProxy);
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_MOCK_SUPPLICANT_PROCESS_PROXY_H_
