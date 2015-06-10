// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TPM_MANAGER_CLIENT_DBUS_PROXY_H_
#define TPM_MANAGER_CLIENT_DBUS_PROXY_H_

#include "tpm_manager/common/tpm_manager_interface.h"

#include <string>

#include <base/memory/ref_counted.h>
#include <dbus/bus.h>
#include <dbus/object_proxy.h>

#include "tpm_manager/common/export.h"

namespace tpm_manager {

// An implementation of TpmManagerInterface that forwards requests to
// tpm_managerd over D-Bus.
// Usage:
// std::unique_ptr<TpmManagerInterface> tpm_manager = new DBusProxy();
// tpm_manager->GetTpmStatus(...);
class TPM_MANAGER_EXPORT DBusProxy : public TpmManagerInterface {
 public:
  DBusProxy();
  virtual ~DBusProxy();

  // TpmManagerInterface methods.
  bool Initialize() override;
  void GetTpmStatus(const GetTpmStatusRequest& request,
                    const GetTpmStatusCallback& callback) override;
  void TakeOwnership(const TakeOwnershipRequest& request,
                     const TakeOwnershipCallback& callback) override;

  void set_object_proxy(dbus::ObjectProxy* object_proxy) {
    object_proxy_ = object_proxy;
  }

 private:
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_proxy_;
  DISALLOW_COPY_AND_ASSIGN(DBusProxy);
};

}  // namespace tpm_manager

#endif  // TPM_MANAGER_CLIENT_DBUS_PROXY_H_
