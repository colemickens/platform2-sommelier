// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_DUMMY_MODEM_HANDLER_H_
#define CROMO_DUMMY_MODEM_HANDLER_H_

#include <vector>

#include <base/basictypes.h>
#include <dbus-c++/types.h>

#include "cromo/modem_handler.h"

class CromoServer;

class DummyModemHandler : public ModemHandler {
 public:
  explicit DummyModemHandler(CromoServer& server);  // NOLINT - refs.

  virtual std::vector<DBus::Path> EnumerateDevices(
      DBus::Error& error);  // NOLINT - refs.

 protected:
  virtual bool Initialize();

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyModemHandler);
};

#endif  // CROMO_DUMMY_MODEM_HANDLER_H_
