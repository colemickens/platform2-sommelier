// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modem_handler.h"

#include <cstdio>

#include "cromo_server.h"

ModemHandler::ModemHandler(CromoServer& server, const std::string& tag)
    : server_(server),
      vendor_tag_(tag),
      instance_number_(0) {
}

void ModemHandler::RegisterSelf() {
  server_.AddModemHandler(this);
}

std::string ModemHandler::MakePath() {
  char instance[32];
  std::string path(CromoServer::kServicePath);

  snprintf(instance, sizeof(instance), "%d", instance_number_++);
  return path.append("/").append(vendor_tag_).append("/").append(instance);
}
