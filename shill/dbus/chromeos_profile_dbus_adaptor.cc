// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_profile_dbus_adaptor.h"

#include <string>

#include "shill/error.h"
#include "shill/logging.h"
#include "shill/profile.h"
#include "shill/service.h"

using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosProfileDBusAdaptor* p) {
  return p->GetRpcIdentifier().value();
}
}  // namespace Logging


// static
const char ChromeosProfileDBusAdaptor::kPath[] = "/profile/";

ChromeosProfileDBusAdaptor::ChromeosProfileDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    Profile* profile)
    : org::chromium::flimflam::ProfileAdaptor(this),
      ChromeosDBusAdaptor(bus, kPath + profile->GetFriendlyName()),
      profile_(profile) {
  // Register DBus object.
  RegisterWithDBusObject(dbus_object());
  dbus_object()->RegisterAndBlock();
}

ChromeosProfileDBusAdaptor::~ChromeosProfileDBusAdaptor() {
  dbus_object()->UnregisterAsync();
  profile_ = nullptr;
}

void ChromeosProfileDBusAdaptor::EmitBoolChanged(const string& name,
                                                 bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosProfileDBusAdaptor::EmitUintChanged(const string& name,
                                                 uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosProfileDBusAdaptor::EmitIntChanged(const string& name, int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosProfileDBusAdaptor::EmitStringChanged(const string& name,
                                                   const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

bool ChromeosProfileDBusAdaptor::GetProperties(
    brillo::ErrorPtr* error, brillo::VariantDictionary* properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(profile_->store(),
                                            properties,
                                            error);
}

bool ChromeosProfileDBusAdaptor::SetProperty(
    brillo::ErrorPtr* error, const string& name, const brillo::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(profile_->mutable_store(),
                                          name,
                                          value,
                                          error);
}

bool ChromeosProfileDBusAdaptor::GetEntry(
    brillo::ErrorPtr* error,
    const std::string& name,
    brillo::VariantDictionary* entry_properties) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  ServiceRefPtr service = profile_->GetServiceFromEntry(name, &e);
  if (!e.IsSuccess()) {
    return !e.ToChromeosError(error);
  }
  return ChromeosDBusAdaptor::GetProperties(service->store(),
                                            entry_properties,
                                            error);
}

bool ChromeosProfileDBusAdaptor::DeleteEntry(brillo::ErrorPtr* error,
                                             const std::string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  Error e;
  profile_->DeleteEntry(name, &e);
  return !e.ToChromeosError(error);
}

}  // namespace shill
