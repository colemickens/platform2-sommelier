// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inst_util.h"
#include "chromeos_postinst.h"
#include "chromeos_test_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string>

using std::string;

const char* usage = ("cros_installer:\n"
                     "   --test\n"
                     "   cros_installer postinst <install_dev_name>\n");

void showHelp() {
  printf("%s", usage);
  exit(1);
}


int main(int argc, char** argv) {

  if (argc < 2)
    showHelp();

  string arg1 = argv[1];

  // Show help
  if (arg1 == "--help") {
    showHelp();
  }

  // Run unitests
  if (arg1 == "--test") {
    Test();
    exit(0);
  }

  // Run postinstall behavior
  if (arg1 == "postinst") {
    if (argc != 4)
      showHelp();

    string install_dir = argv[3];
    string install_dev = argv[2];

    printf("postinst %s %s\n", install_dir.c_str(), install_dev.c_str());

    // ! Converts from true for success to 0 for success
    return !RunPostInstall(install_dir, install_dev);
  }

  printf("Unknown command: %s\n\n", arg1.c_str());
  showHelp();
  exit(1);
}
