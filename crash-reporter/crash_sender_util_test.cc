// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_util.h"

#include <stdlib.h>

#include <string>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <brillo/flag_helper.h>
#include <gtest/gtest.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/test_util.h"

namespace util {
namespace {

// Prases the output file from fake_crash_sender.sh to a vector of items per
// line. Example:
//
// foo1 foo2
// bar1 bar2
//
// => [["foo1", "foo2"], ["bar1, "bar2"]]
//
std::vector<std::vector<std::string>> ParseFakeCrashSenderOutput(
    const std::string& contents) {
  std::vector<std::vector<std::string>> rows;

  std::vector<std::string> lines = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    std::vector<std::string> items =
        base::SplitString(line, base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    rows.push_back(items);
  }

  return rows;
}

class CrashSenderUtilTest : public testing::Test {
 private:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_dir_ = temp_dir_.GetPath();
    paths::SetPrefixForTesting(test_dir_);
  }

  void TearDown() override {
    paths::SetPrefixForTesting(base::FilePath());

    // ParseCommandLine() sets the environment variables. Reset these here to
    // avoid side effects.
    for (const EnvPair& pair : kEnvironmentVariables)
      unsetenv(pair.name);

    // ParseCommandLine() uses base::CommandLine via
    // brillo::FlagHelper. Reset these here to avoid side effects.
    if (base::CommandLine::InitializedForCurrentProcess())
      base::CommandLine::Reset();
    brillo::FlagHelper::ResetForTesting();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  base::FilePath test_dir_;
};

}  // namespace

TEST_F(CrashSenderUtilTest, ParseCommandLine_MalformedValue) {
  const char* argv[] = {"crash_sender", "-e", "WHATEVER"};
  EXPECT_DEATH(ParseCommandLine(arraysize(argv), argv),
               "Malformed value for -e: WHATEVER");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_UnknownVariable) {
  const char* argv[] = {"crash_sender", "-e", "FOO=123"};
  EXPECT_DEATH(ParseCommandLine(arraysize(argv), argv),
               "Unknown variable name: FOO");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_NoFlags) {
  const char* argv[] = {"crash_sender"};
  ParseCommandLine(arraysize(argv), argv);
  // By default, the value is 0.
  EXPECT_STREQ("0", getenv("FORCE_OFFICIAL"));
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_HonorExistingValue) {
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  const char* argv[] = {"crash_sender"};
  ParseCommandLine(arraysize(argv), argv);
  EXPECT_STREQ("1", getenv("FORCE_OFFICIAL"));
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_OverwriteDefaultValue) {
  const char* argv[] = {"crash_sender", "-e", "FORCE_OFFICIAL=1"};
  ParseCommandLine(arraysize(argv), argv);
  EXPECT_STREQ("1", getenv("FORCE_OFFICIAL"));
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_OverwriteExistingValue) {
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  const char* argv[] = {"crash_sender", "-e", "FORCE_OFFICIAL=2"};
  ParseCommandLine(arraysize(argv), argv);
  EXPECT_STREQ("2", getenv("FORCE_OFFICIAL"));
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_Usage) {
  const char* argv[] = {"crash_sender", "-h"};
  // The third parameter is empty because EXPECT_EXIT does not capture stdout
  // where the usage message is written to.
  EXPECT_EXIT(ParseCommandLine(arraysize(argv), argv),
              testing::ExitedWithCode(0), "");
}

TEST_F(CrashSenderUtilTest, IsMock) {
  EXPECT_FALSE(IsMock());
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kSystemRunStateDirectory, paths::kMockCrashSending),
      ""));
  EXPECT_TRUE(IsMock());
}

TEST_F(CrashSenderUtilTest, ShouldPauseSending) {
  EXPECT_FALSE(ShouldPauseSending());

  ASSERT_TRUE(test_util::CreateFile(paths::Get(paths::kPauseCrashSending), ""));
  EXPECT_FALSE(ShouldPauseSending());

  setenv("OVERRIDE_PAUSE_SENDING", "0", 1 /* overwrite */);
  EXPECT_TRUE(ShouldPauseSending());

  setenv("OVERRIDE_PAUSE_SENDING", "1", 1 /* overwrite */);
  EXPECT_FALSE(ShouldPauseSending());
}

TEST_F(CrashSenderUtilTest, CheckDependencies) {
  base::FilePath missing_path;

  const int permissions = 0755;  // rwxr-xr-x
  const base::FilePath kFind = paths::Get(paths::kFind);
  const base::FilePath kMetricsClient = paths::Get(paths::kMetricsClient);
  const base::FilePath kRestrictedCertificatesDirectory =
      paths::Get(paths::kRestrictedCertificatesDirectory);

  // kFind is the missing path.
  EXPECT_FALSE(CheckDependencies(&missing_path));
  EXPECT_EQ(kFind.value(), missing_path.value());

  // Create kFind and try again.
  ASSERT_TRUE(test_util::CreateFile(kFind, ""));
  ASSERT_TRUE(base::SetPosixFilePermissions(kFind, permissions));
  EXPECT_FALSE(CheckDependencies(&missing_path));
  EXPECT_EQ(kMetricsClient.value(), missing_path.value());

  // Create kMetricsClient and try again.
  ASSERT_TRUE(test_util::CreateFile(kMetricsClient, ""));
  ASSERT_TRUE(base::SetPosixFilePermissions(kMetricsClient, permissions));
  EXPECT_FALSE(CheckDependencies(&missing_path));
  EXPECT_EQ(kRestrictedCertificatesDirectory.value(), missing_path.value());

  // Create kRestrictedCertificatesDirectory and try again.
  ASSERT_TRUE(base::CreateDirectory(kRestrictedCertificatesDirectory));
  EXPECT_TRUE(CheckDependencies(&missing_path));
}

TEST_F(CrashSenderUtilTest, Sender) {
  // Set up the mock sesssion manager client.
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();
  test_util::SetActiveSessions(mock.get(),
                               {{"user1", "hash1"}, {"user2", "hash2"}});

  // Set up the output file for fake_crash_sender.sh.
  const base::FilePath output_file = test_dir_.Append("fake_crash_sender.out");
  setenv("FAKE_CRASH_SENDER_OUTPUT", output_file.value().c_str(),
         1 /* overwrite */);

  // Create crash directories.
  // The crash directory for "user1" is not present, thus should be skipped.
  const base::FilePath system_crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_crash_directory));
  const base::FilePath user2_crash_directory =
      paths::Get("/home/user/hash2/crash");
  ASSERT_TRUE(base::CreateDirectory(user2_crash_directory));

  // Set up the sender.
  Sender::Options options;
  options.shell_script = base::FilePath("fake_crash_sender.sh");
  options.proxy = mock.release();
  Sender sender(options);
  ASSERT_TRUE(sender.Init());

  // Send crashes.
  EXPECT_TRUE(sender.SendCrashes(system_crash_directory));
  EXPECT_TRUE(sender.SendUserCrashes());

  // Check the output file from fake_crash_sender.sh.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(output_file, &contents));
  std::vector<std::vector<std::string>> rows =
      ParseFakeCrashSenderOutput(contents);
  ASSERT_EQ(2, rows.size());

  // The first run should be for the system crash directory.
  std::vector<std::string> row = rows[0];
  ASSERT_EQ(2, row.size());
  EXPECT_EQ(sender.temp_dir().value(), row[0]);
  EXPECT_EQ(system_crash_directory.value(), row[1]);

  // The second run should be for "user2".
  row = rows[1];
  ASSERT_EQ(2, row.size());
  EXPECT_EQ(sender.temp_dir().value(), row[0]);
  EXPECT_EQ(user2_crash_directory.value(), row[1]);
}

}  // namespace util
