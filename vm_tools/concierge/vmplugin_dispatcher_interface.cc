// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/guid.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include <vm_plugin_dispatcher/proto_bindings/vm_plugin_dispatcher.pb.h>

#include "vm_tools/concierge/vmplugin_dispatcher_interface.h"

namespace {

constexpr char kVmpluginImageDir[] = "/run/pvm-images";

}  // namespace

namespace vm_tools {
namespace concierge {

dbus::ObjectProxy* GetVmpluginServiceProxy(scoped_refptr<dbus::Bus> bus) {
  return bus->GetObjectProxy(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherServiceName,
      dbus::ObjectPath(
          vm_tools::plugin_dispatcher::kVmPluginDispatcherServicePath));
}

bool VmpluginRegisterVm(dbus::ObjectProxy* proxy,
                        const std::string& owner_id,
                        const base::FilePath& image_path) {
  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kRegisterVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::RegisterVmRequest request;

  request.set_owner_id(owner_id);
  base::FilePath dispatcher_image_path(base::FilePath(kVmpluginImageDir)
                                           .Append(owner_id)
                                           .Append(image_path.BaseName()));
  LOG(INFO) << "Registering VM at " << dispatcher_image_path.value();
  request.set_path(dispatcher_image_path.value());
  // We do not track VMs by uuid but rather by their name, so always generate
  // new one.
  request.set_new_uuid(base::GenerateGUID());
  request.set_preserve_uuid(false);
  request.set_regenerate_src_uuid(true);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode RegisterVmReques protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send RegisterVm message to dispatcher service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::plugin_dispatcher::RegisterVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse RegisterVmResponse protobuf";
    return false;
  }

  if (response.error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to start server: " << response.error();
    return false;
  }

  return true;
}

}  // namespace concierge
}  // namespace vm_tools
