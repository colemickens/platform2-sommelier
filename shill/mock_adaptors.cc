// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"

#include <string>

namespace shill {

// static
const char DeviceMockAdaptor::kRpcId[] = "/device-rpc/";
// static
const char DeviceMockAdaptor::kRpcConnId[] = "/device-rpc-conn/";

DeviceMockAdaptor::DeviceMockAdaptor()
    : rpc_id(kRpcId),
      rpc_conn_id(kRpcConnId) {
}

DeviceMockAdaptor::~DeviceMockAdaptor() {}

const std::string &DeviceMockAdaptor::GetRpcIdentifier() { return rpc_id; }

const std::string &DeviceMockAdaptor::GetRpcConnectionIdentifier() {
  return rpc_conn_id;
}

// static
const char IPConfigMockAdaptor::kRpcId[] = "/ipconfig-rpc/";

IPConfigMockAdaptor::IPConfigMockAdaptor() : rpc_id(kRpcId) {}

IPConfigMockAdaptor::~IPConfigMockAdaptor() {}

const std::string &IPConfigMockAdaptor::GetRpcIdentifier() { return rpc_id; }

// static
const char ManagerMockAdaptor::kRpcId[] = "/manager-rpc/";

ManagerMockAdaptor::ManagerMockAdaptor() : rpc_id(kRpcId) {}

ManagerMockAdaptor::~ManagerMockAdaptor() {}

const std::string &ManagerMockAdaptor::GetRpcIdentifier() { return rpc_id; }

// static
const char ProfileMockAdaptor::kRpcId[] = "/profile-rpc/";

ProfileMockAdaptor::ProfileMockAdaptor() : rpc_id(kRpcId) {}

ProfileMockAdaptor::~ProfileMockAdaptor() {}

const std::string &ProfileMockAdaptor::GetRpcIdentifier() { return rpc_id; }

// static
const char ServiceMockAdaptor::kRpcId[] = "/service-rpc/";

ServiceMockAdaptor::ServiceMockAdaptor() : rpc_id(kRpcId) {}

ServiceMockAdaptor::~ServiceMockAdaptor() {}

const std::string &ServiceMockAdaptor::GetRpcIdentifier() { return rpc_id; }
}  // namespace shill
