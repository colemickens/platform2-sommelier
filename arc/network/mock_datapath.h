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

  MOCK_METHOD(bool,
              AddBridge,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, RemoveBridge, (const std::string&), (override));
  MOCK_METHOD(std::string,
              AddVirtualBridgedInterface,
              (const std::string&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, RemoveInterface, (const std::string&), (override));
  MOCK_METHOD(
      bool,
      AddInterfaceToContainer,
      (int, const std::string&, const std::string&, const std::string&, bool),
      (override));
  MOCK_METHOD(bool, AddLegacyIPv4DNAT, (const std::string&), (override));
  MOCK_METHOD(void, RemoveLegacyIPv4DNAT, (), (override));
  MOCK_METHOD(bool,
              AddInboundIPv4DNAT,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void,
              RemoveInboundIPv4DNAT,
              (const std::string&, const std::string&),
              (override));
  MOCK_METHOD(bool, AddOutboundIPv4, (const std::string&), (override));
  MOCK_METHOD(void, RemoveOutboundIPv4, (const std::string&), (override));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDatapath);
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_MOCK_DATAPATH_H_
