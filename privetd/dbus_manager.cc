// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/dbus_manager.h"

#include <base/memory/ref_counted.h>

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

DBusManager::DBusManager(ExportedObjectManager* object_manager,
                         WifiBootstrapManager* wifi_bootstrap_manager,
                         CloudDelegate* cloud_delegate,
                         SecurityManager* security_manager)
  : dbus_object_{new DBusObject{object_manager,
                                object_manager->GetBus(),
                                ManagerAdaptor::GetObjectPath()}} {
  if (wifi_bootstrap_manager) {
    wifi_bootstrap_manager->RegisterStateListener(
        base::Bind(&DBusManager::UpdateWiFiBootstrapState,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    UpdateWiFiBootstrapState(WifiBootstrapManager::kDisabled);
  }
  // TODO(wiley) Watch for appropriate state variables from |cloud_delegate|.
  // TODO(wiley) Watch for new pairing attempts from |security_manager|.
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
  chromeos::Error::AddTo(error,
                         FROM_HERE,
                         errors::kDomain,
                         errors::manager::kNotImplemented,
                         "Manual WiFi bootstrapping is not implemented.");
  return false;
}

bool DBusManager::DisableWiFiBootstrapping(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error,
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
  chromeos::Error::AddTo(error,
                         FROM_HERE,
                         errors::kDomain,
                         errors::manager::kNotImplemented,
                         "Manual GCD bootstrapping is not implemented.");
  return false;
}

bool DBusManager::DisableGCDBootstrapping(chromeos::ErrorPtr* error) {
  chromeos::Error::AddTo(error,
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

void DBusManager::UpdateWiFiBootstrapState(WifiBootstrapManager::State state) {
  switch (state) {
    case WifiBootstrapManager::kDisabled:
      dbus_adaptor_.SetWiFiBootstrapState("disabled");
      break;
    case WifiBootstrapManager::kBootstrapping:
      dbus_adaptor_.SetWiFiBootstrapState("waiting");
      break;
    case WifiBootstrapManager::kMonitoring:
      dbus_adaptor_.SetWiFiBootstrapState("monitoring");
      break;
    case WifiBootstrapManager::kConnecting:
      dbus_adaptor_.SetWiFiBootstrapState("connecting");
      break;
  }
}

}  // namespace privetd
