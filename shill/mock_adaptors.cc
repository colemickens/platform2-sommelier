// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"

#include <string>

using std::string;

namespace shill {

// static
const char DeviceMockAdaptor::kRpcId[] = "/device_rpc";
// static
const char DeviceMockAdaptor::kRpcConnId[] = "/device_rpc_conn";

DeviceMockAdaptor::DeviceMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_conn_id_(kRpcConnId) {
}

DeviceMockAdaptor::~DeviceMockAdaptor() = default;

const string& DeviceMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const char IPConfigMockAdaptor::kRpcId[] = "/ipconfig_rpc";

IPConfigMockAdaptor::IPConfigMockAdaptor() : rpc_id_(kRpcId) {}

IPConfigMockAdaptor::~IPConfigMockAdaptor() = default;

const string& IPConfigMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const char ManagerMockAdaptor::kRpcId[] = "/manager_rpc";

ManagerMockAdaptor::ManagerMockAdaptor() : rpc_id_(kRpcId) {}

ManagerMockAdaptor::~ManagerMockAdaptor() = default;

const string& ManagerMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const char ProfileMockAdaptor::kRpcId[] = "/profile_rpc";

ProfileMockAdaptor::ProfileMockAdaptor() : rpc_id_(kRpcId) {}

ProfileMockAdaptor::~ProfileMockAdaptor() = default;

const string& ProfileMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const char RpcTaskMockAdaptor::kRpcId[] = "/rpc_task_rpc";
const char RpcTaskMockAdaptor::kRpcConnId[] = "/rpc_task_rpc_conn";

RpcTaskMockAdaptor::RpcTaskMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_conn_id_(kRpcConnId) {}

RpcTaskMockAdaptor::~RpcTaskMockAdaptor() = default;

const string& RpcTaskMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}
const string& RpcTaskMockAdaptor::GetRpcConnectionIdentifier() const {
  return rpc_conn_id_;
}

// static
const char ServiceMockAdaptor::kRpcId[] = "/service_rpc";

ServiceMockAdaptor::ServiceMockAdaptor() : rpc_id_(kRpcId) {}

ServiceMockAdaptor::~ServiceMockAdaptor() = default;

const string& ServiceMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

#ifndef DISABLE_VPN
ThirdPartyVpnMockAdaptor::ThirdPartyVpnMockAdaptor() = default;

ThirdPartyVpnMockAdaptor::~ThirdPartyVpnMockAdaptor() = default;
#endif

}  // namespace shill
