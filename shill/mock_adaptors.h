// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ADAPTORS_H_
#define SHILL_MOCK_ADAPTORS_H_

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include "shill/adaptor_interfaces.h"
#include "shill/error.h"
#include "shill/key_value_store.h"

namespace shill {

// These are the functions that a Device adaptor must support
class DeviceMockAdaptor : public DeviceAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;
  static const RpcIdentifier kRpcConnId;

  DeviceMockAdaptor();
  ~DeviceMockAdaptor() override;
  RpcIdentifier GetRpcIdentifier() const override;

  MOCK_METHOD2(EmitBoolChanged, void(const std::string& name, bool value));
  MOCK_METHOD2(EmitUintChanged, void(const std::string& name, uint32_t value));
  MOCK_METHOD2(EmitUint16Changed,
               void(const std::string& name, uint16_t value));
  MOCK_METHOD2(EmitIntChanged, void(const std::string& name, int value));
  MOCK_METHOD2(EmitStringChanged, void(const std::string& name,
                                       const std::string& value));
  MOCK_METHOD2(EmitStringmapChanged, void(const std::string& name,
                                          const Stringmap& value));
  MOCK_METHOD2(EmitStringmapsChanged, void(const std::string& name,
                                           const Stringmaps& value));
  MOCK_METHOD2(EmitStringsChanged, void(const std::string& name,
                                        const Strings& value));
  MOCK_METHOD2(EmitKeyValueStoreChanged, void(const std::string& name,
                                              const KeyValueStore& value));
  MOCK_METHOD2(EmitRpcIdentifierChanged,
               void(const std::string& name,
                    const RpcIdentifier& value));
  MOCK_METHOD2(EmitRpcIdentifierArrayChanged,
               void(const std::string& name,
                    const std::vector<RpcIdentifier>& value));

 private:
  const RpcIdentifier rpc_id_;
  const RpcIdentifier rpc_conn_id_;
};

// These are the functions that a IPConfig adaptor must support
class IPConfigMockAdaptor : public IPConfigAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;

  IPConfigMockAdaptor();
  ~IPConfigMockAdaptor() override;
  RpcIdentifier GetRpcIdentifier() const override;

  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32_t));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));
  MOCK_METHOD2(EmitStringsChanged,
               void(const std::string&, const std::vector<std::string>&));

 private:
  const RpcIdentifier rpc_id_;
};

// These are the functions that a Manager adaptor must support
class ManagerMockAdaptor : public ManagerAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;

  ManagerMockAdaptor();
  ~ManagerMockAdaptor() override;
  RpcIdentifier GetRpcIdentifier() const override;

  MOCK_METHOD1(RegisterAsync,
               void(const base::Callback<void(bool)>& completion_callback));
  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32_t));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));
  MOCK_METHOD2(EmitStringsChanged,
               void(const std::string&, const std::vector<std::string>&));
  MOCK_METHOD2(EmitRpcIdentifierChanged,
               void(const std::string&, const RpcIdentifier&));
  MOCK_METHOD2(EmitRpcIdentifierArrayChanged,
               void(const std::string&, const std::vector<RpcIdentifier>&));

 private:
  const RpcIdentifier rpc_id_;
};

// These are the functions that a Profile adaptor must support
class ProfileMockAdaptor : public ProfileAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;

  ProfileMockAdaptor();
  ~ProfileMockAdaptor() override;
  RpcIdentifier GetRpcIdentifier() const override;

  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32_t));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));

 private:
  const RpcIdentifier rpc_id_;
};

// These are the functions that a Task adaptor must support
class RpcTaskMockAdaptor : public RpcTaskAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;
  static const RpcIdentifier kRpcConnId;

  RpcTaskMockAdaptor();
  ~RpcTaskMockAdaptor() override;

  RpcIdentifier GetRpcIdentifier() const override;
  RpcIdentifier GetRpcConnectionIdentifier() const override;

 private:
  const RpcIdentifier rpc_id_;
  const RpcIdentifier rpc_conn_id_;
};

// These are the functions that a Service adaptor must support
class ServiceMockAdaptor : public ServiceAdaptorInterface {
 public:
  static const RpcIdentifier kRpcId;

  ServiceMockAdaptor();
  ~ServiceMockAdaptor() override;
  RpcIdentifier GetRpcIdentifier() const override;

  MOCK_METHOD2(EmitBoolChanged, void(const std::string& name, bool value));
  MOCK_METHOD2(EmitUint8Changed, void(const std::string& name, uint8_t value));
  MOCK_METHOD2(EmitUint16Changed,
               void(const std::string& name, uint16_t value));
  MOCK_METHOD2(EmitUint16sChanged, void(const std::string& name,
                                        const Uint16s& value));
  MOCK_METHOD2(EmitUintChanged, void(const std::string& name, uint32_t value));
  MOCK_METHOD2(EmitIntChanged, void(const std::string& name, int value));
  MOCK_METHOD2(EmitRpcIdentifierChanged,
               void(const std::string& name, const RpcIdentifier& value));
  MOCK_METHOD2(EmitStringChanged,
               void(const std::string& name, const std::string& value));
  MOCK_METHOD2(EmitStringmapChanged,
               void(const std::string& name, const Stringmap& value));

 private:
  const RpcIdentifier rpc_id_;
};

#ifndef DISABLE_VPN
class ThirdPartyVpnMockAdaptor : public ThirdPartyVpnAdaptorInterface {
 public:
  ThirdPartyVpnMockAdaptor();
  ~ThirdPartyVpnMockAdaptor() override;

  MOCK_METHOD1(EmitPacketReceived, void(const std::vector<uint8_t>& packet));

  MOCK_METHOD1(EmitPlatformMessage, void(uint32_t message));
};
#endif

}  // namespace shill

#endif  // SHILL_MOCK_ADAPTORS_H_
