// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include <base/guid.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/bus.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/object_proxy.h>

#include <vm_plugin_dispatcher/proto_bindings/vm_plugin_dispatcher.pb.h>

#include "vm_tools/concierge/vmplugin_dispatcher_interface.h"

namespace vm_tools {
namespace concierge {
namespace pvm {
namespace dispatcher {
namespace {

constexpr char kVmpluginImageDir[] = "/run/pvm-images";

constexpr base::TimeDelta kVmShutdownTimeout = base::TimeDelta::FromMinutes(2);
constexpr base::TimeDelta kVmSuspendTimeout = base::TimeDelta::FromSeconds(20);

}  // namespace

dbus::ObjectProxy* GetServiceProxy(scoped_refptr<dbus::Bus> bus) {
  return bus->GetObjectProxy(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherServiceName,
      dbus::ObjectPath(
          vm_tools::plugin_dispatcher::kVmPluginDispatcherServicePath));
}

bool RegisterVm(dbus::ObjectProxy* proxy,
                const VmId& vm_id,
                const base::FilePath& image_path) {
  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kRegisterVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::RegisterVmRequest request;

  request.set_owner_id(vm_id.owner_id());
  request.set_new_name(vm_id.name());
  base::FilePath dispatcher_image_path(base::FilePath(kVmpluginImageDir)
                                           .Append(vm_id.owner_id())
                                           .Append(image_path.BaseName()));
  LOG(INFO) << "Registering VM at " << dispatcher_image_path.value();
  request.set_path(dispatcher_image_path.value());
  // We do not track VMs by uuid but rather by their name, so always generate
  // new one.
  request.set_new_uuid(base::GenerateGUID());
  request.set_preserve_uuid(false);
  request.set_regenerate_src_uuid(true);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode RegisterVmRequest protobuf";
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
    LOG(ERROR) << "Failed to register VM: " << response.error();
    return false;
  }

  return true;
}

bool UnregisterVm(dbus::ObjectProxy* proxy, const VmId& vm_id) {
  LOG(INFO) << "Unregistering VM " << vm_id;

  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kUnregisterVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::UnregisterVmRequest request;

  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name_uuid(vm_id.name());

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode UnregisterVmRequest protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send UnregisterVm message to dispatcher service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::plugin_dispatcher::UnregisterVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse UnregisterVmResponse protobuf";
    return false;
  }

  if (response.error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to unregister VM: " << response.error();
    return false;
  }

  return true;
}

bool IsVmRegistered(dbus::ObjectProxy* proxy, const VmId& vm_id, bool* result) {
  LOG(INFO) << "Checking whether VM " << vm_id << " is registered";

  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kListVmsMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::ListVmRequest request;

  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name_uuid(vm_id.name());

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode ListVmRequest protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response = proxy->CallMethodAndBlock(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send ListVm message to dispatcher service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::plugin_dispatcher::ListVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse ListVmResponse protobuf";
    return false;
  }

  if (response.error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to get VM info: " << response.error();
    return false;
  }

  *result = false;
  for (const auto& vm_info : response.vm_info()) {
    if (vm_info.name() == vm_id.name()) {
      *result = true;
      break;
    }
  }

  return true;
}

bool ShutdownVm(dbus::ObjectProxy* proxy, const VmId& vm_id) {
  LOG(INFO) << "Shutting down VM " << vm_id;

  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kStopVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::StopVmRequest request;

  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name_uuid(vm_id.name());
  // Allow request to fail if VM is busy.
  request.set_noforce(true);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode StopVmRequest protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response = proxy->CallMethodAndBlock(
      &method_call, kVmShutdownTimeout.InMilliseconds());
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send StopVm message to dispatcher service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::plugin_dispatcher::StopVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse StopVmResponse protobuf";
    return false;
  }

  if (response.error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to stop VM: " << response.error();
    return false;
  }

  return true;
}

bool SuspendVm(dbus::ObjectProxy* proxy, const VmId& vm_id) {
  LOG(INFO) << "Suspending VM " << vm_id;

  dbus::MethodCall method_call(
      vm_tools::plugin_dispatcher::kVmPluginDispatcherInterface,
      vm_tools::plugin_dispatcher::kSuspendVmMethod);
  dbus::MessageWriter writer(&method_call);

  vm_tools::plugin_dispatcher::SuspendVmRequest request;

  request.set_owner_id(vm_id.owner_id());
  request.set_vm_name_uuid(vm_id.name());

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    LOG(ERROR) << "Failed to encode SuspendVmRequest protobuf";
    return false;
  }

  std::unique_ptr<dbus::Response> dbus_response = proxy->CallMethodAndBlock(
      &method_call, kVmSuspendTimeout.InMilliseconds());
  if (!dbus_response) {
    LOG(ERROR) << "Failed to send SuspendVm message to dispatcher service";
    return false;
  }

  dbus::MessageReader reader(dbus_response.get());
  vm_tools::plugin_dispatcher::SuspendVmResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    LOG(ERROR) << "Failed to parse SuspendVmResponse protobuf";
    return false;
  }

  if (response.error() != vm_tools::plugin_dispatcher::VM_SUCCESS) {
    LOG(ERROR) << "Failed to suspend VM: " << response.error();
    return false;
  }

  return true;
}

}  // namespace dispatcher
}  // namespace pvm
}  // namespace concierge
}  // namespace vm_tools
