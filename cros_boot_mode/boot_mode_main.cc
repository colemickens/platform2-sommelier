// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Returns the best guess of system modality.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <err.h>

#include "boot_mode.h"
#include "developer_switch.h"
#include "active_main_firmware.h"
#include "bootloader_type.h"

void usage(const char *self) {
  fprintf(stderr,
    "Usage: %s [options]\n\n"
    "Options:\n"
    "  [empty]		Prints the current mode\n"
    "  -unsupported_as_developer\n"
    "  -u		Treats an unsupported platform as developer mode\n"
    "  -ignore_bootloader\n"
    "  -b		Ignores the bootloader configuration\n"
    "  -strict_match\n"
    "  -s		With -m, performs a strict match\n"
    "  -in_mode\n"
    "  -m [mode]	Tests if the given mode is active instead of printing\n"
    "\n"
    "Mode:\n"
    "  normal\n"
    "  normal recovery\n"
    "  developer\n"
    "  developer recovery\n\n"
    "If supplied, the given text will be matched as a substring against\n"
    "the current mode. If it matches, the exit code will be zero, if not it\n"
    "will be non-zero.\n"
    "If -strict_match is supplied, then the given mode text must match the\n"
    "current running mode:\n"
    "  -m developer matches 'developer' or 'developer recovery'\n"
    "  -s -m developer matches only 'developer'\n\n",
    self);
}

// Use getopt instead of libbase.a so that this is sane to call from
// chromeos_startup.
static int flag_unsupported_as_developer = 0;
static int flag_ignore_bootloader = 0;
static int flag_strict_match = 0;
static int flag_help = 0;
static char *flag_mode = NULL;

static void parse_args(int argc, char **argv) {
  int c = 0;
  while (c >= 0) {
    int option_index = 0;
    static const struct option long_options[] = {
      {"b", no_argument, &flag_ignore_bootloader, 1},
      {"ignore_bootloader", no_argument, &flag_ignore_bootloader, 1},
      {"h", no_argument, &flag_help, 1},
      {"help", no_argument, &flag_help, 1},
      {"m", required_argument, NULL, 'm'},
      {"in_mode", required_argument, NULL, 'm'},
      {"s", no_argument, &flag_strict_match, 1},
      {"strict_match", no_argument, &flag_strict_match, 1},
      {"u", no_argument, &flag_unsupported_as_developer, 1},
      {"unsupported_as_developer", no_argument, &flag_unsupported_as_developer,
       1},
      {0, 0, 0, 0}
    };

    c = getopt_long_only(argc, argv, "", long_options, &option_index);
    switch (c) {
    case 'm':
      flag_mode = optarg;
      break;
    case '?':
      flag_help = 1;
      c = -1;
      break;
    }
  }

  if (flag_strict_match && flag_mode == NULL) {
    flag_help = 1;
    warnx("-s requires -m [mode]");
    return;
  }

  if (optind < argc) {
    fprintf(stderr, "Too many free arguments: %d\n", argc - optind);
    flag_help = 1;
   }
}

int main(int argc, char **argv) {
  // Make the exit codes readable.
  enum { kMatch = 0, kNoMatch };

  parse_args(argc, argv);
  if (flag_help) {
    usage(argv[0]);
    return kNoMatch;
  }

  cros_boot_mode::BootMode mode;
  mode.Initialize(flag_unsupported_as_developer, !flag_ignore_bootloader);

  if (flag_mode) {
    bool match = false;
    // flag_mode may be normal, developer, or even "normal recovery" or
    // "developer recovery".  We shorten the comparison by using the
    // test mode length.  strict_match will enforce the reverse.
    if (flag_strict_match) {
      if (strlen(mode.mode_text()) != strlen(flag_mode))
        return kNoMatch;  // no match
    }
    if (strstr(mode.mode_text(), flag_mode))
      match = true;

    return (match ? kMatch : kNoMatch);
  }

  printf("%s\n", mode.mode_text());
  return 0;
}
