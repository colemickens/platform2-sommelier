// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "common/testutil.h"

#include <glib-object.h>

#include <gtest/gtest.h>
#include <gtest/gtest-spi.h>

#include <base/file_util.h>

using std::string;
using std::vector;

using base::FilePath;

namespace p2p {

namespace testutil {

TEST(TestUtil, TestDir) {
  FilePath testdir = SetupTestDir("test-dir");

  string path = testdir.value();

  EXPECT_EQ(path.find("/tmp/p2p-testing-test-dir."), 0);

  EXPECT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_EXISTS));
  EXPECT_TRUE(g_file_test(path.c_str(), G_FILE_TEST_IS_DIR));

  int num_files = 0;
  GDir* dir = g_dir_open(path.c_str(), 0, NULL);
  while (g_dir_read_name(dir) != NULL) {
    num_files++;
  }
  g_dir_close(dir);
  EXPECT_EQ(num_files, 0);

  TeardownTestDir(testdir);

  EXPECT_TRUE(!g_file_test(path.c_str(), G_FILE_TEST_EXISTS));
}

TEST(TestUtil, ExpectCommandSimple) {
  ExpectCommand(0, "true");
  ExpectCommand(1, "false");
}

TEST(TestUtil, ExpectCommandSideEffects) {
  FilePath testdir = SetupTestDir("expect-command-side-effects");

  ExpectCommand(
      0, "echo -n xyz > %s", testdir.Append("file.txt").value().c_str());

  string contents;
  EXPECT_TRUE(
      file_util::ReadFileToString(testdir.Append("file.txt"), &contents));
  EXPECT_EQ(contents, "xyz");

  TeardownTestDir(testdir);
}

TEST(TestUtil, FileSize) {
  FilePath testdir = SetupTestDir("expect-file-size");

  ExpectCommand(0, "echo -n 1 > %s", testdir.Append("a").value().c_str());
  ExpectCommand(0, "echo -n 11 > %s", testdir.Append("b").value().c_str());
  ExpectCommand(0, "echo -n 111 > %s", testdir.Append("c").value().c_str());

  ExpectFileSize(testdir, "a", 1);
  ExpectFileSize(testdir, "b", 2);
  ExpectFileSize(testdir, "c", 3);

  TeardownTestDir(testdir);
}

TEST(TestUtil, VectorsEquals) {
  vector<string> vec1;
  vec1.push_back("foo");
  vec1.push_back("bar");
  vec1.push_back("baz");
  vector<string> vec2;
  vec2.push_back("foo");
  vec2.push_back("bar");
  vec2.push_back("baz");
  vec2.push_back("extra");
  vector<string> vec3;
  vec3.push_back("extra");
  vec3.push_back("foo");
  vec3.push_back("bar");
  vec3.push_back("baz");
  vector<string> vec4;
  vec4.push_back("a");
  vec4.push_back("b");
  vec4.push_back("c");
  vector<string> vec5;
  vec5.push_back("d");
  vec5.push_back("e");
  vector<string> vec6;
  vector<string> vec7;
  vec7.push_back("foo");
  vec7.push_back("bar");
  vec7.push_back("baz");
  vector<vector<string>*> vectors;
  vectors.push_back(&vec1);
  vectors.push_back(&vec2);
  vectors.push_back(&vec3);
  vectors.push_back(&vec4);
  vectors.push_back(&vec5);
  vectors.push_back(&vec6);
  vectors.push_back(&vec7);

  for (vector<string>* v1 : vectors) {
    for (vector<string>* v2 : vectors) {
      if ((v1 == v2) || (v1 == &vec1 && v2 == &vec7) ||
          (v2 == &vec1 && v1 == &vec7)) {
        EXPECT_TRUE(VectorsEqual(*v1, *v2));
      } else {
        EXPECT_FALSE(VectorsEqual(*v1, *v2));
      }
    }
  }
}

}  // namespace testutil

}  // namespace p2p
