// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/cryptohome_client.h"

#include <memory>

#include <brillo/dbus/dbus_object.h>

#include <dbus/cryptohome/dbus-constants.h>
#include <dbus/object_proxy.h>

namespace authpolicy {

CryptohomeClient::CryptohomeClient(
    brillo::dbus_utils::DBusObject* dbus_object) {
  cryptohome_proxy_ = dbus_object->GetBus()->GetObjectProxy(
      cryptohome::kCryptohomeServiceName,
      dbus::ObjectPath(cryptohome::kCryptohomeServicePath));
}

CryptohomeClient::~CryptohomeClient() = default;

std::string CryptohomeClient::GetSanitizedUsername(
    const std::string& account_id_key) {
  dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                               cryptohome::kCryptohomeGetSanitizedUsername);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(account_id_key);
  std::unique_ptr<dbus::Response> response =
      cryptohome_proxy_->CallMethodAndBlock(
          &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT);
  if (!response)
    return std::string();

  dbus::MessageReader reader(response.get());
  std::string sanitized_username;
  if (!reader.PopString(&sanitized_username))
    return std::string();

  return sanitized_username;
}

}  // namespace authpolicy
