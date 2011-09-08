// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_proxy.h"

#include <base/logging.h>

namespace chaps {

ChapsProxyImpl::ChapsProxyImpl() {}

ChapsProxyImpl::~ChapsProxyImpl() {}

bool ChapsProxyImpl::Connect(const std::string& username) {
  bool success = false;
  try {
    if (proxy_ == NULL) {
      // Establish a D-Bus connection.
      DBus::Connection connection = DBus::Connection::SessionBus();
      proxy_.reset(new Proxy(connection, "/org/chromium/Chaps",
                             "org.chromium.Chaps"));
    }
    // Connect to our service via D-Bus.
    if (proxy_ != NULL)
      proxy_->Connect(username, session_id_, success);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
  return success;
}

void ChapsProxyImpl::Disconnect() {
  try {
    if (proxy_ != NULL)
      proxy_->Disconnect(session_id_);
  } catch (DBus::Error err) {
    LOG(ERROR) << "DBus::Error - " << err.what();
  }
}

}  // namespace

