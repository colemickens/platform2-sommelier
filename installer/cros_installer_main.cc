// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "inst_util.h"
#include "chromeos_install_config.h"
#include "chromeos_legacy.h"
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
    "     --bios [ secure | legacy | efi | uboot ]\n"
    "     --legacy\n"
    "     --postcommit\n");

int showHelp() {
  printf("%s", usage);
  return 1;
}

bool StrToBiosType(string name, BiosType* bios_type) {
  if (name == "secure")
    *bios_type = kBiosTypeSecure;
  else if (name == "uboot")
    *bios_type = kBiosTypeUBoot;
  else if (name == "legacy")
    *bios_type = kBiosTypeLegacy;
  else if (name == "efi")
    *bios_type = kBiosTypeEFI;
  else {
    printf("Bios type %s is not one of secure, legacy, efi, or uboot\n",
           name.c_str());
    return false;
  }

  return true;
}

int main(int argc, char** argv) {

  struct option long_options[] = {
    {"bios", required_argument, NULL, 'b'},
    {"debug", no_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"postcommit", no_argument, NULL, 'p'},
    {"test", no_argument, NULL, 't'},
    {NULL, 0, NULL, 0},
  };

  // Unkown means we will attempt to autodetect later on.
  BiosType bios_type = kBiosTypeUnknown;

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

      case 'b':
        // Bios type has been explicitly given, don't autodetect
        if (!StrToBiosType(optarg, &bios_type))
          exit(1);
        break;

      case 'd':
        // I don't think this is used, but older update engines might
        // in some cases. So, it's present but ignored.
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
    return !RunPostInstall(install_dev, install_dir, bios_type);
  }

  printf("Unknown command: '%s'\n\n", command.c_str());
  return showHelp();
}
