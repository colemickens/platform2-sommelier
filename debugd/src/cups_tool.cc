// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tool to stop cupsd and clear its state. Needs to launch helper with root
// permissions, so we can restart Upstart jobs, and clear privileged
// directories.

#include "debugd/src/cups_tool.h"
#include "debugd/src/process_with_output.h"

#include <errno.h>
#include <ftw.h>
#include <string>
#include <unistd.h>

namespace debugd {

namespace {

const std::string job_name = "cupsd";

int StopCups(DBus::Error* error) {
  int result;

  result = ProcessWithOutput::RunProcess("initctl",
                                                 {"stop", job_name},
                                                 true,      // requires root
                                                 nullptr,   // stdin
                                                 nullptr,   // stdout
                                                 nullptr,   // stderr
                                                 error);

  // Don't log errors, since job may not be started.
  return result;
}


int RemovePath(const char *fpath, const struct stat *sb, int typeflag,
               struct FTW *ftwbuf) {
  int ret = remove(fpath);
  if (ret)
    PLOG(WARNING) << "could not remove file/path: " << fpath;
  return ret;
}

int ClearDirectory(const char *path) {
  if (access(path, F_OK) && errno == ENOENT)
    // Directory doesn't exist. Skip quietly.
    return 0;

  return nftw(path, RemovePath, FTW_D, FTW_DEPTH | FTW_PHYS);
}


int ClearCupsState() {
  int ret = 0;

  ret |= ClearDirectory("/var/cache/cups");
  ret |= ClearDirectory("/var/spool/cups");

  return ret;
}

}  // namespace


void CupsTool::ResetState(DBus::Error* error) {
  // Ignore errors; CUPS may not even be started.
  StopCups(error);

  // There's technically a race -- cups can be restarted in the meantime -- but
  // (a) we don't expect applications to be racing with this (e.g., this method
  // may be used on logout or login) and
  // (b) clearing CUPS's state while it's running should at most confuse CUPS
  // (e.g., missing printers or jobs).
  ClearCupsState();
}

}  // namespace debugd
