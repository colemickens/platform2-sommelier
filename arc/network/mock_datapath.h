// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_MOCK_DATAPATH_H_
#define ARC_NETWORK_MOCK_DATAPATH_H_

#include <string>

#include <base/macros.h>

#include "arc/network/minijailed_process_runner.h"

namespace arc_networkd {

// ARC networking data path configuration utility.
class MockDatapath : public Datapath {
 public:
  explicit MockDatapath(MinijailedProcessRunner* runner) : Datapath(runner) {}
  ~MockDatapath() = default;

  MOCK_METHOD2(AddBridge,
               bool(const std::string& ifname, const std::string& ipv4_addr));
  MOCK_METHOD1(RemoveBridge, void(const std::string& ifname));

  MOCK_METHOD1(AddLegacyIPv4DNAT, bool(const std::string& ipv4_addr));
  MOCK_METHOD0(RemoveLegacyIPv4DNAT, void());
  MOCK_METHOD2(AddInboundIPv4DNAT,
               bool(const std::string& ifname, const std::string& ipv4_addr));
  MOCK_METHOD2(RemoveInboundIPv4DNAT,
               void(const std::string& ifname, const std::string& ipv4_addr));
  MOCK_METHOD1(AddOutboundIPv4, bool(const std::string& ifname));
  MOCK_METHOD1(RemoveOutboundIPv4, void(const std::string& ifname));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDatapath);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MOCK_DATAPATH_H_
