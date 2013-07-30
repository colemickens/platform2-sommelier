// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_CLOCK_INTERFACE_H__
#define P2P_CLIENT_CLOCK_INTERFACE_H__

namespace p2p {

namespace client {

// The clock interface allows access to some system time-related functions.
class ClockInterface {
 public:
  // Sleeps for |seconds| seconds. Returns zero if the time has elapsed, or
  // the number of seconds left to sleep if the call was interrupted.
  virtual unsigned int Sleep(unsigned int seconds) = 0;

  virtual ~ClockInterface() {}
};

}  // namespace p2p

}  // namespace client

#endif  // P2P_CLIENT_CLOCK_INTERFACE_H__
