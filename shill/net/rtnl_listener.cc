// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/net/rtnl_listener.h"

#include "shill/net/rtnl_handler.h"

using base::Callback;

namespace shill {

RTNLListener::RTNLListener(int listen_flags,
                           const Callback<void(const RTNLMessage&)>& callback)
    : listen_flags_(listen_flags), callback_(callback) {
  RTNLHandler::GetInstance()->AddListener(this);
}

RTNLListener::~RTNLListener() {
  RTNLHandler::GetInstance()->RemoveListener(this);
}

void RTNLListener::NotifyEvent(int type, const RTNLMessage& msg) {
  if ((type & listen_flags_) != 0)
    callback_.Run(msg);
}

}  // namespace shill
