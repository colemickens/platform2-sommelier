// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SHILL_STUB_PROXY_H_
#define APMANAGER_SHILL_STUB_PROXY_H_

#include <string>

#include <base/macros.h>

#include "apmanager/shill_proxy_interface.h"

namespace apmanager {

class ShillStubProxy : public ShillProxyInterface {
 public:
  ShillStubProxy();
  ~ShillStubProxy() override;

  // Implementation of ShillProxyInterface.
  bool ClaimInterface(const std::string& interface_name) override;
  bool ReleaseInterface(const std::string& interface_name) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShillStubProxy);
};

}  // namespace apmanager

#endif  // APMANAGER_SHILL_STUB_PROXY_H_
