// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "vm_tools/garcon/app_search.h"

#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace vm_tools {
namespace garcon {

namespace {

class AppSearchTest : public ::testing::Test {
 public:
  AppSearchTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    package_tags_path_ = temp_dir_.GetPath().Append("package-tags");
  }
  ~AppSearchTest() override = default;

  void WriteContents(const std::string& file_contents) {
    EXPECT_EQ(file_contents.size(),
              base::WriteFile(package_tags_path_, file_contents.c_str(),
                              file_contents.size()));
  }

  std::string TempFilePath() const { return package_tags_path_.value(); }

 private:
  base::ScopedTempDir temp_dir_;
  base::FilePath package_tags_path_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchTest);
};

}  // namespace

TEST_F(AppSearchTest, NonExistentFile) {
  std::vector<std::string> packages;
  std::string error_msg;
  ParseDebtags(TempFilePath(), &packages, &error_msg);
  std::vector<std::string> expected_packages = {};
  std::string expected_error =
      "package-tags file '" + TempFilePath() + "' does not exist";
  EXPECT_THAT(packages, testing::ContainerEq(expected_packages));
  EXPECT_EQ(error_msg, expected_error);
}

TEST_F(AppSearchTest, ValidParsingResult) {
  std::vector<std::string> packages;
  std::string error_msg;
  WriteContents(
      u8R"xxx(
    package1: devel::compiler, not, debtags, not, interface::graphical
    package2: deve, more, promising
    package3: good, suite::, devel::compiler, long
    package5: interface::graphical, devel::editor
    )xxx");
  std::vector<std::string> expected = {"package1", "package5"};
  ParseDebtags(TempFilePath(), &packages, &error_msg);
  EXPECT_THAT(packages, testing::ContainerEq(expected));
}

TEST_F(AppSearchTest, ValidSearchResult) {
  std::string query = "package5";
  std::vector<std::string> packages = {"package3", "package5"};
  std::vector<std::pair<std::string, float>> expected = {
      std::pair<std::string, float>("package5", 1)};
  EXPECT_THAT(SearchPackages(packages, query), testing::ContainerEq(expected));
}

TEST_F(AppSearchTest, EmptySearchResult) {
  std::string query = "package5";
  std::vector<std::string> packages = {"element1", "element2"};
  std::vector<std::pair<std::string, float>> expected = {};
  EXPECT_THAT(SearchPackages(packages, query), testing::ContainerEq(expected));
}

}  // namespace garcon
}  // namespace vm_tools
