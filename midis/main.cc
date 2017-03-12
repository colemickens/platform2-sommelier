// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/logging.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
  LOG(ERROR) << "MIDIS:Starting MIDI native service\n";
  while (1)
    sleep(10);
  LOG(ERROR) << "MIDIS:Shouldn't reach here\n";

  return 0;
}
