// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SERVICE_
#define SHILL_MOCK_SERVICE_

#include <string>

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/connection.h"
#include "shill/refptr_types.h"
#include "shill/service.h"
#include "shill/technology.h"

namespace shill {

class MockService : public Service {
 public:
  // A constructor for the Service object
  MockService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Metrics *metrics,
              Manager *manager);
  virtual ~MockService();

  MOCK_METHOD0(AutoConnect, void());
  MOCK_METHOD1(Connect, void(Error *error));
  MOCK_METHOD1(Disconnect, void(Error *error));
  MOCK_METHOD2(DisconnectWithFailure, void(Service::ConnectFailure failure,
                                           Error *error));
  MOCK_METHOD1(UserInitiatedDisconnect, void(Error *error));
  MOCK_METHOD1(CalculateState, std::string(Error *error));
  MOCK_CONST_METHOD0(state, ConnectState());
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
  MOCK_CONST_METHOD0(IsFailed, bool());
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_CONST_METHOD0(failure, ConnectFailure());
  MOCK_METHOD1(GetDeviceRpcId, std::string(Error *error));
  MOCK_CONST_METHOD0(GetRpcIdentifier, std::string());
  MOCK_CONST_METHOD0(GetStorageIdentifier, std::string());
  MOCK_METHOD1(Load, bool(StoreInterface *store_interface));
  MOCK_METHOD0(Unload, bool());
  MOCK_METHOD1(Save, bool(StoreInterface *store_interface));
  MOCK_METHOD0(SaveToCurrentProfile, void());
  MOCK_METHOD2(Configure, void(const KeyValueStore &args, Error *error));
  MOCK_CONST_METHOD0(IsPortalDetectionDisabled, bool());
  MOCK_CONST_METHOD0(IsPortalDetectionAuto, bool());
  MOCK_CONST_METHOD0(IsRemembered, bool());
  MOCK_CONST_METHOD0(HasProxyConfig, bool());
  MOCK_METHOD1(SetConnection, void(const ConnectionRefPtr &connection));
  MOCK_CONST_METHOD0(connection, const ConnectionRefPtr &());
  MOCK_CONST_METHOD0(explicitly_disconnected, bool());
  MOCK_CONST_METHOD0(technology, Technology::Identifier());
  // Set a string for this Service via |store|.  Can be wired to Save() for
  // test purposes.
  bool FauxSave(StoreInterface *store);
  // Sets the connection reference returned by default when connection()
  // is called.
  void set_mock_connection(const ConnectionRefPtr &connection) {
    mock_connection_ = connection;
  }

 private:
  ConnectionRefPtr mock_connection_;
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

}  // namespace shill

#endif  // SHILL_MOCK_SERVICE_
