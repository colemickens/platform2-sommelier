// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_adaptors.h"

#include <string>

using std::string;

namespace shill {

// static
const char DeviceMockAdaptor::kRpcId[] = "/device-rpc/";
// static
const char DeviceMockAdaptor::kRpcConnId[] = "/device-rpc-conn/";

DeviceMockAdaptor::DeviceMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_conn_id_(kRpcConnId) {
}

DeviceMockAdaptor::~DeviceMockAdaptor() {}

const string &DeviceMockAdaptor::GetRpcIdentifier() { return rpc_id_; }

const string &DeviceMockAdaptor::GetRpcConnectionIdentifier() {
  return rpc_conn_id_;
}

// static
const char IPConfigMockAdaptor::kRpcId[] = "/ipconfig-rpc/";

IPConfigMockAdaptor::IPConfigMockAdaptor() : rpc_id_(kRpcId) {}

IPConfigMockAdaptor::~IPConfigMockAdaptor() {}

const string &IPConfigMockAdaptor::GetRpcIdentifier() { return rpc_id_; }

// static
const char ManagerMockAdaptor::kRpcId[] = "/manager-rpc/";

ManagerMockAdaptor::ManagerMockAdaptor() : rpc_id_(kRpcId) {}

ManagerMockAdaptor::~ManagerMockAdaptor() {}

const string &ManagerMockAdaptor::GetRpcIdentifier() { return rpc_id_; }

// static
const char ProfileMockAdaptor::kRpcId[] = "/profile-rpc/";

ProfileMockAdaptor::ProfileMockAdaptor() : rpc_id_(kRpcId) {}

ProfileMockAdaptor::~ProfileMockAdaptor() {}

const string &ProfileMockAdaptor::GetRpcIdentifier() { return rpc_id_; }

// static
const char RPCTaskMockAdaptor::kRpcId[] = "/rpc-task-rpc/";
const char RPCTaskMockAdaptor::kRpcInterfaceId[] = "rpc.task";
const char RPCTaskMockAdaptor::kRpcConnId[] = "/rpc-task-rpc-conn/";

RPCTaskMockAdaptor::RPCTaskMockAdaptor()
    : rpc_id_(kRpcId),
      rpc_interface_id_(kRpcInterfaceId),
      rpc_conn_id_(kRpcConnId) {}

RPCTaskMockAdaptor::~RPCTaskMockAdaptor() {}

const string &RPCTaskMockAdaptor::GetRpcIdentifier() { return rpc_id_; }
const string &RPCTaskMockAdaptor::GetRpcInterfaceIdentifier() {
  return rpc_interface_id_;
}
const string &RPCTaskMockAdaptor::GetRpcConnectionIdentifier() {
  return rpc_conn_id_;
}

// static
const char ServiceMockAdaptor::kRpcId[] = "/service-rpc/";

ServiceMockAdaptor::ServiceMockAdaptor() : rpc_id_(kRpcId) {}

ServiceMockAdaptor::~ServiceMockAdaptor() {}

const string &ServiceMockAdaptor::GetRpcIdentifier() { return rpc_id_; }

}  // namespace shill
