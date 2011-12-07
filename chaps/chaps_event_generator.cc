// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a simple application which fires login / logout events to chapsd.
#include <stdio.h>
#include <stdlib.h>

#include <base/command_line.h>

#include "chaps_proxy.h"

static void usage() {
  printf("Usage:\n");
  printf("  chaps_event_generator login --path=<path> --auth=<auth_data>\n");
  printf("  chaps_event_generator logout --path=<path>\n");
  printf("  chaps_event_generator change --path=<path> "
         "--oldauth=<old_auth_data> --newauth=<new_auth_data>\n");
  exit(1);
}

int main(int argc, char** argv) {
  CommandLine::Init(argc, argv);
  CommandLine* cl = CommandLine::ForCurrentProcess();

  chaps::ChapsProxyImpl proxy;
  if (!proxy.Init()) {
    printf("Failed to initialize proxy.\n");
    exit(-1);
  }
  if (cl->args().empty())
    usage();
  if (cl->args()[0] == "login") {
    proxy.FireLoginEvent(cl->GetSwitchValueASCII("path"),
                         cl->GetSwitchValueASCII("auth"));
  } else if (cl->args()[0] == "logout") {
    proxy.FireLogoutEvent(cl->GetSwitchValueASCII("path"));
  } else if (cl->args()[0] == "change") {
    proxy.FireChangeAuthDataEvent(cl->GetSwitchValueASCII("path"),
                                  cl->GetSwitchValueASCII("oldauth"),
                                  cl->GetSwitchValueASCII("newauth"));
  } else {
    usage();
  }
  return 0;
}
