// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_SHILL_MANAGER_H_
#define APMANAGER_SHILL_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include <base/macros.h>
#include <base/memory/weak_ptr.h>

#include "apmanager/shill_proxy_interface.h"

namespace apmanager {

class ControlInterface;

class ShillManager {
 public:
  ShillManager();
  virtual ~ShillManager();

  void Init(ControlInterface* control_interface);

  // Claim the given interface |interface_name| from shill.
  virtual void ClaimInterface(const std::string& interface_name);
  // Release the given interface |interface_name| to shill.
  virtual void ReleaseInterface(const std::string& interface_name);

 private:
  void OnShillServiceAppeared();
  void OnShillServiceVanished();

  std::unique_ptr<ShillProxyInterface> shill_proxy_;
  // List of interfaces apmanager have claimed.
  std::set<std::string> claimed_interfaces_;

  base::WeakPtrFactory<ShillManager> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ShillManager);
};

}  // namespace apmanager

#endif  // APMANAGER_SHILL_MANAGER_H_
