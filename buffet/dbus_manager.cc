// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_manager.h"

#include <string>

#include <base/bind.h>

namespace buffet {

namespace {

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  auto response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);

  response_sender.Run(response.Pass());
}

}  // namespace

DBusManager::DBusManager()
    : bus_(nullptr),
      buffet_dbus_object_(nullptr) {}

DBusManager::~DBusManager() {}

void DBusManager::Init() {
  InitDBus();
}

void DBusManager::Finalize() {
  ShutDownDBus();
}

void DBusManager::InitDBus() {
  dbus::Bus::Options options;
  // TODO(sosa): Should this be on the system bus?
  options.bus_type = dbus::Bus::SYSTEM;
  bus_ = new dbus::Bus(options);
  CHECK(bus_->Connect());

  buffet_dbus_object_ = bus_->GetExportedObject(
      dbus::ObjectPath(kBuffetServicePath));
  ExportDBusMethod(kTestMethod, &DBusManager::HandleTestMethod);

  CHECK(bus_->RequestOwnershipAndBlock(kBuffetServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kBuffetServiceName;
}

void DBusManager::ShutDownDBus() {
  bus_->ShutdownAndBlock();
}

void DBusManager::ExportDBusMethod(const std::string& method_name,
                              DBusMethodCallMemberFunction member) {
  DCHECK(buffet_dbus_object_);
  CHECK(buffet_dbus_object_->ExportMethodAndBlock(
      kBuffetInterface, method_name,
      base::Bind(&HandleSynchronousDBusMethodCall,
                 base::Bind(member, base::Unretained(this)))));
}

scoped_ptr<dbus::Response> DBusManager::HandleTestMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received call to test method.";
  return scoped_ptr<dbus::Response>();
}

}  // namespace buffet
