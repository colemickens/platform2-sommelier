// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_CLIENT_CLOCK_INTERFACE_H__
#define P2P_CLIENT_CLOCK_INTERFACE_H__

#include <base/time.h>

namespace p2p {

namespace client {

// TODO(deymo): Move this class to libchromeos and merge it with the one in
// update_engine.

// The clock interface allows access to some system time-related functions.
class ClockInterface {
 public:
  // Sleeps for |seconds| seconds. Returns zero if the time has elapsed, or
  // the number of seconds left to sleep if the call was interrupted.
  virtual unsigned int Sleep(unsigned int seconds) = 0;

  // Returns monotonic time since some unspecified starting point. It
  // is not increased when the system is sleeping nor is it affected
  // by NTP or the user changing the time.
  //
  // (This is a simple wrapper around clock_gettime(2) / CLOCK_MONOTONIC_RAW.)
  virtual base::Time GetMonotonicTime() = 0;

  virtual ~ClockInterface() {}
};

}  // namespace p2p

}  // namespace client

#endif  // P2P_CLIENT_CLOCK_INTERFACE_H__
