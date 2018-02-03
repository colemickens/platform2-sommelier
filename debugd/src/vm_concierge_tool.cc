// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/vm_concierge_tool.h"

#include <utility>

#include <base/bind.h>
#include <base/bind_helpers.h>
#include <base/location.h>
#include <brillo/process.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/object_path.h>

using std::string;

namespace debugd {
namespace {

// Posted to the MessageLoop by dbus::ObjectProxy once the concierge
// service is available on dbus.
void ServiceReady(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response,
    bool is_available) {
  response->Return(is_available);
}

}  // namespace

VmConciergeTool::VmConciergeTool(scoped_refptr<dbus::Bus> bus)
    : bus_(bus), running_(false) {
  CHECK(bus_);

  concierge_proxy_ = bus_->GetObjectProxy(
      vm_tools::concierge::kVmConciergeServiceName,
      dbus::ObjectPath(vm_tools::concierge::kVmConciergeServicePath));
  concierge_proxy_->SetNameOwnerChangedCallback(base::Bind(
      &VmConciergeTool::HandleNameOwnerChanged, weak_factory_.GetWeakPtr()));
}

void VmConciergeTool::StartVmConcierge(
    std::unique_ptr<brillo::dbus_utils::DBusMethodResponse<bool>> response) {
  if (running_) {
    response->Return(true);
    return;
  }

  LOG(INFO) << "Starting vm_concierge";
  brillo::ProcessImpl concierge;
  concierge.AddArg("/sbin/start");
  concierge.AddArg("vm_concierge");

  concierge.Run();

  // dbus::ObjectProxy keeps a list of WaitForServiceToBeAvailable
  // callbacks so we can safely call this multiple times if there are multiple
  // pending dbus requests.
  concierge_proxy_->WaitForServiceToBeAvailable(
      base::Bind(&ServiceReady, base::Passed(std::move(response))));
}

void VmConciergeTool::StopVmConcierge() {
  if (!running_) {
    // Nothing to do.
    return;
  }

  LOG(INFO) << "Stopping vm_concierge";

  brillo::ProcessImpl concierge;
  concierge.AddArg("/sbin/stop");
  concierge.AddArg("vm_concierge");

  concierge.Run();
}

void VmConciergeTool::HandleNameOwnerChanged(const string& old_owner,
                                             const string& new_owner) {
  running_ = !new_owner.empty();
}

}  // namespace debugd
