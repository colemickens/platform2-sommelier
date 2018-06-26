// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screenshot/capture.h"
#include "screenshot/crtc.h"
#include "screenshot/png.h"

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    LOG(ERROR) << "Usage: screenshot path/to/out.png";
    return 1;
  }

  auto crtc = screenshot::CrtcFinder::FindAnyDisplay();
  if (!crtc) {
    LOG(ERROR) << "No valid CRTC found. Is the screen on?";
    return 1;
  }

  auto map = screenshot::Capture(*crtc);

  screenshot::SaveAsPng(argv[1], map->buffer(), map->width(), map->height(),
                        map->stride());
  return 0;
}
