// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "privetd/shill_client.h"

#include <base/message_loop/message_loop.h>
#include <chromeos/any.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/errors/error.h>
#include <chromeos/errors/error_codes.h>

using chromeos::Any;
using dbus::ObjectPath;
using org::chromium::flimflam::ServiceProxy;
using std::string;

namespace privetd {

namespace {
void IgnoreDetachEvent() { }
}  // namespace

ShillClient::ShillClient(const scoped_refptr<dbus::Bus>& bus)
    : bus_{bus}, manager_proxy_{bus_, ObjectPath{"/"}} {
}

bool ShillClient::ConnectToService(const string& ssid,
                                   const string& passphrase,
                                   const base::Closure& on_success,
                                   chromeos::ErrorPtr* error) {
  CleanupConnectingService(false);
  chromeos::VariantDictionary service_properties;
  service_properties[shill::kTypeProperty] = Any{string{shill::kTypeWifi}};
  service_properties[shill::kSSIDProperty] = Any{ssid};
  service_properties[shill::kPassphraseProperty] = Any{passphrase};
  service_properties[shill::kSecurityProperty] =
      Any{string{shill::kSecurityPsk}};
  service_properties[shill::kSaveCredentialsProperty] = Any{true};
  service_properties[shill::kAutoConnectProperty] = Any{true};
  ObjectPath service_path;
  if (!manager_proxy_.ConfigureService(service_properties,
                                       &service_path,
                                       error)) {
    return false;
  }
  if (!manager_proxy_.RequestScan(shill::kTypeWifi, error)) {
    return false;
  }
  connecting_service_reset_pending_ = false;
  have_called_connect_ = false;
  connecting_service_.reset(new ServiceProxy{bus_, service_path});
  on_connect_success_.Reset(on_success);
  connecting_service_->RegisterPropertyChangedSignalHandler(
      base::Bind(&ShillClient::OnServicePropertyChange,
                 weak_factory_.GetWeakPtr(),
                 service_path),
      base::Bind(&ShillClient::OnServicePropertyChangeRegistration,
                 weak_factory_.GetWeakPtr(),
                 service_path));
  return true;
}

void ShillClient::RegisterConnectivityListener(
    const ConnectivityListener& listener) {
  // TODO(wiley) Actually call these guys back at some point.
  connectivity_listeners_.push_back(listener);
}

bool ShillClient::IsConnectedState(const std::string& service_state) {
  if ((service_state.compare(shill::kStateReady) == 0) ||
      (service_state.compare(shill::kStatePortal) == 0) ||
      (service_state.compare(shill::kStateOnline) == 0)) {
    return true;
  }
  return false;
}

void ShillClient::OnServicePropertyChangeRegistration(const ObjectPath& path,
                                                      const string& interface,
                                                      const string& signal_name,
                                                      bool success) {
  VLOG(3) << "OnServicePropertyChangeRegistration(" << path.value() << ");";
  if (!connecting_service_ ||
      connecting_service_->GetObjectPath() != path) {
    return;  // This is success for a proxy we no longer care about.
  }
  if (!success) {
    CleanupConnectingService(false);
    return;
  }
  chromeos::VariantDictionary properties;
  if (!connecting_service_->GetProperties(&properties, nullptr)) {
    CleanupConnectingService(false);
    return;
  }
  // Give ourselves property changed signals for the initial property
  // values.
  auto it = properties.find(shill::kStateProperty);
  if (it != properties.end()) {
    OnServicePropertyChange(path, shill::kStateProperty,
                            it->second);
  }
  it = properties.find(shill::kSignalStrengthProperty);
  if (it != properties.end()) {
    OnServicePropertyChange(path, shill::kSignalStrengthProperty,
                            it->second);
  }
}

void ShillClient::OnServicePropertyChange(const ObjectPath& service_path,
                                          const string& property_name,
                                          const Any& property_value) {
  VLOG(3) << "ServicePropertyChange(" << service_path.value() << ", "
          << property_name << ", ...);";
  if (!connecting_service_ || on_connect_success_.IsCancelled() ||
      service_path != connecting_service_->GetObjectPath()) {
    // Nothing we could potentially do with this information.
    VLOG(3) << "Ignoring property change because we're not currently "
               "connecting.";
    return;
  }
  if (property_name == shill::kStateProperty) {
    const string state{property_value.TryGet<string>()};
    VLOG(1) << "New service state=" << state;
    if (IsConnectedState(state)) {
      connecting_service_reset_pending_ = true;
      on_connect_success_.callback().Run();
      CleanupConnectingService(true);
    }
  }
  if (property_name == shill::kSignalStrengthProperty &&
      !have_called_connect_) {
    uint8_t strength = property_value.TryGet<uint8_t>();
    if (strength > 0) {
      VLOG(1) << "Service has signal strength. Calling Connect().";
      have_called_connect_ = true;
      chromeos::ErrorPtr error;
      if (!connecting_service_->Connect(&error)) {
        // Sometimes this just indicates that we've already connected,
        // or are connecting, or some other very unexciting thing.
        // Ignore all that, and rely on state changes to detect
        // connectivity.
        LOG(ERROR) << "Failed to call Connect() on WiFi service.";
      }
    }
  }
}

void ShillClient::CleanupConnectingService(bool check_for_reset_pending) {
  if (check_for_reset_pending && !connecting_service_reset_pending_) {
    return;  // Must have called connect before we got here.
  }
  if (connecting_service_) {
    connecting_service_->ReleaseObjectProxy(base::Bind(&IgnoreDetachEvent));
    connecting_service_.reset();
  }
  on_connect_success_.Cancel();
}

}  // namespace privetd
