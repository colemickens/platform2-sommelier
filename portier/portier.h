// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PORTIER_PORTIER_H_
#define PORTIER_PORTIER_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <dbus/bus.h>

#include "portier/status.h"

namespace portier {

// Portier interface.  This class acts as a D-Bus proxy interface for
// communicating with the Portier daemon.  This interface is intended
// to be used synchronously.
class Portier {
 public:
  static std::unique_ptr<Portier> Create();
  ~Portier() = default;

  Status BindInterface(const std::string& if_name);

  Status ReleaseInterface(const std::string& if_name);

  Status CreateProxyGroup(const std::string& pg_name);

  Status ReleaseProxyGroup(const std::string& pg_name);

  Status AddToGroup(const std::string& if_name,
                    const std::string& pg_name,
                    bool as_upstream = false);

  Status RemoveFromGroup(const std::string& if_name);

  Status SetUpstream(const std::string& if_name);

  Status UnsetUpstream(const std::string& pg_name);

 private:
  Portier() = default;

  bool Init();

  dbus::ObjectProxy* GetProxy();

  scoped_refptr<dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(Portier);
};

}  // namespace portier

#endif  // PORTIER_PORTIER_H_
