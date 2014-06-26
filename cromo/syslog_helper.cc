// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <syslog.h>

#include "base/basictypes.h"

// Moved syslog related functionality to this file to avoid
// conflicts with defintions in base/logging.h

int SysLogHelperSetLevel(const std::string& level) {
  int mask = 0;
  const char *cstr = level.c_str();

  if (strcasecmp(cstr, "debug") == 0) {
    mask = LOG_UPTO(LOG_DEBUG);
  } else if (strcasecmp(cstr, "info") == 0) {
    mask = LOG_UPTO(LOG_INFO);
  } else if (strcasecmp(cstr, "warn")  == 0) {
    mask = LOG_UPTO(LOG_WARNING);
  } else if (strcasecmp(cstr, "error") == 0) {
    mask = LOG_UPTO(LOG_ERR);
  } else {
    std::string msg(std::string("Invalid Logging Level: ") + level);
    return -1;
  }
  setlogmask(mask);
  return 0;
}

void SysLogHelperInit(void) {
  // Can't use LOG here, unfortunately :( we don't want it to be an error but we
  // do want it logged regardless of priority level.
  openlog("cromo", LOG_PID, LOG_LOCAL3);
  syslog(LOG_NOTICE, "vcsid %s", VCSID);
  closelog();

  // The gobi-sdk's log level for its TRACE messages is DEBUG. That spews
  // too much info, so by default set the syslog level to INFO.
  SysLogHelperSetLevel("info");
}
