// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_GOBI_MODEM_HANDLER_H_
#define CROMO_GOBI_MODEM_HANDLER_H_

#include "base/basictypes.h"

#include <map>

#include <dbus-c++/types.h>

#include <cromo/modem_handler.h>
#include "gobi_modem.h"


class GobiModemHandler : public ModemHandler {
 public:
  explicit GobiModemHandler(CromoServer& server);
  virtual ~GobiModemHandler() {};

  virtual std::vector<DBus::Path> EnumerateDevices();
  bool Initialize();

 protected:
  std::vector<DEVICE_ELEMENT> GetAvailableDevices();
  void HandleNewDeviceList(const std::vector<DEVICE_ELEMENT> &incoming);

 private:
  DISALLOW_COPY_AND_ASSIGN(GobiModemHandler);
  typedef std::map<std::string, GobiModem *> KeyToModem;
  KeyToModem key_to_modem_;

  int scan_generation_;
};


#endif  // CROMO_GOBI_MODEM_HANDLER_H_
