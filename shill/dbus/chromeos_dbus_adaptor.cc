// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dbus/chromeos_dbus_adaptor.h"

#include <string>
#include <utility>

#include <base/bind.h>
#include <base/callback.h>

#include "shill/error.h"
#include "shill/logging.h"

using base::Bind;
using base::Passed;
using brillo::dbus_utils::DBusObject;
using std::string;

namespace shill {

namespace Logging {
static auto kModuleLogScope = ScopeLogger::kDBus;
static string ObjectID(ChromeosDBusAdaptor* d) {
  if (d == nullptr)
    return "(dbus_adaptor)";
  return d->dbus_path().value();
}
}  // namespace Logging

namespace {

template <typename T>
void TypedMethodReplyCallback(DBusMethodResponsePtr<T> response,
                              const Error& error,
                              const T& returned) {
  brillo::ErrorPtr chromeos_error;
  if (error.ToChromeosError(&chromeos_error)) {
    response->ReplyWithError(chromeos_error.get());
  } else {
    response->Return(returned);
  }
}

}  // namespace

// public static
const char ChromeosDBusAdaptor::kNullPath[] = "/";

ChromeosDBusAdaptor::ChromeosDBusAdaptor(const scoped_refptr<dbus::Bus>& bus,
                                         const std::string& object_path)
    : dbus_path_(object_path),
      dbus_object_(new DBusObject(nullptr, bus, dbus_path_)) {
  SLOG(this, 2) << "DBusAdaptor: " << object_path;
}

ChromeosDBusAdaptor::~ChromeosDBusAdaptor() = default;

// static
bool ChromeosDBusAdaptor::SetProperty(PropertyStore* store,
                                      const std::string& name,
                                      const brillo::Any& value,
                                      brillo::ErrorPtr* error) {
  Error e;
  store->SetAnyProperty(name, value, &e);
  return !e.ToChromeosError(error);
}

// static
bool ChromeosDBusAdaptor::GetProperties(
    const PropertyStore& store,
    brillo::VariantDictionary* out_properties,
    brillo::ErrorPtr* error) {
  Error e;
  store.GetProperties(out_properties, &e);
  return !e.ToChromeosError(error);
}

// static
bool ChromeosDBusAdaptor::ClearProperty(PropertyStore* store,
                                        const std::string& name,
                                        brillo::ErrorPtr* error) {
  Error e;
  store->ClearProperty(name, &e);
  return !e.ToChromeosError(error);
}

// static
string ChromeosDBusAdaptor::SanitizePathElement(const string& object_path) {
  string sanitized_path(object_path);

  for (auto& c : sanitized_path) {
    // The D-Bus specification
    // (http://dbus.freedesktop.org/doc/dbus-specification.html) states:
    // Each element must only contain the ASCII characters "[A-Z][a-z][0-9]_"
    if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
        !(c >= '0' && c <= '9') && c != '_') {
      c = '_';
    }
  }

  return sanitized_path;
}

ResultCallback ChromeosDBusAdaptor::GetMethodReplyCallback(
    DBusMethodResponsePtr<> response) {
  return Bind(&ChromeosDBusAdaptor::MethodReplyCallback, AsWeakPtr(),
              Passed(&response));
}

ResultStringCallback ChromeosDBusAdaptor::GetStringMethodReplyCallback(
    DBusMethodResponsePtr<string> response) {
  return Bind(&ChromeosDBusAdaptor::StringMethodReplyCallback, AsWeakPtr(),
              Passed(&response));
}

ResultBoolCallback ChromeosDBusAdaptor::GetBoolMethodReplyCallback(
    DBusMethodResponsePtr<bool> response) {
  return Bind(&ChromeosDBusAdaptor::BoolMethodReplyCallback, AsWeakPtr(),
              Passed(&response));
}

void ChromeosDBusAdaptor::ReturnResultOrDefer(const ResultCallback& callback,
                                              const Error& error) {
  // Invoke response if command is completed synchronously (either
  // success or failure).
  if (!error.IsOngoing()) {
    callback.Run(error);
  }
}

void ChromeosDBusAdaptor::MethodReplyCallback(DBusMethodResponsePtr<> response,
                                              const Error& error) {
  brillo::ErrorPtr chromeos_error;
  if (error.ToChromeosError(&chromeos_error)) {
    response->ReplyWithError(chromeos_error.get());
  } else {
    response->Return();
  }
}

void ChromeosDBusAdaptor::StringMethodReplyCallback(
    DBusMethodResponsePtr<string> response,
    const Error& error,
    const string& returned) {
  TypedMethodReplyCallback(std::move(response), error, returned);
}

void ChromeosDBusAdaptor::BoolMethodReplyCallback(
    DBusMethodResponsePtr<bool> response, const Error& error, bool returned) {
  TypedMethodReplyCallback(std::move(response), error, returned);
}

}  // namespace shill
