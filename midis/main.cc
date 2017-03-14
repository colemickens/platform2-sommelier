// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <base/at_exit.h>
#include <base/logging.h>
#include <base/memory/ptr_util.h>
#include <base/message_loop/message_loop.h>
#include <base/run_loop.h>

#include "midis/daemon.h"

int main(int argc, char* argv[]) {
  LOG(INFO) << "Starting MIDI native service\n";
  midis::Daemon daemon;

  daemon.Run();
  return 0;
}
