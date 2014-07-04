// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cromo/modem_handler.h"

#include <base/strings/stringprintf.h>

#include "cromo/cromo_server.h"

ModemHandler::ModemHandler(CromoServer& server, const std::string& tag)
    : server_(server),
      vendor_tag_(tag),
      instance_number_(0) {
}

void ModemHandler::RegisterSelf() {
  server_.AddModemHandler(this);
}

std::string ModemHandler::MakePath() {
  return base::StringPrintf("%s/%s/%d",
                            CromoServer::kServicePath,
                            vendor_tag_.c_str(),
                            instance_number_++);
}
