// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SERVICE_ADAPTOR_INTERFACE_H_
#define APMANAGER_SERVICE_ADAPTOR_INTERFACE_H_

#include <string>

#include "apmanager/rpc_interface.h"

namespace apmanager {

class Config;

class ServiceAdaptorInterface {
 public:
  virtual ~ServiceAdaptorInterface() {}

  virtual RPCObjectIdentifier GetRpcObjectIdentifier() = 0;
  virtual void SetConfig(Config* config) = 0;
  virtual void SetState(const std::string& state) = 0;
};

}  // namespace apmanager

#endif  // APMANAGER_SERVICE_ADAPTOR_INTERFACE_H_
