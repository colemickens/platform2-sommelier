// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/file_reader.h"

#include <string>
#include <vector>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <gtest/gtest.h>

using base::FilePath;
using std::string;
using std::vector;

namespace shill {

class FileReaderTest : public ::testing::Test {
 public:
  void VerifyReadLines(const FilePath& path, const vector<string>& lines) {
    string line;
    EXPECT_FALSE(reader_.ReadLine(&line));
    EXPECT_TRUE(reader_.Open(path));
    for (size_t i = 0; i < lines.size(); ++i) {
      EXPECT_TRUE(reader_.ReadLine(&line));
      EXPECT_EQ(lines[i], line);
    }
    EXPECT_FALSE(reader_.ReadLine(&line));
    reader_.Close();
    EXPECT_FALSE(reader_.ReadLine(&line));
  }

 protected:
  FileReader reader_;
};

TEST_F(FileReaderTest, OpenNonExistentFile) {
  EXPECT_FALSE(reader_.Open(FilePath("a_nonexistent_file")));
}

TEST_F(FileReaderTest, OpenEmptyFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &path));

  EXPECT_TRUE(reader_.Open(path));
  string line;
  EXPECT_FALSE(reader_.ReadLine(&line));
  reader_.Close();
}

TEST_F(FileReaderTest, ReadLine) {
  vector<string> lines;
  lines.push_back("this is");
  lines.push_back("a");
  lines.push_back("");
  lines.push_back("test");
  string content = JoinString(lines, '\n');

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.path(), &path));

  // Test a file not ending with a new-line character
  ASSERT_EQ(content.size(),
            base::WriteFile(path, content.c_str(), content.size()));
  VerifyReadLines(path, lines);

  // Test a file ending with a new-line character
  content.push_back('\n');
  ASSERT_EQ(content.size(),
            base::WriteFile(path, content.c_str(), content.size()));
  VerifyReadLines(path, lines);
}

}  // namespace shill
