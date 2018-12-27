// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ARC_NETWORK_OPTIONS_H_
#define ARC_NETWORK_OPTIONS_H_

#include <sys/types.h>

#include <string>

namespace arc_networkd {

// Stores the options parsed from the command line.  Shared with
// the main process and any subprocesses.
struct Options {
  std::string int_ifname;
  std::string mdns_ipaddr;
  std::string con_ifname;
  pid_t con_netns;
};

}  // namespace arc_networkd

#endif  // ARC_NETWORK_OPTIONS_H_
