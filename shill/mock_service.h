// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_SERVICE_
#define SHILL_MOCK_SERVICE_

#include <base/memory/ref_counted.h>
#include <gmock/gmock.h>

#include "shill/refptr_types.h"
#include "shill/service.h"

namespace shill {

class MockService : public Service {
 public:
  // A constructor for the Service object
  MockService(ControlInterface *control_interface,
              EventDispatcher *dispatcher,
              Manager *manager);
  virtual ~MockService();

  MOCK_METHOD1(Connect, void(Error *error));
  MOCK_METHOD0(Disconnect, void());
  MOCK_METHOD0(CalculateState, std::string());
  MOCK_CONST_METHOD1(TechnologyIs,
                     bool(const Technology::Identifier technology));
  MOCK_METHOD1(SetState, void(ConnectState state));
  MOCK_CONST_METHOD0(state, ConnectState());
  MOCK_METHOD1(SetFailure, void(ConnectFailure failure));
  MOCK_CONST_METHOD0(failure, ConnectFailure());
  MOCK_METHOD0(GetDeviceRpcId, std::string());
  MOCK_CONST_METHOD0(GetRpcIdentifier, std::string());
  MOCK_CONST_METHOD0(GetStorageIdentifier, std::string());
  MOCK_METHOD1(Load, bool(StoreInterface *store_interface));
  MOCK_METHOD1(Save, bool(StoreInterface *store_interface));

  // Set a string for this Service via |store|.  Can be wired to Save() for
  // test purposes.
  bool FauxSave(StoreInterface *store);

 private:
  DISALLOW_COPY_AND_ASSIGN(MockService);
};

}  // namespace shill

#endif  // SHILL_MOCK_SERVICE_
