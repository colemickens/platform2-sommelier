// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_RTNL_LISTENER_
#define SHILL_RTNL_LISTENER_

#include <base/callback_old.h>

namespace shill {

class RTNLMessage;

class RTNLListener {
 public:
  RTNLListener(int listen_flags,
               Callback1<const RTNLMessage &>::Type *callback);
  ~RTNLListener();

  void NotifyEvent(int type, const RTNLMessage &msg);

 private:
  int listen_flags_;
  Callback1<const RTNLMessage &>::Type *callback_;

  DISALLOW_COPY_AND_ASSIGN(RTNLListener);
};

}  // namespace shill

#endif  // SHILL_RTNL_LISTENER_
