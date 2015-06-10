// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "trunks/trunks_ftdi_spi.h"

namespace trunks {

TrunksFtdiSpi::TrunksFtdiSpi() {}

TrunksFtdiSpi::~TrunksFtdiSpi() {}

bool TrunksFtdiSpi::Init() {
  return false;
}

void TrunksFtdiSpi::SendCommand(const std::string& command,
                                const ResponseCallback& callback) {
}

std::string TrunksFtdiSpi::SendCommandAndWait(const std::string& command) {
  return std::string("");
}

}  // namespace trunks
