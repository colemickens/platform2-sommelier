// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSLOG_HELPER_H
#define SYSLOG_HELPER_H

void SysLogHelperInit(void);
int SysLogHelperSetLevel(const std::string& level);

#endif // SYSLOG_HELPER_H
