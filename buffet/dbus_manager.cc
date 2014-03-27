// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffet/dbus_manager.h"

#include <string>

#include <base/bind.h>

#include "buffet/dbus_constants.h"

using ::std::string;

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
    : bus_(nullptr) {}

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

  // buffet_dbus_object is owned by the Bus.
  auto buffet_dbus_object = GetExportedObject(dbus_constants::kRootServicePath);
  ExportDBusMethod(
      buffet_dbus_object,
      dbus_constants::kRootInterface, dbus_constants::kRootTestMethod,
      base::Bind(&DBusManager::HandleTestMethod, base::Unretained(this)));

  CHECK(bus_->RequestOwnershipAndBlock(dbus_constants::kServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << dbus_constants::kServiceName;
}

void DBusManager::ShutDownDBus() {
  bus_->ShutdownAndBlock();
}

dbus::ExportedObject* DBusManager::GetExportedObject(
    const string& object_path) {
  return bus_->GetExportedObject(dbus::ObjectPath(object_path));
}

void DBusManager::ExportDBusMethod(
    dbus::ExportedObject* exported_object,
    const string& interface_name,
    const string& method_name,
    base::Callback<scoped_ptr<dbus::Response>(dbus::MethodCall*)> handler) {
  DCHECK(exported_object);
  CHECK(exported_object->ExportMethodAndBlock(
      interface_name, method_name,
      base::Bind(&HandleSynchronousDBusMethodCall, handler)));
}

scoped_ptr<dbus::Response> DBusManager::HandleTestMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Received call to test method.";
  return scoped_ptr<dbus::Response>();
}

}  // namespace buffet
