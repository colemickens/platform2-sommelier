// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/portier.h"

#include <utility>

#include <dbus/message.h>
#include <dbus/object_path.h>
#include <dbus/object_proxy.h>
#include <portier/portier.pb.h>

#include "portier/dbus/constants.h"

namespace portier {

using std::string;
using std::unique_ptr;

using dbus::MessageReader;
using dbus::MessageWriter;
using dbus::MethodCall;
using dbus::ObjectProxy;

using Code = Status::Code;

namespace {

constexpr int kDefaultTimeoutMs = 10 * 1000;

}  // namespace

unique_ptr<Portier> Portier::Create() {
  auto portier_ptr = unique_ptr<Portier>(new Portier());

  if (!portier_ptr->Init()) {
    LOG(ERROR) << "Failed to initialize Portier dbus interface";
    portier_ptr.reset();
  }

  return portier_ptr;
}

bool Portier::Init() {
  dbus::Bus::Options opts;
  opts.bus_type = dbus::Bus::SYSTEM;
  scoped_refptr<dbus::Bus> bus(new dbus::Bus(std::move(opts)));
  if (!bus->Connect()) {
    LOG(ERROR) << "Failed to connect to system bus";
    return false;
  }
  bus_ = std::move(bus);
  return true;
}

ObjectProxy* Portier::GetProxy() {
  DCHECK(bus_);
  return bus_->GetObjectProxy(kPortierServiceName,
                              dbus::ObjectPath(kPortierServicePath));
}

Status Portier::BindInterface(const string& if_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kBindInterfaceMethod);
  MessageWriter writer(&method_call);

  BindInterfaceRequest request;
  request.set_interface_name(if_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  BindInterfaceResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case BindInterfaceResponse::SUCCESS:
      return Status();
    case BindInterfaceResponse::EXISTS:
      return Status(Code::ALREADY_EXISTS)
             << "Interface " << if_name << " is already managed";
    case BindInterfaceResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::ReleaseInterface(const string& if_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kReleaseInterfaceMethod);
  MessageWriter writer(&method_call);

  ReleaseInterfaceRequest request;
  request.set_interface_name(if_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  ReleaseInterfaceResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case ReleaseInterfaceResponse::SUCCESS:
      return Status();
    case ReleaseInterfaceResponse::DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Interface " << if_name << " is not managed";
    case ReleaseInterfaceResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::CreateProxyGroup(const string& pg_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kCreateProxyGroupMethod);
  MessageWriter writer(&method_call);

  CreateProxyGroupRequest request;
  request.set_group_name(pg_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  CreateProxyGroupResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case CreateProxyGroupResponse::SUCCESS:
      return Status();
    case CreateProxyGroupResponse::EXISTS:
      return Status(Code::ALREADY_EXISTS)
             << "Proxy group " << pg_name << " is already exists";
    case CreateProxyGroupResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::ReleaseProxyGroup(const string& pg_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kReleaseProxyGroupMethod);
  MessageWriter writer(&method_call);

  ReleaseProxyGroupRequest request;
  request.set_group_name(pg_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  ReleaseProxyGroupResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case ReleaseProxyGroupResponse::SUCCESS:
      return Status();
    case ReleaseProxyGroupResponse::DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Proxy group " << pg_name << " does not exist";
    case ReleaseProxyGroupResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::AddToGroup(const string& if_name,
                           const string& pg_name,
                           bool as_upstream) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kAddToGroupMethod);
  MessageWriter writer(&method_call);

  AddToGroupRequest request;
  request.set_interface_name(if_name);
  request.set_group_name(pg_name);
  request.set_as_upstream(as_upstream);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  AddToGroupResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case AddToGroupResponse::SUCCESS:
      return Status();
    case AddToGroupResponse::EXISTS:
      return Status(Code::ALREADY_EXISTS)
             << "Interface " << if_name
             << " is already a member of another group";
    case AddToGroupResponse::DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Either interface " << if_name << " and / or proxy group "
             << pg_name << " does not exist";
    case AddToGroupResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::RemoveFromGroup(const string& if_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kRemoveFromGroupMethod);
  MessageWriter writer(&method_call);

  RemoveFromGroupRequest request;
  request.set_interface_name(if_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  RemoveFromGroupResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case RemoveFromGroupResponse::SUCCESS:
    case RemoveFromGroupResponse::NO_OPERATION:
      return Status();
    case RemoveFromGroupResponse::DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Interface " << if_name << " is not managed";
    case RemoveFromGroupResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::SetUpstream(const string& if_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kSetUpstreamInterfaceMethod);
  MessageWriter writer(&method_call);

  SetUpstreamInterfaceRequest request;
  request.set_interface_name(if_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  SetUpstreamInterfaceResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case SetUpstreamInterfaceResponse::SUCCESS:
      return Status();
    case SetUpstreamInterfaceResponse::IF_DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Interface " << if_name << " is not managed";
    case SetUpstreamInterfaceResponse::GROUP_DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Interface " << if_name << " is not part of any group";
    case SetUpstreamInterfaceResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

Status Portier::UnsetUpstream(const string& pg_name) {
  ObjectProxy* proxy = GetProxy();
  DCHECK(proxy);

  MethodCall method_call(kPortierInterface, kUnsetUpstreamInterfaceMethod);
  MessageWriter writer(&method_call);

  UnsetUpstreamInterfaceRequest request;
  request.set_group_name(pg_name);

  if (!writer.AppendProtoAsArrayOfBytes(request)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not serialize request";
  }

  unique_ptr<dbus::Response> dbus_response =
      proxy->CallMethodAndBlock(&method_call, kDefaultTimeoutMs);
  if (!dbus_response) {
    return Status(Code::UNEXPECTED_FAILURE) << "Dbus method call failed";
  }

  MessageReader reader(dbus_response.get());
  UnsetUpstreamInterfaceResponse response;
  if (!reader.PopArrayOfBytesAsProto(&response)) {
    return Status(Code::UNEXPECTED_FAILURE) << "Could not deserialize response";
  }

  switch (response.status()) {
    case UnsetUpstreamInterfaceResponse::SUCCESS:
      return Status();
    case UnsetUpstreamInterfaceResponse::DOES_NOT_EXIST:
      return Status(Code::DOES_NOT_EXIST)
             << "Proxy group " << pg_name << " does not exist";
    case UnsetUpstreamInterfaceResponse::FAILED:
      return Status(Code::UNEXPECTED_FAILURE) << response.failure_reason();
    default:
      return Status(Code::UNEXPECTED_FAILURE) << "Unknown response";
  }
}

}  // namespace portier
