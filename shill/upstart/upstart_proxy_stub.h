// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_UPSTART_UPSTART_PROXY_STUB_H_
#define SHILL_UPSTART_UPSTART_PROXY_STUB_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/upstart/upstart_proxy_interface.h"

namespace shill {

// Stub implementation of UpstartProxyInterface.
class UpstartProxyStub : public UpstartProxyInterface {
 public:
  UpstartProxyStub();
  ~UpstartProxyStub() override = default;

  // Inherited from UpstartProxyInterface.
  void EmitEvent(const std::string& name,
                 const std::vector<std::string>& env,
                 bool wait) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpstartProxyStub);
};

}  // namespace shill

#endif  // SHILL_UPSTART_UPSTART_PROXY_STUB_H_
