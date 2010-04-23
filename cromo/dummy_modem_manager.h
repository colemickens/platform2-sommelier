// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_DUMMY_MODEM_MANAGER_H_
#define CROMO_DUMMY_MODEM_MANAGER_H_

#include <dbus-c++/types.h>
#include "modem_manager.h"

class ModemManagerServer;

class DummyModemManager : public ModemManager {
 public:
  explicit DummyModemManager(ModemManagerServer& server);

  virtual std::vector<DBus::Path> EnumerateDevices();

 protected:
  virtual bool Initialize();
};

#endif // CROMO_DUMMY_MODEM_MANAGER_H_
