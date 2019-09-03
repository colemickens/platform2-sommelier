// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <brillo/dbus/introspectable_helper.h>
#include <dbus/cryptohome/dbus-constants.h>

#include "cryptohome/proxy/dbus_proxy_service.h"

namespace cryptohome {

using brillo::dbus_utils::AsyncEventSequencer;

CryptohomeProxyService::CryptohomeProxyService(scoped_refptr<dbus::Bus> bus)
    : bus_(bus) {}

void CryptohomeProxyService::OnInit() {
  scoped_refptr<AsyncEventSequencer> sequencer(new AsyncEventSequencer());

  DCHECK(!dbus_object_);
  dbus_object_ = std::make_unique<brillo::dbus_utils::DBusObject>(
      nullptr, bus_, dbus::ObjectPath(kCryptohomeServicePath));

  adaptor_.reset(
      new LegacyCryptohomeInterfaceAdaptor(bus_, dbus_object_.get()));
  adaptor_->RegisterAsync();

  brillo::dbus_utils::IntrospectableInterfaceHelper introspection;
  introspection.AddInterfaceXml(adaptor_->GetIntrospectionXml());
  introspection.RegisterWithDBusObject(dbus_object_.get());

  dbus_object_->RegisterAsync(
      sequencer->GetHandler("RegisterAsync() failed", true));

  sequencer->OnAllTasksCompletedCall({base::Bind(
      &CryptohomeProxyService::TakeServiceOwnership, base::Unretained(this))});
}

void CryptohomeProxyService::TakeServiceOwnership(bool success) {
  CHECK(success) << "Init of one or more DBus objects has failed.";
  CHECK(bus_->RequestOwnershipAndBlock(kCryptohomeServiceName,
                                       dbus::Bus::REQUIRE_PRIMARY))
      << "Unable to take ownership of " << kCryptohomeServiceName;
}

}  // namespace cryptohome
