// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_number_conversions.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "metrics/crosvm_process_list.h"

namespace chromeos_metrics {

class CrosvmProcessListTest : public testing::Test {
 public:
  CrosvmProcessListTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    slash_proc_ = temp_dir_.GetPath().Append("proc");
    CHECK(base::CreateDirectory(slash_proc_));
  }
  ~CrosvmProcessListTest() override = default;

  // Create a directory |dir_name| under /proc and write |contents| to file
  // |file_name| under this directory.
  void WriteContentsToFileUnderSubdir(const std::string& contents,
                                      const std::string& dir_name,
                                      const std::string& file_name) {
    base::FilePath dir = slash_proc_.Append(dir_name);
    CHECK(base::CreateDirectory(dir));
    base::FilePath file = dir.Append(file_name);
    EXPECT_EQ(contents.size(),
              base::WriteFile(file, contents.c_str(), contents.size()));
  }

 protected:
  base::FilePath slash_proc_;

 private:
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(CrosvmProcessListTest);
};

TEST_F(CrosvmProcessListTest, ConciergeIsTheOnlyCrosvmProcess) {
  pid_t concierge_pid = 2222;
  std::string status_contents =
      "Name:\tvm_concierge\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t2222\n"
      "PPid:\t2\n";
  WriteContentsToFileUnderSubdir(status_contents,
                                 base::IntToString(concierge_pid), "status");
  EXPECT_THAT(GetCrosvmPids(slash_proc_), testing::ElementsAre(concierge_pid));
}

TEST_F(CrosvmProcessListTest, ConciergeNotRunning) {
  EXPECT_TRUE(GetCrosvmPids(slash_proc_).empty());
}

TEST_F(CrosvmProcessListTest, SkipOtherDirAndFile) {
  base::FilePath other_dir = slash_proc_.Append("other_dir");
  CHECK(base::CreateDirectory(other_dir));

  WriteContentsToFileUnderSubdir("other contents", "other_dir", "other_file");
  EXPECT_TRUE(GetCrosvmPids(slash_proc_).empty());
}

TEST_F(CrosvmProcessListTest, SkipOtherProcess) {
  pid_t concierge_pid = 2222;
  std::string status_contents =
      "Name:\tvm_concierge\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t2222\n"
      "PPid:\t2\n";
  WriteContentsToFileUnderSubdir(status_contents,
                                 base::IntToString(concierge_pid), "status");

  pid_t other_pid = 1111;
  std::string other_status_contents =
      "Name:\tother\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t1111\n"
      "PPid:\t2\n";
  WriteContentsToFileUnderSubdir(other_status_contents,
                                 base::IntToString(other_pid), "status");
  EXPECT_THAT(GetCrosvmPids(slash_proc_), testing::ElementsAre(concierge_pid));
}

TEST_F(CrosvmProcessListTest, ChildrenAreIncluded) {
  pid_t concierge_pid = 2222;
  std::string status_contents =
      "Name:\tvm_concierge\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t2222\n"
      "PPid:\t2\n";
  WriteContentsToFileUnderSubdir(status_contents,
                                 base::IntToString(concierge_pid), "status");

  pid_t child_pid = 3333;
  std::string child_status_contents =
      "Name:\tchild\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t3333\n"
      "PPid:\t2222\n";
  WriteContentsToFileUnderSubdir(child_status_contents,
                                 base::IntToString(child_pid), "status");

  pid_t grand_child_pid = 4444;
  std::string grand_child_status_contents =
      "Name:\tgrand_child\n"
      "Umask:\t0000\n"
      "State:\tS (sleeping)\n"
      "Tgid:\t99\n"
      "Ngid:\t0\n"
      "Pid:\t4444\n"
      "PPid:\t3333\n";
  WriteContentsToFileUnderSubdir(grand_child_status_contents,
                                 base::IntToString(grand_child_pid), "status");

  EXPECT_THAT(GetCrosvmPids(slash_proc_),
              testing::ElementsAre(concierge_pid, child_pid, grand_child_pid));
}
}  // namespace chromeos_metrics
