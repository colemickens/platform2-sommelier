// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "portier/portierd.h"

#include <sysexits.h>

#include <map>
#include <utility>

#include <portier/portier.pb.h>

#include "portier/dbus/constants.h"

namespace portier {

using std::unique_ptr;

using PortierdMethod =
    unique_ptr<dbus::Response> (Portierd::*)(dbus::MethodCall*);

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

}  // namespace

Portierd::Portierd()
    : DBusServiceDaemon(kPortierServiceName, kPortierServicePath) {}

std::unique_ptr<Portierd> Portierd::Create() {
  auto portierd_ptr = unique_ptr<Portierd>(new Portierd());

  if (!portierd_ptr->Init()) {
    portierd_ptr.reset();
  }

  return portierd_ptr;
}

bool Portierd::Init() {
  // TODO(sigquit): Initialize the ND Proxy and RTNetlink listeners
  LOG(INFO) << "Portierd::Init";
  return true;
}

// Deamon callbacks.
int Portierd::OnInit() {
  // Must call the superclass's OnInit() before exporting objects.
  const int exit_code = DBusServiceDaemon::OnInit();
  if (exit_code != EX_OK) {
    return exit_code;
  }

  LOG(INFO) << "Portierd::OnInit";

  exported_object_ =
      bus_->GetExportedObject(dbus::ObjectPath(kPortierServicePath));
  if (!exported_object_) {
    LOG(ERROR) << "Failed to exported object " << kPortierServicePath;
  }

  static const std::map<const char*, PortierdMethod> kPortierMethods = {
      {kBindInterfaceMethod, &Portierd::BindInterface},
      {kReleaseInterfaceMethod, &Portierd::ReleaseInterface},
      {kCreateProxyGroupMethod, &Portierd::CreateProxyGroup},
      {kReleaseProxyGroupMethod, &Portierd::ReleaseProxyGroup},
      {kAddToGroupMethod, &Portierd::AddToGroup},
      {kRemoveFromGroupMethod, &Portierd::RemoveFromGroup},
      {kSetUpstreamInterfaceMethod, &Portierd::SetUpstream},
      {kUnsetUpstreamInterfaceMethod, &Portierd::UnsetUpstream}};

  for (const auto& iter : kPortierMethods) {
    bool ret = exported_object_->ExportMethodAndBlock(
        kPortierInterface, iter.first,
        base::Bind(&HandleSynchronousDBusMethodCall,
                   base::Bind(iter.second, base::Unretained(this))));
    if (!ret) {
      LOG(ERROR) << "Failed to export method " << iter.first;
      return EX_SOFTWARE;
    }
  }
  if (!bus_->RequestOwnershipAndBlock(kPortierServiceName,
                                      dbus::Bus::REQUIRE_PRIMARY)) {
    LOG(ERROR) << "Failed to take ownership of " << kPortierServiceName;
    return EX_SOFTWARE;
  }
  return EX_OK;
}

int Portierd::OnEventLoopStarted() {
  LOG(INFO) << "Portierd::OnEventLoopStarted";
  return EX_OK;
}

void Portierd::OnShutdown(int* exit_code) {}

bool Portierd::OnRestart() {
  return true;
}

// Portier D-Bus methods.
// TODO(sigquie): Implement these methods.

unique_ptr<dbus::Response> Portierd::BindInterface(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::BindInterface";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  BindInterfaceRequest request;
  BindInterfaceResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse BindInterfaceRequest from message";
    response.set_status(BindInterfaceResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Interface: " << request.interface_name();
  response.set_status(BindInterfaceResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::ReleaseInterface(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::ReleaseInterface";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ReleaseInterfaceRequest request;
  ReleaseInterfaceResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ReleaseInterfaceRequest from message";
    response.set_status(ReleaseInterfaceResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Interface: " << request.interface_name();
  response.set_status(ReleaseInterfaceResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::CreateProxyGroup(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::CreateProxyGroup";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  CreateProxyGroupRequest request;
  CreateProxyGroupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse CreateProxyGroupResponse from message";
    response.set_status(CreateProxyGroupResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Group: " << request.group_name();
  response.set_status(CreateProxyGroupResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::ReleaseProxyGroup(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::ReleaseProxyGroup";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  ReleaseProxyGroupRequest request;
  ReleaseProxyGroupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse ReleaseProxyGroupRequest from message";
    response.set_status(ReleaseProxyGroupResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Group: " << request.group_name();
  response.set_status(ReleaseProxyGroupResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::AddToGroup(dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::AddToGroup";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  AddToGroupRequest request;
  AddToGroupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse AddToGroupRequest from message";
    response.set_status(AddToGroupResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Interface: " << request.interface_name();
  LOG(INFO) << "Group: " << request.group_name();
  LOG(INFO) << "As Upstream: " << (request.as_upstream() ? "true" : "false");
  response.set_status(AddToGroupResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::RemoveFromGroup(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::RemoveFromGroup";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  RemoveFromGroupRequest request;
  RemoveFromGroupResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse RemoveFromGroupRequest from message";
    response.set_status(RemoveFromGroupResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Interface: " << request.interface_name();
  response.set_status(RemoveFromGroupResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::SetUpstream(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::SetUpstream";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  SetUpstreamInterfaceRequest request;
  SetUpstreamInterfaceResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse SetUpstreamInterfaceRequest from message";
    response.set_status(SetUpstreamInterfaceResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Interface: " << request.interface_name();
  response.set_status(SetUpstreamInterfaceResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

unique_ptr<dbus::Response> Portierd::UnsetUpstream(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Portierd::UnsetUpstream";

  std::unique_ptr<dbus::Response> dbus_response(
      dbus::Response::FromMethodCall(method_call));

  dbus::MessageReader reader(method_call);
  dbus::MessageWriter writer(dbus_response.get());

  UnsetUpstreamInterfaceRequest request;
  UnsetUpstreamInterfaceResponse response;

  if (!reader.PopArrayOfBytesAsProto(&request)) {
    LOG(ERROR) << "Unable to parse UnsetUpstreamInterfaceRequest from message";
    response.set_status(UnsetUpstreamInterfaceResponse::FAILED);
    response.set_failure_reason("Unable to parse protobuf");
    writer.AppendProtoAsArrayOfBytes(response);
    return dbus_response;
  }

  LOG(INFO) << "Group: " << request.group_name();
  response.set_status(UnsetUpstreamInterfaceResponse::SUCCESS);
  writer.AppendProtoAsArrayOfBytes(response);
  return dbus_response;
}

}  // namespace portier
