// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/logging.h"
#include "base/string_util.h"

#include "main.h"
#include "utils.h"

#include "all_tests.h"
#include "testbase.h"

using std::string;
using std::vector;

DEFINE_int32(duration, 0, "run tests in a loop for at least this many seconds");
DEFINE_string(tests, "", "colon-separated list of tests to run; "
              "all tests if omitted");
DEFINE_bool(get_board_id, false, "return the board id for checksums");


bool test_is_enabled(glbench::TestBase* test,
                     const vector<string>& enabled_tests) {
  if (enabled_tests.empty())
    return true;

  const char* test_name = test->Name();
  for (vector<string>::const_iterator i = enabled_tests.begin();
       i != enabled_tests.end(); ++i) {
    // This is not very precise, but will do until there's a need for something
    // more flexible.
    if (strstr(test_name, i->c_str()))
      return true;
  }

  return false;
}

int main(int argc, char *argv[]) {
  SetBasePathFromArgv0(argv[0], "src");
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (!Init()) {
    printf("# Failed to initialize.\n");
    return 1;
  }

  if (FLAGS_get_board_id) {
    InitContext();
    printf("%s / %s\n", glGetString(GL_VENDOR), glGetString(GL_RENDERER));
    DestroyContext();
    return 0;
  }

  vector<string> enabled_tests;
  SplitString(FLAGS_tests, ':', &enabled_tests);

  glbench::TestBase* tests[] = {
    glbench::GetSwapTest(),
    glbench::GetClearTest(),
    glbench::GetFillRateTest(),
#if defined(USE_OPENGL)
    glbench::GetWindowManagerCompositingTest(false),
    glbench::GetWindowManagerCompositingTest(true),
#endif
    glbench::GetTriangleSetupTest(),
    glbench::GetYuvToRgbTest(),
    glbench::GetReadPixelTest(),
    glbench::GetAttributeFetchShaderTest(),
    glbench::GetVaryingsAndDdxyShaderTest(),
    glbench::GetTextureUpdateTest(),
  };

  uint64_t done = GetUTime() + 1000000ULL * FLAGS_duration;
  do {
    for (unsigned int i = 0; i < arraysize(tests); i++) {
      if (!test_is_enabled(tests[i], enabled_tests))
        continue;
      InitContext();
      tests[i]->Run();
      DestroyContext();
    }
  } while (GetUTime() < done);

  for (unsigned int i = 0; i < arraysize(tests); i++) {
    delete tests[i];
    tests[i] = NULL;
  }

  return 0;
}
