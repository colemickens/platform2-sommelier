// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEBUGD_SRC_VM_CONCIERGE_TOOL_H_
#define DEBUGD_SRC_VM_CONCIERGE_TOOL_H_

#include <memory>
#include <string>

#include <base/macros.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <brillo/dbus/dbus_method_response.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

namespace debugd {

// Manages the vm_concierge service.
class VmConciergeTool {
 public:
  explicit VmConciergeTool(scoped_refptr<dbus::Bus> bus);
  ~VmConciergeTool() = default;

  void StartVmConcierge(
      std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response);
  void StopVmConcierge();

 private:
  // Called when the owner of the concierge service changes.
  void HandleNameOwnerChanged(const std::string& old_owner,
                              const std::string& new_owner);

  // Connection to the system bus.
  scoped_refptr<dbus::Bus> bus_;

  // Proxy to the concierge dbus service.  Owned by |bus_|.
  dbus::ObjectProxy* concierge_proxy_;

  // Whether the concierge service is running.
  bool running_;

  base::WeakPtrFactory<VmConciergeTool> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(VmConciergeTool);
};

}  // namespace debugd

#endif  // DEBUGD_SRC_VM_CONCIERGE_TOOL_H_
