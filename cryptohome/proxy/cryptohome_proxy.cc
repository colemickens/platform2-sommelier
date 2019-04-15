// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>

#include "cryptohome/proxy/dbus_service.h"

int main(int argc, char** argv) {
#if USE_CRYPTOHOME_USERDATAAUTH_INTERFACE
  cryptohome::CryptohomeProxyDaemon proxy_daemon;
  return proxy_daemon.Run();
#else
  LOG(FATAL) << "cryptohome_userdataauth_interface USE flag is unset, "
                "cryptohome-proxy is disabled.";
  return 0;
#endif
}
