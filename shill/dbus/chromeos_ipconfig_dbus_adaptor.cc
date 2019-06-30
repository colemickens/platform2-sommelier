// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_ipconfig_dbus_adaptor.h"

#include <string>
#include <vector>

#include <base/strings/stringprintf.h>

#include "shill/error.h"
#include "shill/ipconfig.h"
#include "shill/logging.h"

using base::StringPrintf;
using std::string;
using std::vector;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosIPConfigDBusAdaptor* i) {
  return i->GetRpcIdentifier().value();
}
}  // namespace Logging

// static
const char ChromeosIPConfigDBusAdaptor::kPath[] = "/ipconfig/";

ChromeosIPConfigDBusAdaptor::ChromeosIPConfigDBusAdaptor(
    const scoped_refptr<dbus::Bus>& bus,
    IPConfig* config)
    : org::chromium::flimflam::IPConfigAdaptor(this),
      ChromeosDBusAdaptor(bus,
                          StringPrintf("%s%s_%u_%s",
                                       kPath,
                                       SanitizePathElement(
                                           config->device_name()).c_str(),
                                       config->serial(),
                                       config->type().c_str())),
      ipconfig_(config) {
  // Register DBus object.
  RegisterWithDBusObject(dbus_object());
  dbus_object()->RegisterAndBlock();
}

ChromeosIPConfigDBusAdaptor::~ChromeosIPConfigDBusAdaptor() {
  dbus_object()->UnregisterAsync();
  ipconfig_ = nullptr;
}

void ChromeosIPConfigDBusAdaptor::EmitBoolChanged(const string& name,
                                                  bool value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosIPConfigDBusAdaptor::EmitUintChanged(const string& name,
                                                  uint32_t value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosIPConfigDBusAdaptor::EmitIntChanged(const string& name,
                                                 int value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosIPConfigDBusAdaptor::EmitStringChanged(const string& name,
                                                    const string& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

void ChromeosIPConfigDBusAdaptor::EmitStringsChanged(
    const string& name, const vector<string>& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  SendPropertyChangedSignal(name, brillo::Any(value));
}

bool ChromeosIPConfigDBusAdaptor::GetProperties(
    brillo::ErrorPtr* error, brillo::VariantDictionary* properties) {
  SLOG(this, 2) << __func__;
  return ChromeosDBusAdaptor::GetProperties(ipconfig_->store(),
                                            properties,
                                            error);
}

bool ChromeosIPConfigDBusAdaptor::SetProperty(
    brillo::ErrorPtr* error, const string& name, const brillo::Any& value) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::SetProperty(ipconfig_->mutable_store(),
                                          name,
                                          value,
                                          error);
}

bool ChromeosIPConfigDBusAdaptor::ClearProperty(
    brillo::ErrorPtr* error, const string& name) {
  SLOG(this, 2) << __func__ << ": " << name;
  return ChromeosDBusAdaptor::ClearProperty(ipconfig_->mutable_store(),
                                            name,
                                            error);
}

bool ChromeosIPConfigDBusAdaptor::Remove(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  return !Error(Error::kNotSupported).ToChromeosError(error);
}

bool ChromeosIPConfigDBusAdaptor::Refresh(brillo::ErrorPtr* error) {
  SLOG(this, 2) << __func__;
  Error e;
  ipconfig_->Refresh(&e);
  return !e.ToChromeosError(error);
}

}  // namespace shill
