// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef P2P_COMMON_UTIL_H__
#define P2P_COMMON_UTIL_H__

namespace p2p {

namespace util {

// Sets up the libbase/libchrome logging infrastructure (e.g. LOG(INFO))
// to use the standard syslog mechanism (e.g. typically
// /var/log/messages). Each log message will be prepended by
// |program_name| and, if |include_pid| is true, the process id.
void SetupSyslog(const char* program_name, bool include_pid);

}  // namespace util

}  // namespace p2p

#endif  // P2P_COMMON_UTIL_H__
