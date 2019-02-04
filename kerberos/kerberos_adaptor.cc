// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "kerberos/kerberos_adaptor.h"

#include <utility>

#include <brillo/dbus/dbus_object.h>

namespace kerberos {

KerberosAdaptor::KerberosAdaptor(
    std::unique_ptr<brillo::dbus_utils::DBusObject> dbus_object)
    : org::chromium::KerberosAdaptor(this),
      dbus_object_(std::move(dbus_object)) {}

KerberosAdaptor::~KerberosAdaptor() = default;

void KerberosAdaptor::RegisterAsync(
    const brillo::dbus_utils::AsyncEventSequencer::CompletionAction&
        completion_callback) {
  RegisterWithDBusObject(dbus_object_.get());
  dbus_object_->RegisterAsync(completion_callback);
}

void KerberosAdaptor::Test(bool test) {
  LOG(ERROR) << "Test(" << (test ? "true" : "false") << ")";
}

}  // namespace kerberos
