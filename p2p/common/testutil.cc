// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/testutil.h"

#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <glib-object.h>

#include <cctype>
#include <cinttypes>
#include <cstdlib>

#include <gtest/gtest.h>

using std::string;

using base::FilePath;

namespace p2p {

namespace testutil {

void ExpectCommand(int expected_exit_status,
                   const char* format,
                   ...) {
  va_list args;
  char buf[8192];

  va_start(args, format);
  vsnprintf(buf, sizeof buf, format, args);
  va_end(args);

  int rc = system(buf);

  EXPECT_TRUE(WIFEXITED(rc));
  EXPECT_EQ(WEXITSTATUS(rc), expected_exit_status);
}

FilePath SetupTestDir(const string& test_name) {
  // Create testing directory
  FilePath ret;
  string dir_name = string("/tmp/p2p-testing-") + test_name + ".XXXXXX";
  char* buf = strdup(dir_name.c_str());
  EXPECT_TRUE(mkdtemp(buf) != NULL);
  ret = FilePath(buf);
  free(buf);
  return ret;
}

void TeardownTestDir(const FilePath& dir_path) {
  // Sanity check
  EXPECT_TRUE(dir_path.value().find("/tmp/p2p-testing-") == 0);
  ExpectCommand(0, "rm -rf %s", dir_path.value().c_str());
}

static gboolean RunGMainLoopOnTimeout(gpointer user_data) {
  GMainLoop* loop = static_cast<GMainLoop*>(user_data);
  g_main_loop_quit(loop);
  return FALSE;  // Remove timeout source
}

void RunGMainLoop(int timeout_msec) {
  GMainLoop* loop = g_main_loop_new(NULL, FALSE);
  g_timeout_add(timeout_msec, p2p::testutil::RunGMainLoopOnTimeout, loop);
  g_main_loop_run(loop);
  g_main_loop_unref(loop);
}

size_t FileSize(const FilePath& dir,
                const string& file_name) {
  struct stat stat_buf;
  FilePath path = dir.Append(file_name);
  if (stat(path.value().c_str(), &stat_buf) != 0) {
    return 0;
  }
  return stat_buf.st_size;
}

void ExpectFileSize(const FilePath& dir,
                    const string& file_name,
                    size_t expected_size) {
  EXPECT_EQ(FileSize(dir, file_name), expected_size);
}

}  // namespace testutil

}  // namespace p2p
