// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_2k_modem.h"
#include "gobi_modem_handler.h"

#include <cromo/carrier.h>
#include <cromo/cromo_server.h>
#include <cromo/utilities.h>

void Gobi2KModemHelper::SetCarrier(GobiModem *modem,
                                   GobiModemHandler *handler,
                                   const std::string& carrier_name,
                                   DBus::Error& error) {

  const Carrier *carrier = handler->server().FindCarrierByName(carrier_name);
  if (carrier == NULL) {
    // TODO(rochberg):  Do we need to sanitize this string?
    LOG(WARNING) << "Could not parse carrier: " << carrier_name;
    error.set(kFirmwareLoadError, kErrorUnknownCarrier);
    return;
  }

  LOG(INFO) << "Carrier image selection starting: " << carrier_name;
  ULONG firmware_id;
  ULONG technology;
  ULONG modem_carrier_id;
  ULONG region;
  ULONG gps_capability;

  ScopedApiConnection connection(*modem);
  connection.ApiConnect(error);
  if (error.is_set())
    return;

  ULONG rc = sdk_->GetFirmwareInfo(&firmware_id,
                                   &technology,
                                   &modem_carrier_id,
                                   &region,
                                   &gps_capability);
  ENSURE_SDK_SUCCESS(GetFirmwareInfo, rc, kFirmwareLoadError);

  if (modem_carrier_id != carrier->carrier_id()) {

    // UpgradeFirmware doesn't pay attention to anything before the
    // last /, so we don't supply it
    std::string image_path = std::string("/") + carrier->firmware_directory();

    LOG(INFO) << "Current Gobi carrier: " << modem_carrier_id
              << ".  Carrier image selection "
              << image_path;
    rc = sdk_->UpgradeFirmware(const_cast<CHAR *>(image_path.c_str()));
    if (rc != 0) {
      LOG(WARNING) << "Carrier image selection from: "
                   << image_path << " failed: " << rc;
      error.set(kFirmwareLoadError, "UpgradeFirmware");
    } else {
      // Explicitly disconnect in all cases, because modem is reset
      connection.ApiDisconnect();
    }
  }
}
