// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inst_util.h"
#include "chromeos_install_config.h"
#include "chromeos_postinst.h"
#include "chromeos_test_utils.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using std::string;

const char* usage = (
    "cros_installer:\n"
    "   --help\n"
    "   --debug\n"
    "   --test\n"
    "   cros_installer postinst <install_dev> <mount_point> [ args ]\n"
    "     --legacy\n"
    "     --postcommit\n");

int showHelp() {
  printf("%s", usage);
  return 1;
}

bool post_install(const string& install_dir,
                  const string& install_dev,
                  bool legacy) {
  printf("postinst %s %s\n", install_dir.c_str(), install_dev.c_str());

  InstallConfig install_config;

  if (!ConfigureInstall(install_dir, install_dev, &install_config)) {
    printf("Configure failed.\n");
    return false;
  }

  // Log how we are configured.
  printf("PostInstall Configured: (%s, %s, %s, %s)\n",
         install_config.slot.c_str(),
         install_config.root.device().c_str(),
         install_config.kernel.device().c_str(),
         install_config.boot.device().c_str());

  // The normal postinstall is also needed for legacy cases
  if (!RunPostInstall(install_config)) {
    printf("Primary PostInstall failed.\n");
    return false;
  }

  return true;
}

int main(int argc, char** argv) {

  struct option long_options[] = {
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"legacy", no_argument, NULL, 'l'},
    {"postcommit", no_argument, NULL, 'p'},
    {"test", no_argument, NULL, 't'},
    {NULL, 0, NULL, 0},
  };

  bool legacy = false;

  while (true) {
    int option_index;
    int c = getopt_long(argc, argv, "h", long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
      case '?':
        // Unknown argument
        printf("\n");
        return showHelp();

      case 'h':
        // --help
        return showHelp();

      case 'd':
        // I don't think this is used, but older update engines might
        // in some cases. So, it's present but ignored.
        break;

      case 'l':
        // Turns on support for non-Chrome hardware. This breaks support
        // for 32 to 64 bit transitions.
        printf("legacy\n");
        legacy = true;
        break;

      case 'p':
        // This is an outdated argument. When we receive it, we just
        // exit with success right away.
        printf("Received --postcommit. This is a successful no-op.\n");
        exit(0);

      case 't':
        // Cheesy gtest replacement for now...
        Test();
        exit(0);

      default:
        printf("Unknown argument %d - switch and struct out of sync\n\n", c);
        return showHelp();
    }
  }

  if (argc - optind < 1) {
    printf("No command type present (postinst, etc)\n\n");
    return showHelp();
  }

  string command = argv[optind++];

  // Run postinstall behavior
  if (command == "postinst") {
    if (argc - optind != 2)
      return showHelp();

    string install_dir = argv[optind++];
    string install_dev = argv[optind++];

    // ! converts bool to 0 / non-zero exit code
    return !post_install(install_dev, install_dir, legacy);
  }

  printf("Unknown command: '%s'\n\n", command.c_str());
  return showHelp();
}
