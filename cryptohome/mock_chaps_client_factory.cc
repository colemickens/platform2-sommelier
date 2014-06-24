// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mock_chaps_client_factory.h"

#include <chaps/token_manager_client_mock.h>
#include <gmock/gmock.h>

using testing::_;
using testing::DoAll;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;

namespace cryptohome {

MockChapsClientFactory::MockChapsClientFactory() {}
MockChapsClientFactory::~MockChapsClientFactory() {}

chaps::TokenManagerClient* MockChapsClientFactory::New() {
  NiceMock<chaps::TokenManagerClientMock>* mock =
      new NiceMock<chaps::TokenManagerClientMock>();
  ON_CALL(*mock, LoadToken(_, _, _, _, _))
      .WillByDefault(DoAll(SetArgumentPointee<4>(0), Return(true)));
  return mock;
}

}  // namespace cryptohome
