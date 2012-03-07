// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ADAPTORS_
#define SHILL_MOCK_ADAPTORS_

#include <string>

#include <gmock/gmock.h>

#include "shill/adaptor_interfaces.h"
#include "shill/error.h"

namespace shill {

// These are the functions that a Device adaptor must support
class DeviceMockAdaptor : public DeviceAdaptorInterface {
 public:
  static const char kRpcId[];
  static const char kRpcConnId[];

  DeviceMockAdaptor();
  virtual ~DeviceMockAdaptor();
  virtual const std::string &GetRpcIdentifier();
  virtual const std::string &GetRpcConnectionIdentifier();

  MOCK_METHOD0(UpdateEnabled, void());
  MOCK_METHOD2(EmitBoolChanged, void(const std::string &name, bool value));
  MOCK_METHOD2(EmitUintChanged, void(const std::string &name, uint32 value));
  MOCK_METHOD2(EmitIntChanged, void(const std::string &name, int value));
  MOCK_METHOD2(EmitStringChanged, void(const std::string &name,
                                       const std::string &value));
  MOCK_METHOD2(EmitStringmapsChanged, void(const std::string &name,
                                           const Stringmaps &value));
  MOCK_METHOD2(EmitKeyValueStoreChanged, void(const std::string &name,
                                              const KeyValueStore &value));

 private:
  const std::string rpc_id_;
  const std::string rpc_conn_id_;
};

// These are the functions that a IPConfig adaptor must support
class IPConfigMockAdaptor : public IPConfigAdaptorInterface {
 public:
  static const char kRpcId[];

  IPConfigMockAdaptor();
  virtual ~IPConfigMockAdaptor();
  virtual const std::string &GetRpcIdentifier();

  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));

 private:
  const std::string rpc_id_;
};

// These are the functions that a Manager adaptor must support
class ManagerMockAdaptor : public ManagerAdaptorInterface {
 public:
  static const char kRpcId[];

  ManagerMockAdaptor();
  virtual ~ManagerMockAdaptor();
  virtual const std::string &GetRpcIdentifier();

  MOCK_METHOD0(UpdateRunning, void(void));
  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));
  MOCK_METHOD2(EmitStringsChanged,
               void(const std::string &, const std::vector<std::string> &));
  MOCK_METHOD2(EmitRpcIdentifierArrayChanged,
               void(const std::string &, const std::vector<std::string> &));

  MOCK_METHOD1(EmitStateChanged, void(const std::string&));

 private:
  const std::string rpc_id_;
};

// These are the functions that a Profile adaptor must support
class ProfileMockAdaptor : public ProfileAdaptorInterface {
 public:
  static const char kRpcId[];

  ProfileMockAdaptor();
  virtual ~ProfileMockAdaptor();
  virtual const std::string &GetRpcIdentifier();

  MOCK_METHOD2(EmitBoolChanged, void(const std::string&, bool));
  MOCK_METHOD2(EmitUintChanged, void(const std::string&, uint32));
  MOCK_METHOD2(EmitIntChanged, void(const std::string&, int));
  MOCK_METHOD2(EmitStringChanged, void(const std::string&, const std::string&));

 private:
  const std::string rpc_id_;
};

// These are the functions that a Task adaptor must support
class RPCTaskMockAdaptor : public RPCTaskAdaptorInterface {
 public:
  static const char kRpcId[];
  static const char kRpcInterfaceId[];
  static const char kRpcConnId[];

  RPCTaskMockAdaptor();
  virtual ~RPCTaskMockAdaptor();

  virtual const std::string &GetRpcIdentifier();
  virtual const std::string &GetRpcInterfaceIdentifier();
  virtual const std::string &GetRpcConnectionIdentifier();

 private:
  const std::string rpc_id_;
  const std::string rpc_interface_id_;
  const std::string rpc_conn_id_;
};

// These are the functions that a Service adaptor must support
class ServiceMockAdaptor : public ServiceAdaptorInterface {
 public:
  static const char kRpcId[];

  ServiceMockAdaptor();
  virtual ~ServiceMockAdaptor();
  virtual const std::string &GetRpcIdentifier();

  MOCK_METHOD0(UpdateConnected, void());
  MOCK_METHOD2(EmitBoolChanged, void(const std::string &name, bool value));
  MOCK_METHOD2(EmitUint8Changed, void(const std::string &name, uint8 value));
  MOCK_METHOD2(EmitUint16Changed, void(const std::string &name, uint16 value));
  MOCK_METHOD2(EmitUintChanged, void(const std::string &name, uint32 value));
  MOCK_METHOD2(EmitIntChanged, void(const std::string &name, int value));
  MOCK_METHOD2(EmitStringChanged,
               void(const std::string &name, const std::string &value));
  MOCK_METHOD2(EmitStringmapChanged,
               void(const std::string &name, const Stringmap &value));

 private:
  const std::string rpc_id_;
};

}  // namespace shill
#endif  // SHILL_MOCK_ADAPTORS_
