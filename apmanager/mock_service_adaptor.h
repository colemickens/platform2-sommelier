// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_MOCK_SERVICE_ADAPTOR_H_
#define APMANAGER_MOCK_SERVICE_ADAPTOR_H_

#include <string>

#include <base/macros.h>
#include <gmock/gmock.h>

#include "apmanager/service_adaptor_interface.h"

namespace apmanager {

class MockServiceAdaptor : public ServiceAdaptorInterface {
 public:
  MockServiceAdaptor();
  ~MockServiceAdaptor() override;

  MOCK_METHOD0(GetRpcObjectIdentifier, RPCObjectIdentifier());
  MOCK_METHOD1(SetConfig, void(Config* config));
  MOCK_METHOD1(SetState, void(const std::string& state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockServiceAdaptor);
};

}  // namespace apmanager

#endif  // APMANAGER_MOCK_SERVICE_ADAPTOR_H_
