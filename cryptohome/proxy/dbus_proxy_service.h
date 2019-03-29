// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTOHOME_PROXY_DBUS_PROXY_SERVICE_H_
#define CRYPTOHOME_PROXY_DBUS_PROXY_SERVICE_H_

#include <memory>
#include <dbus/bus.h>

#include "cryptohome/proxy/legacy_cryptohome_interface_adaptor.h"

namespace cryptohome {

// The CryptohomeProxyService class takes a dbus::Bus object and uses it to
// register the old interface that it exposes, and also uses it for making the
// calls to the new interface. Note that the standard DbusDaemon and related
// class in not used here so that we can remain flexible in terms of using this
// class in a standalone process (create a standalone DbusDaemon then pass
// dbus::Bus to this class) or within cryptohome itself (use the dbus::Bus from
// the original daemon that serves the new interface)
class CryptohomeProxyService {
 public:
  explicit CryptohomeProxyService(scoped_refptr<dbus::Bus> bus);

  // Called at the start of the daemon that uses this service.
  // This generally registers all the adaptors we've for this service.
  void OnInit();

  // This is called after we've registered all method handlers on DBus.
  void TakeServiceOwnership(bool success);

 private:
  scoped_refptr<dbus::Bus> bus_;
  std::unique_ptr<LegacyCryptohomeInterfaceAdaptor> adaptor_;
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PROXY_DBUS_PROXY_SERVICE_H_
