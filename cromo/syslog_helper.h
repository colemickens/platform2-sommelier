// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROMO_SYSLOG_HELPER_H_
#define CROMO_SYSLOG_HELPER_H_

#include <string>

void SysLogHelperInit();
int SysLogHelperSetLevel(const std::string& level);

#endif  // CROMO_SYSLOG_HELPER_H_
