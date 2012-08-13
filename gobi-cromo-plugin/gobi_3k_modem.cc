// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_3k_modem.h"
#include "gobi_modem_handler.h"

#include <gobi3k.h>

void Gobi3KModemHelper::SetCarrier(GobiModem *modem,
                                   GobiModemHandler *handler,
                                   const std::string& carrier_name,
                                   DBus::Error& error)
{
  gobifw_init(NULL);
  gobifw *fw = gobifw_bycarrier(carrier_name.c_str());
  enum gobifw_activate_status rc;
  bool was_connected = modem->IsApiConnected();
  DBus::Error connect_error;

  if (!fw) {
    LOG(WARNING) << "No such carrier: " << carrier_name << ": "
                 << gobifw_lasterror();
    error.set(kFirmwareLoadError, kErrorUnknownCarrier);
    return;
  }

  if (modem->is_connecting_or_connected()) {
    error.set(kFirmwareLoadError,
              "Cannot load firmware while connected or connecting");
    gobifw_free(fw);
    return;
  }

  if (was_connected)
    modem->ApiDisconnect();
  rc = gobifw_activate(fw);
  if (rc) {
    LOG(WARNING) << "Firmware activation failed: " << gobifw_lasterror();
    error.set(kFirmwareLoadError, gobifw_lasterror());
  }

  if (rc == GOBIFW_ACTIVATE_OK || rc == GOBIFW_ACTIVATE_ERROR_RESET) {
    // Object is deceased. Remove it early so that we don't process any
    // queued-up dbus calls on the now-dead device.
    modem->unregister_obj();
    handler->Remove(modem);
  } else if (rc == GOBIFW_ACTIVATE_ERROR_NORESET) {
    if (was_connected)
      modem->ApiConnect(connect_error);
  }
}
