// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/dbus_manager.h"

#include <base/memory/ref_counted.h>

using chromeos::Error;
using chromeos::dbus_utils::AsyncEventSequencer;
using chromeos::dbus_utils::DBusObject;
using chromeos::dbus_utils::ExportedObjectManager;
using org::chromium::privetd::ManagerAdaptor;

namespace privetd {
namespace {
namespace errors {
namespace manager {

const char kNotImplemented[] = "not_implemented";

}  // namespace manager

const char kDomain[] = "privetd";

}  // namespace errors

const char kPingResponse[] = "Hello world!";

}  // namespace

DBusManager::DBusManager(ExportedObjectManager* object_manager)
  : dbus_object_{new DBusObject{object_manager,
                                object_manager->GetBus(),
                                ManagerAdaptor::GetObjectPath()}} {
}

void DBusManager::RegisterAsync(const CompletionAction& on_done) {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());
  dbus_adaptor_.RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(
      sequencer->GetHandler("Failed exporting DBusManager.", true));
  sequencer->OnAllTasksCompletedCall({on_done});
}

bool DBusManager::EnableWiFiBootstrapping(
    chromeos::ErrorPtr* error,
    const dbus::ObjectPath& in_listener_path,
    const chromeos::VariantDictionary& in_options) {
  Error::AddTo(error,
               FROM_HERE,
               errors::kDomain,
               errors::manager::kNotImplemented,
               "Manual WiFi bootstrapping is not implemented.");
  return false;
}

bool DBusManager::DisableWiFiBootstrapping(chromeos::ErrorPtr* error) {
  Error::AddTo(error,
               FROM_HERE,
               errors::kDomain,
               errors::manager::kNotImplemented,
               "Manual WiFi bootstrapping is not implemented.");
  return false;
}

bool DBusManager::EnableGCDBootstrapping(
    chromeos::ErrorPtr* error,
    const dbus::ObjectPath& in_listener_path,
    const chromeos::VariantDictionary& in_options) {
  Error::AddTo(error,
               FROM_HERE,
               errors::kDomain,
               errors::manager::kNotImplemented,
               "Manual GCD bootstrapping is not implemented.");
  return false;
}

bool DBusManager::DisableGCDBootstrapping(chromeos::ErrorPtr* error) {
  Error::AddTo(error,
               FROM_HERE,
               errors::kDomain,
               errors::manager::kNotImplemented,
               "Manual GCD bootstrapping is not implemented.");
  return false;
}

void DBusManager::SetName(const std::string& in_name) {
  // TODO(wiley) Pass in appropriate objects and implement this.
}

void DBusManager::SetDescription(const std::string& in_description) {
  // TODO(wiley) Pass in appropriate objects and implement this.
}

std::string DBusManager::Ping() {
  return kPingResponse;
}


}  // namespace privetd
