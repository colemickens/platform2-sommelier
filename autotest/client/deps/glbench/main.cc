// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "base/logging.h"

#include "main.h"
#include "utils.h"
#include "yuv2rgb.h"

#include "all_tests.h"
#include "testbase.h"


const int enabled_tests_max = 8;
// TODO: fix enabled_tests.
const char *enabled_tests[enabled_tests_max+1] = {NULL};
int seconds_to_run = 0;


// TODO: use proper command line parsing library.
static void ParseArgs(int argc, char *argv[]) {
  const char **enabled_tests_ptr = enabled_tests;
  bool test_name_arg = false;
  bool duration_arg = false;
  for (int i = 0; i < argc; i++) {
    if (test_name_arg) {
      test_name_arg = false;
      *enabled_tests_ptr++ = argv[i];
      if (enabled_tests_ptr - enabled_tests >= enabled_tests_max)
        break;
    } else if (duration_arg) {
      duration_arg = false;
      seconds_to_run = atoi(argv[i]);
    } else if (strcmp("-t", argv[i]) == 0) {
      test_name_arg = true;
    } else if (strcmp("-d", argv[i]) == 0) {
      duration_arg = true;
    }
  }
  *enabled_tests_ptr++ = NULL;
}


int main(int argc, char *argv[]) {
  SetBasePathFromArgv0(argv[0], "src");
  ParseArgs(argc, argv);
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  glbench::TestBase* tests[] = {
    glbench::GetSwapTest(),
    glbench::GetClearTest(),
    glbench::GetFillRateTest(),
    glbench::GetYuvToRgbTest(1, "yuv_shader_1"),
    glbench::GetYuvToRgbTest(2, "yuv_shader_2"),
    glbench::GetReadPixelTest(),
    glbench::GetTriangleSetupTest(),
    glbench::GetAttributeFetchShaderTest(),
    glbench::GetVaryingsAndDdxyShaderTest(),
    glbench::GetWindowManagerCompositingTest(false),
    glbench::GetWindowManagerCompositingTest(true),
    glbench::GetTextureUpdateTest(),
  };

  for (unsigned int i = 0; i < arraysize(tests); i++) {
    InitContext();
    tests[i]->Run();
    DestroyContext();
    delete tests[i];
    tests[i] = NULL;
  }

  return 0;
}
