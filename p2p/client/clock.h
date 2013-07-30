// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_CLOCK_H__
#define P2P_CLIENT_CLOCK_H__

#include "client/clock_interface.h"

#include <base/basictypes.h>

#include <unistd.h>

namespace p2p {

namespace client {

class Clock : public ClockInterface {
 public:
  Clock() {}

  virtual unsigned int Sleep(unsigned int seconds) {
    return sleep(seconds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Clock);
};

}  // namespace p2p

}  // namespace client

#endif  // P2P_CLIENT_CLOCK_H__
