// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include "common.h"
#include "profiler.h"

Profiler::Profiler(const std::string& perf_location,
                   const std::string& event,
                   const std::string& frequency,
                   const std::string& time,
                   const std::string& output_location) :
  perf_location_(perf_location),
  event_(event),
  frequency_(frequency),
  time_(time),
  output_location_(output_location) {}


int Profiler::DoProfile() {
  std::string base = "sudo " + perf_location_ + " record -a";
  std::string output = " --output=" + output_location_;
  std::string event = " --event=" + event_;
  std::string freq = " -F " + frequency_;
  std::string sleep = " -- sleep " + time_;
  std::string redir = " 2>&1";
  std::string cmd = base + output + event + freq + sleep + redir;
  const char* cmd_c = cmd.c_str();
  FILE* stream = popen(cmd_c, "r");
  if (stream == NULL) {
    syslog(LOG_NOTICE, "Could not run \"%s\"\n", cmd_c);
    syslog(LOG_NOTICE, "Error: %s\n", strerror(errno));
  }
  char buf[PERF_OUTPUT_LINE_LEN];
  while (fgets(buf, PERF_OUTPUT_LINE_LEN, stream) != NULL) {
    syslog(LOG_INFO, "Perf output: %s", buf);
  }
  int ret = pclose(stream);
  if (ret != 0) {
    syslog(LOG_NOTICE, "Perf command \"%s\" failed, return=%d\n", cmd_c, ret);
    if (ret == -1) syslog(LOG_NOTICE, "Error: %s\n", strerror(errno));
  }
  return ret;
}

