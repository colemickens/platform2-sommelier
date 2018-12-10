// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "authpolicy/cryptohome_client.h"

#include <memory>

#include <brillo/dbus/dbus_object.h>
#include <cryptohome/dbus-proxies.h>
#include <dbus/cryptohome/dbus-constants.h>

namespace authpolicy {

CryptohomeClient::CryptohomeClient(
    brillo::dbus_utils::DBusObject* dbus_object) {
  proxy_ = std::make_unique<org::chromium::CryptohomeInterfaceProxy>(
      dbus_object->GetBus());
}

CryptohomeClient::~CryptohomeClient() = default;

std::string CryptohomeClient::GetSanitizedUsername(
    const std::string& account_id_key) {
  std::string sanitized;
  brillo::ErrorPtr error;
  if (!proxy_->GetSanitizedUsername(account_id_key, &sanitized, &error)) {
    LOG(ERROR) << "Call to " << cryptohome::kCryptohomeGetSanitizedUsername
               << " failed. "
               << (error ? error->GetMessage().c_str() : "Unknown error.");
    return std::string();
  }
  return sanitized;
}

}  // namespace authpolicy
