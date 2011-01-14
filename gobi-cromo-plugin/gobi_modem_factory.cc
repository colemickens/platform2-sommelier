// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi_modem_factory.h"

#include "gobi_cdma_modem.h"
#include "gobi_gsm_modem.h"

#include <glog/logging.h>

GobiModem* GobiModemFactory::CreateModem(DBus::Connection& connection,
                                         const DBus::Path& path,
                                         gobi::DeviceElement& device,
                                         gobi::Sdk* sdk) {
  // Determine whether modem is configured for CDMA or
  // GSM, and instantiate an GobiModem object of the
  // corresponding type.
  ULONG rc = sdk->QCWWANConnect(device.deviceNode, device.deviceKey);
  if (rc != 0) {
    LOG(ERROR) << "CreateModem: QCWWANConnect() failed: " << rc;
    return NULL;
  }

  ULONG firmware_id;
  ULONG technology;
  ULONG carrier;
  ULONG region;
  ULONG gps_capability;
  rc = sdk->GetFirmwareInfo(&firmware_id,
                            &technology,
                            &carrier,
                            &region,
                            &gps_capability);
  if (rc != 0) {
    LOG(ERROR) << "CreateModem: cannot get firmware info: " << rc;
    (void)sdk->QCWWANDisconnect();
    return NULL;
  }

  GobiModem *modem = NULL;
  switch (technology) {
    case gobi::kConfigurationCdma:
      modem = new GobiCdmaModem(connection, path, device, sdk);
      LOG(INFO) << "CreateModem: CDMA modem";
      break;
    case gobi::kConfigurationUmts:
      modem = new GobiGsmModem(connection, path, device, sdk);
      LOG(INFO) << "CreateModem: GSM modem";
      break;
    case gobi::kConfigurationUnknownTechnology:
      LOG(ERROR) << "CreateModem: technology is unknown";
      break;
    default:
      LOG(ERROR) << "CreateModem: invalid technology value " << technology;
      break;
  }
  rc = sdk->QCWWANDisconnect();
  if (rc != 0)
    LOG(WARNING) << "CreateModem: QCWWANDisconnect failed: " << rc;
  return modem;
}
