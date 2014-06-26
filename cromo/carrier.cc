// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "carrier.h"

#include <mm/mm-modem.h>

#include "cromo_server.h"

void AddBaselineCarriers(CromoServer *server) {
  server->AddCarrier(new Carrier(
      "Vodafone", "0", 202, MM_MODEM_TYPE_GSM, Carrier::kNone, NULL));
  server->AddCarrier(new Carrier(
      "Verizon Wireless", "1", 101, MM_MODEM_TYPE_CDMA, Carrier::kOtasp,
      "*22899"));
  server->AddCarrier(new Carrier(
      "AT&T", "2", 201, MM_MODEM_TYPE_GSM, Carrier::kNone, NULL));
  server->AddCarrier(new Carrier(
      "Sprint", "3", 102, MM_MODEM_TYPE_CDMA, Carrier::kOmadm, NULL));
  server->AddCarrier(new Carrier(
      "T-Mobile", "4", 203, MM_MODEM_TYPE_GSM, Carrier::kNone, NULL));
  server->AddCarrier(new Carrier(
      "Generic UMTS", "6", 1, MM_MODEM_TYPE_GSM, Carrier::kNone, NULL));
}
