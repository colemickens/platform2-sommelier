// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <stdio.h>
#include <getopt.h>

#include "label_detect.h"

int verbose = 0;

/* detector's name and function. detect_func() returns true if the feature is
 * detected. */
struct detector {
  char *name;
  bool (*detect_func)(void);
};

struct detector detectors[] = {
  { "hw_video_acc_h264", detect_video_acc_h264 },
  { "hw_video_acc_vp8", detect_video_acc_vp8 },
  { "hw_video_acc_enc_h264", detect_video_acc_enc_h264 },
  { "hw_video_acc_enc_vp8", detect_video_acc_enc_vp8 },
  { "webcam", detect_webcam },
  { NULL, NULL }
};

int main(int argc, char *argv[]) {
  int i;
  int opt;

  while ((opt = getopt(argc, argv, "vh")) != -1) {
    switch (opt) {
      case 'v':
        verbose = 1;
        break;
      case 'h':
        printf("Usage: %s [-vh]\n", argv[0]);
        return 0;
    }
  }

  for (i = 0; detectors[i].name; i++) {
    TRACE("Detecting [%s]\n", detectors[i].name);
    if (detectors[i].detect_func()) {
      printf("Detected label: %s\n", detectors[i].name);
    }
    TRACE("\n");
  }
  return 0;
}
