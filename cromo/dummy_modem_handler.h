// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_DUMMY_MODEM_HANDLER_H_
#define CROMO_DUMMY_MODEM_HANDLER_H_

#include "base/basictypes.h"

#include <dbus-c++/types.h>

#include "modem_handler.h"

class CromoServer;

class DummyModemHandler : public ModemHandler {
 public:
  explicit DummyModemHandler(CromoServer& server);

  virtual std::vector<DBus::Path> EnumerateDevices(DBus::Error &error);

 protected:
  virtual bool Initialize();

  DISALLOW_COPY_AND_ASSIGN(DummyModemHandler);
};

#endif // CROMO_DUMMY_MODEM_HANDLER_H_
