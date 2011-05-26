// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/callback_old.h>

#include "shill/rtnl_listener.h"
#include "shill/rtnl_handler.h"

namespace shill {

RTNLListener::RTNLListener(int listen_flags,
                           Callback1<struct nlmsghdr *>::Type *callback)
    : listen_flags_(listen_flags), callback_(callback) {
  RTNLHandler::GetInstance()->AddListener(this);
}

RTNLListener::~RTNLListener() {
  RTNLHandler::GetInstance()->RemoveListener(this);
}

void RTNLListener::NotifyEvent(int type, struct nlmsghdr *hdr) {
  if ((type & listen_flags_) != 0)
    callback_->Run(hdr);
}

}  // namespace shill
