// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  char shell_script_path[] = "/sbin/crash_sender.sh";
  argv[0] = shell_script_path;
  execve(argv[0], argv, environ);
  return EXIT_FAILURE;  // execve() failed.
}
