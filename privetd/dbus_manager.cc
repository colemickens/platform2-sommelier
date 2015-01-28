// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/dbus_manager.h"

#include <base/memory/ref_counted.h>
#include <chromeos/any.h>

#include "privetd/security_delegate.h"
#include "privetd/security_manager.h"

using chromeos::Any;
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
const char kPairingSessionIdKey[] = "sessionId";
const char kPairingModeKey[] = "mode";
const char kPairingCodeKey[] = "code";

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
  security_manager->RegisterPairingListeners(
      base::Bind(&DBusManager::OnPairingStart,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&DBusManager::OnPairingEnd,
                 weak_ptr_factory_.GetWeakPtr()));
  // TODO(wiley) Watch for appropriate state variables from |cloud_delegate|.
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

void DBusManager::OnPairingStart(const std::string& session_id,
                                 PairingType pairing_type,
                                 const std::string& code) {
  // For now, just overwrite the exposed PairInfo with
  // the most recent pairing attempt.
  std::string pairing_type_str;
  switch (pairing_type) {
    case PairingType::kPinCode:
      pairing_type_str = "pinCode";
      break;
    case PairingType::kEmbeddedCode:
      pairing_type_str = "embeddedCode";
      break;
    case PairingType::kUltrasoundDsssBroadcaster:
      pairing_type_str = "ultrasound32";
      break;
    case PairingType::kAudibleDtmfBroadcaster:
      pairing_type_str = "audible32";
      break;
  }
  dbus_adaptor_.SetPairingInfo(chromeos::VariantDictionary{
      {kPairingSessionIdKey, session_id},
      {kPairingModeKey, pairing_type_str},
      {kPairingCodeKey, code},
  });
}

void DBusManager::OnPairingEnd(const std::string& session_id) {
  auto exposed_pairing_attempt = dbus_adaptor_.GetPairingInfo();
  auto it = exposed_pairing_attempt.find(kPairingSessionIdKey);
  if (it == exposed_pairing_attempt.end()) {
    return;
  }
  std::string exposed_session{it->second.TryGet<std::string>()};
  if (exposed_session == session_id) {
    dbus_adaptor_.SetPairingInfo(chromeos::VariantDictionary{});
  }
}

}  // namespace privetd
