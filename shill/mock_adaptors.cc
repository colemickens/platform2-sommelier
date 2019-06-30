// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"

namespace shill {

// static
const RpcIdentifier DeviceMockAdaptor::kRpcId("/device_rpc");
// static
const RpcIdentifier DeviceMockAdaptor::kRpcConnId =
  RpcIdentifier("/device_rpc_conn");

DeviceMockAdaptor::DeviceMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_conn_id_(kRpcConnId) {
}

DeviceMockAdaptor::~DeviceMockAdaptor() = default;

RpcIdentifier DeviceMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const RpcIdentifier IPConfigMockAdaptor::kRpcId = "/ipconfig_rpc";

IPConfigMockAdaptor::IPConfigMockAdaptor() : rpc_id_(kRpcId) {}

IPConfigMockAdaptor::~IPConfigMockAdaptor() = default;

RpcIdentifier IPConfigMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const RpcIdentifier ManagerMockAdaptor::kRpcId = "/manager_rpc";

ManagerMockAdaptor::ManagerMockAdaptor() : rpc_id_(kRpcId) {}

ManagerMockAdaptor::~ManagerMockAdaptor() = default;

RpcIdentifier ManagerMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const RpcIdentifier ProfileMockAdaptor::kRpcId = "/profile_rpc";

ProfileMockAdaptor::ProfileMockAdaptor() : rpc_id_(kRpcId) {}

ProfileMockAdaptor::~ProfileMockAdaptor() = default;

RpcIdentifier ProfileMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

// static
const RpcIdentifier RpcTaskMockAdaptor::kRpcId = "/rpc_task_rpc";
const RpcIdentifier RpcTaskMockAdaptor::kRpcConnId = "/rpc_task_rpc_conn";

RpcTaskMockAdaptor::RpcTaskMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_conn_id_(kRpcConnId) {}

RpcTaskMockAdaptor::~RpcTaskMockAdaptor() = default;

RpcIdentifier RpcTaskMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}
RpcIdentifier RpcTaskMockAdaptor::GetRpcConnectionIdentifier() const {
  return rpc_conn_id_;
}

// static
const RpcIdentifier ServiceMockAdaptor::kRpcId = "/service_rpc";

ServiceMockAdaptor::ServiceMockAdaptor() : rpc_id_(kRpcId) {}

ServiceMockAdaptor::~ServiceMockAdaptor() = default;

RpcIdentifier ServiceMockAdaptor::GetRpcIdentifier() const {
  return rpc_id_;
}

#ifndef DISABLE_VPN
ThirdPartyVpnMockAdaptor::ThirdPartyVpnMockAdaptor() = default;

ThirdPartyVpnMockAdaptor::~ThirdPartyVpnMockAdaptor() = default;
#endif

}  // namespace shill
