// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_util.h"

#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/macros.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <brillo/key_value_store.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <metrics/metrics_library_mock.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/test_util.h"

using testing::HasSubstr;

namespace util {
namespace {

// Enum types for setting the runtime conditions.
enum BuildType { kOfficialBuild, kUnofficialBuild };
enum SessionType { kSignInMode, kGuestMode };
enum MetricsFlag { kMetricsEnabled, kMetricsDisabled };

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

// Helper function for calling GetBasePartOfCrashFile() concisely for tests.
std::string GetBasePartHelper(const std::string& file_name) {
  return GetBasePartOfCrashFile(base::FilePath(file_name)).value();
}

// Helper function for calling base::TouchFile() concisely for tests.
bool TouchFileHelper(const base::FilePath& file_name,
                     base::Time modified_time) {
  return base::TouchFile(file_name, modified_time, modified_time);
}

// Creates lsb-release file with information about the build type.
bool CreateLsbReleaseFile(BuildType type) {
  std::string label = "Official build";
  if (type == kUnofficialBuild)
    label = "Test build";

  return test_util::CreateFile(paths::Get("/etc/lsb-release"),
                               "CHROMEOS_RELEASE_DESCRIPTION=" + label + "\n");
}

class CrashSenderUtilTest : public testing::Test {
 private:
  void SetUp() override {
    metrics_lib_ = std::make_unique<MetricsLibraryMock>();
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
  // Creates test crash files in |crash_directory|. Returns true on success.
  bool CreateTestCrashFiles(const base::FilePath& crash_directory) {
    // These should be kept, since the payload is a known kind and exists.
    good_meta_ = crash_directory.Append("good.meta");
    good_log_ = crash_directory.Append("good.log");
    if (!test_util::CreateFile(good_meta_, "payload=good.log\ndone=1\n"))
      return false;
    if (!test_util::CreateFile(good_log_, ""))
      return false;

    // These should be kept, the payload path is absolute but should be handled
    // properly.
    absolute_meta_ = crash_directory.Append("absolute.meta");
    absolute_log_ = crash_directory.Append("absolute.log");
    if (!test_util::CreateFile(
            absolute_meta_,
            "payload=" + absolute_log_.value() + "\n" + "done=1\n"))
      return false;
    if (!test_util::CreateFile(absolute_log_, ""))
      return false;

    // This should be removed, since metadata is corrupted.
    corrupted_meta_ = crash_directory.Append("corrupted.meta");
    if (!test_util::CreateFile(corrupted_meta_, "!@#$%^&*\ndone=1\n"))
      return false;

    // This should be removed, since no payload info is recorded.
    empty_meta_ = crash_directory.Append("empty.meta");
    if (!test_util::CreateFile(empty_meta_, "done=1\n"))
      return false;

    // This should be removed, since the payload file does not exist.
    nonexistent_meta_ = crash_directory.Append("nonexistent.meta");
    if (!test_util::CreateFile(nonexistent_meta_,
                               "payload=nonexistent.log\n"
                               "done=1\n"))
      return false;

    // These should be removed, since the payload is an unknown kind.
    unknown_meta_ = crash_directory.Append("unknown.meta");
    unknown_xxx_ = crash_directory.Append("unknown.xxx");
    if (!test_util::CreateFile(unknown_meta_,
                               "payload=unknown.xxx\n"
                               "done=1\n"))
      return false;
    if (!test_util::CreateFile(unknown_xxx_, ""))
      return false;

    const base::Time now = base::Time::Now();
    const base::TimeDelta hour = base::TimeDelta::FromHours(1);

    // This should be removed, since the meta file is old.
    old_incomplete_meta_ = crash_directory.Append("old_incomplete.meta");
    if (!test_util::CreateFile(old_incomplete_meta_, "payload=good.log\n"))
      return false;
    if (!TouchFileHelper(old_incomplete_meta_, now - hour * 24))
      return false;

    // This should be removed, since the meta file is new.
    new_incomplete_meta_ = crash_directory.Append("new_incomplete.meta");
    if (!test_util::CreateFile(new_incomplete_meta_, "payload=good.log\n"))
      return false;

    // Update timestamps, so that the return value of GetMetaFiles() is sorted
    // per timestamps correctly.
    if (!TouchFileHelper(good_meta_, now - hour))
      return false;
    if (!TouchFileHelper(absolute_log_, now))
      return false;

    return true;
  }

  // Sets the runtime condtions that affect behaviors of ChooseAction().
  // Returns true on success.
  bool SetConditions(BuildType build_type,
                     SessionType session_type,
                     MetricsFlag metrics_flag) {
    if (!CreateLsbReleaseFile(build_type))
      return false;

    metrics_lib_->set_guest_mode(session_type == kGuestMode);
    metrics_lib_->set_metrics_enabled(metrics_flag == kMetricsEnabled);

    return true;
  }

  std::unique_ptr<MetricsLibraryMock> metrics_lib_;
  base::ScopedTempDir temp_dir_;
  base::FilePath test_dir_;

  base::FilePath good_meta_;
  base::FilePath good_log_;
  base::FilePath absolute_meta_;
  base::FilePath absolute_log_;
  base::FilePath empty_meta_;
  base::FilePath corrupted_meta_;
  base::FilePath nonexistent_meta_;
  base::FilePath unknown_meta_;
  base::FilePath unknown_xxx_;
  base::FilePath old_incomplete_meta_;
  base::FilePath new_incomplete_meta_;
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

TEST_F(CrashSenderUtilTest, GetBasePartOfCrashFile) {
  EXPECT_EQ("1", GetBasePartHelper("1"));
  EXPECT_EQ("1.2", GetBasePartHelper("1.2"));
  EXPECT_EQ("1.2.3", GetBasePartHelper("1.2.3"));
  EXPECT_EQ("1.2.3.4", GetBasePartHelper("1.2.3.4"));
  EXPECT_EQ("1.2.3.4", GetBasePartHelper("1.2.3.4.log"));
  EXPECT_EQ("1.2.3.4", GetBasePartHelper("1.2.3.4.log"));
  EXPECT_EQ("1.2.3.4", GetBasePartHelper("1.2.3.4.log.tar"));
  EXPECT_EQ("1.2.3.4", GetBasePartHelper("1.2.3.4.log.tar.gz"));
  // Directory should be preserved.
  EXPECT_EQ("/d/1.2", GetBasePartHelper("/d/1.2"));
  EXPECT_EQ("/d/1.2.3.4", GetBasePartHelper("/d/1.2.3.4.log"));
  // Dots in directory name should not affect the function.
  EXPECT_EQ("/d.d.d.d/1.2.3.4", GetBasePartHelper("/d.d.d.d/1.2.3.4.log"));
}

TEST_F(CrashSenderUtilTest, RemoveOrphanedCrashFiles) {
  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(crash_directory));

  const base::FilePath new_log = crash_directory.Append("0.0.0.0.log");
  const base::FilePath old1_log = crash_directory.Append("1.1.1.1.log");
  const base::FilePath old1_meta = crash_directory.Append("1.1.1.1.meta");
  const base::FilePath old2_log = crash_directory.Append("2.2.2.2.log");
  const base::FilePath old3_log = crash_directory.Append("3.3.3.3.log");
  const base::FilePath old4_log = crash_directory.Append("4.log");

  base::Time now = base::Time::Now();

  // new_log is new thus should not be removed.
  ASSERT_TRUE(test_util::CreateFile(new_log, ""));

  // old1_log is old but comes with the meta file thus should not be removed.
  ASSERT_TRUE(test_util::CreateFile(old1_log, ""));
  ASSERT_TRUE(test_util::CreateFile(old1_meta, ""));
  ASSERT_TRUE(TouchFileHelper(old1_log, now - base::TimeDelta::FromHours(24)));
  ASSERT_TRUE(TouchFileHelper(old1_meta, now - base::TimeDelta::FromHours(24)));

  // old2_log is old without the meta file thus should be removed.
  ASSERT_TRUE(test_util::CreateFile(old2_log, ""));
  ASSERT_TRUE(TouchFileHelper(old2_log, now - base::TimeDelta::FromHours(24)));

  // old3_log is very old without the meta file thus should be removed.
  ASSERT_TRUE(test_util::CreateFile(old3_log, ""));
  ASSERT_TRUE(TouchFileHelper(old3_log, now - base::TimeDelta::FromDays(365)));

  // old4_log is misnamed, but should be removed since it's old.
  ASSERT_TRUE(test_util::CreateFile(old4_log, ""));
  ASSERT_TRUE(TouchFileHelper(old4_log, now - base::TimeDelta::FromHours(24)));

  RemoveOrphanedCrashFiles(crash_directory);

  // Check what files were removed.
  EXPECT_TRUE(base::PathExists(new_log));
  EXPECT_TRUE(base::PathExists(old1_log));
  EXPECT_TRUE(base::PathExists(old1_meta));
  EXPECT_FALSE(base::PathExists(old2_log));
  EXPECT_FALSE(base::PathExists(old3_log));
  EXPECT_FALSE(base::PathExists(old4_log));
}

TEST_F(CrashSenderUtilTest, ChooseAction) {
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled));

  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(CreateDirectory(crash_directory));
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));

  std::string reason;

  // The following files should be sent.
  EXPECT_EQ(kSend, ChooseAction(good_meta_, metrics_lib_.get(), &reason));
  EXPECT_EQ(kSend, ChooseAction(absolute_meta_, metrics_lib_.get(), &reason));

  // The following files should be ignored.
  EXPECT_EQ(kIgnore,
            ChooseAction(new_incomplete_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Recent incomplete metadata"));

  // The following files should be removed.
  EXPECT_EQ(kRemove, ChooseAction(empty_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Payload is not found"));

  EXPECT_EQ(kRemove,
            ChooseAction(corrupted_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Corrupted metadata"));

  EXPECT_EQ(kRemove,
            ChooseAction(nonexistent_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Missing payload"));

  EXPECT_EQ(kRemove, ChooseAction(unknown_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Unknown kind"));

  EXPECT_EQ(kRemove,
            ChooseAction(old_incomplete_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Removing old incomplete metadata"));

  ASSERT_TRUE(SetConditions(kUnofficialBuild, kSignInMode, kMetricsEnabled));
  EXPECT_EQ(kRemove, ChooseAction(good_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Not an official OS version"));

  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsDisabled));
  EXPECT_EQ(kRemove, ChooseAction(good_meta_, metrics_lib_.get(), &reason));
  EXPECT_THAT(reason, HasSubstr("Crash reporting is disabled"));

  // Valid crash files should be kept in the guest mode.
  ASSERT_TRUE(SetConditions(kOfficialBuild, kGuestMode, kMetricsDisabled));
  EXPECT_EQ(kSend, ChooseAction(good_meta_, metrics_lib_.get(), &reason));
}

TEST_F(CrashSenderUtilTest, RemoveAndPickCrashFiles) {
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled));

  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(CreateDirectory(crash_directory));
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));

  std::vector<base::FilePath> to_send;
  RemoveAndPickCrashFiles(crash_directory, metrics_lib_.get(), &to_send);
  // Check what files were removed.
  EXPECT_TRUE(base::PathExists(good_meta_));
  EXPECT_TRUE(base::PathExists(good_log_));
  EXPECT_TRUE(base::PathExists(absolute_meta_));
  EXPECT_TRUE(base::PathExists(absolute_log_));
  EXPECT_TRUE(base::PathExists(new_incomplete_meta_));
  EXPECT_FALSE(base::PathExists(empty_meta_));
  EXPECT_FALSE(base::PathExists(corrupted_meta_));
  EXPECT_FALSE(base::PathExists(nonexistent_meta_));
  EXPECT_FALSE(base::PathExists(unknown_meta_));
  EXPECT_FALSE(base::PathExists(unknown_xxx_));
  EXPECT_FALSE(base::PathExists(old_incomplete_meta_));
  // Check what files were picked for sending.
  ASSERT_EQ(2, to_send.size());
  EXPECT_EQ(good_meta_.value(), to_send[0].value());
  EXPECT_EQ(absolute_meta_.value(), to_send[1].value());

  // All crash files should be removed for an unofficial build.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kUnofficialBuild, kSignInMode, kMetricsEnabled));
  to_send.clear();
  RemoveAndPickCrashFiles(crash_directory, metrics_lib_.get(), &to_send);
  EXPECT_TRUE(base::IsDirectoryEmpty(crash_directory));
  EXPECT_TRUE(to_send.empty());

  // All crash files should be removed if metrics are disabled.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsDisabled));
  to_send.clear();
  RemoveAndPickCrashFiles(crash_directory, metrics_lib_.get(), &to_send);
  EXPECT_TRUE(base::IsDirectoryEmpty(crash_directory));
  EXPECT_TRUE(to_send.empty());

  // Valid crash files should be kept in the guest mode, thus the directory
  // won't be empty.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kOfficialBuild, kGuestMode, kMetricsDisabled));
  to_send.clear();
  RemoveAndPickCrashFiles(crash_directory, metrics_lib_.get(), &to_send);
  EXPECT_FALSE(base::IsDirectoryEmpty(crash_directory));
  // TODO(satorux): This will become zero, once we move the "skip in guest mode"
  // logic to C++.
  ASSERT_EQ(2, to_send.size());
  EXPECT_EQ(good_meta_.value(), to_send[0].value());
  EXPECT_EQ(absolute_meta_.value(), to_send[1].value());
}

TEST_F(CrashSenderUtilTest, RemoveReportFiles) {
  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(crash_directory));

  const base::FilePath foo_meta = crash_directory.Append("foo.meta");
  const base::FilePath foo_log = crash_directory.Append("foo.log");
  const base::FilePath foo_dmp = crash_directory.Append("foo.dmp");
  const base::FilePath bar_log = crash_directory.Append("bar.log");

  ASSERT_TRUE(test_util::CreateFile(foo_meta, ""));
  ASSERT_TRUE(test_util::CreateFile(foo_log, ""));
  ASSERT_TRUE(test_util::CreateFile(foo_dmp, ""));
  ASSERT_TRUE(test_util::CreateFile(bar_log, ""));

  // This should remove foo.*.
  RemoveReportFiles(foo_meta);
  // This should do nothing because the suffix is not ".meta".
  RemoveReportFiles(bar_log);

  // Check what files were removed.
  EXPECT_FALSE(base::PathExists(foo_meta));
  EXPECT_FALSE(base::PathExists(foo_log));
  EXPECT_FALSE(base::PathExists(foo_dmp));
  EXPECT_TRUE(base::PathExists(bar_log));
}

TEST_F(CrashSenderUtilTest, GetMetaFiles) {
  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(crash_directory));

  // Use unsorted file names, to check that GetMetaFiles() sort files by
  // timestamps, not file names.
  const base::FilePath meta_1 = crash_directory.Append("a.meta");
  const base::FilePath meta_2 = crash_directory.Append("s.meta");
  const base::FilePath meta_3 = crash_directory.Append("d.meta");
  const base::FilePath meta_4 = crash_directory.Append("f.meta");
  // This one should not appear in the result.
  const base::FilePath metal_5 = crash_directory.Append("g.metal");

  ASSERT_TRUE(test_util::CreateFile(meta_1, ""));
  ASSERT_TRUE(test_util::CreateFile(meta_2, ""));
  ASSERT_TRUE(test_util::CreateFile(meta_3, ""));
  ASSERT_TRUE(test_util::CreateFile(meta_4, ""));
  ASSERT_TRUE(test_util::CreateFile(metal_5, ""));

  // Change timestamps so that meta_1 is the newest and metal_5 is the oldest.
  base::Time now = base::Time::Now();
  ASSERT_TRUE(TouchFileHelper(meta_1, now - base::TimeDelta::FromHours(1)));
  ASSERT_TRUE(TouchFileHelper(meta_2, now - base::TimeDelta::FromHours(2)));
  ASSERT_TRUE(TouchFileHelper(meta_3, now - base::TimeDelta::FromHours(3)));
  ASSERT_TRUE(TouchFileHelper(meta_4, now - base::TimeDelta::FromHours(4)));
  ASSERT_TRUE(TouchFileHelper(metal_5, now - base::TimeDelta::FromHours(5)));

  std::vector<base::FilePath> meta_files = GetMetaFiles(crash_directory);
  ASSERT_EQ(4, meta_files.size());
  // Confirm that files are sorted in the old-to-new order.
  EXPECT_EQ(meta_4.value(), meta_files[0].value());
  EXPECT_EQ(meta_3.value(), meta_files[1].value());
  EXPECT_EQ(meta_2.value(), meta_files[2].value());
  EXPECT_EQ(meta_1.value(), meta_files[3].value());
}

TEST_F(CrashSenderUtilTest, GetBaseNameFromMetadata) {
  brillo::KeyValueStore metadata;
  metadata.LoadFromString("");
  EXPECT_EQ("", GetBaseNameFromMetadata(metadata, "payload").value());

  metadata.LoadFromString("payload=test.log\n");
  EXPECT_EQ("test.log", GetBaseNameFromMetadata(metadata, "payload").value());

  metadata.LoadFromString("payload=/foo/test.log\n");
  EXPECT_EQ("test.log", GetBaseNameFromMetadata(metadata, "payload").value());
}

TEST_F(CrashSenderUtilTest, GetKindFromPayloadPath) {
  EXPECT_EQ("", GetKindFromPayloadPath(base::FilePath()));
  EXPECT_EQ("", GetKindFromPayloadPath(base::FilePath("foo")));
  EXPECT_EQ("log", GetKindFromPayloadPath(base::FilePath("foo.log")));
  // "dmp" is a special case.
  EXPECT_EQ("minidump", GetKindFromPayloadPath(base::FilePath("foo.dmp")));

  // ".gz" should be ignored.
  EXPECT_EQ("log", GetKindFromPayloadPath(base::FilePath("foo.log.gz")));
  EXPECT_EQ("minidump", GetKindFromPayloadPath(base::FilePath("foo.dmp.gz")));
  EXPECT_EQ("", GetKindFromPayloadPath(base::FilePath("foo.gz")));

  // The directory name should not afect the function.
  EXPECT_EQ("minidump",
            GetKindFromPayloadPath(base::FilePath("/1.2.3/foo.dmp.gz")));
}

TEST_F(CrashSenderUtilTest, ParseMetadata) {
  brillo::KeyValueStore metadata;
  EXPECT_TRUE(ParseMetadata("", &metadata));
  EXPECT_TRUE(ParseMetadata("log=test.log\n", &metadata));
  EXPECT_TRUE(ParseMetadata("#comment\nlog=test.log\n", &metadata));

  // Underscores, dashes, and periods should allowed, as Chrome uses them.
  // https://crbug.com/821530.
  EXPECT_TRUE(ParseMetadata("abcABC012_.-=test.log\n", &metadata));
  std::string value;
  EXPECT_TRUE(metadata.GetString("abcABC012_.-", &value));
  EXPECT_EQ("test.log", value);

  // Invalid metadata should be detected.
  EXPECT_FALSE(ParseMetadata("=test.log\n", &metadata));
  EXPECT_FALSE(ParseMetadata("***\n", &metadata));
  EXPECT_FALSE(ParseMetadata("***=test.log\n", &metadata));
  EXPECT_FALSE(ParseMetadata("log\n", &metadata));
}

TEST_F(CrashSenderUtilTest, IsCompleteMetadata) {
  brillo::KeyValueStore metadata;
  metadata.LoadFromString("");
  EXPECT_FALSE(IsCompleteMetadata(metadata));

  metadata.LoadFromString("log=test.log\n");
  EXPECT_FALSE(IsCompleteMetadata(metadata));

  metadata.LoadFromString("log=test.log\ndone=1\n");
  EXPECT_TRUE(IsCompleteMetadata(metadata));

  metadata.LoadFromString("done=1\n");
  EXPECT_TRUE(IsCompleteMetadata(metadata));
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

  // Create the system crash directory, and crash files in it.
  const base::FilePath system_dir = paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_dir));
  const base::FilePath system_meta = system_dir.Append("0.0.0.0.meta");
  const base::FilePath system_log = system_dir.Append("0.0.0.0.log");
  ASSERT_TRUE(test_util::CreateFile(system_meta,
                                    "payload=0.0.0.0.log\n"
                                    "done=1\n"));
  ASSERT_TRUE(test_util::CreateFile(system_log, ""));

  // Create a user crash directory, and crash files in it.
  // The crash directory for "user1" is not present, thus should be skipped.
  const base::FilePath user2_dir = paths::Get("/home/user/hash2/crash");
  ASSERT_TRUE(base::CreateDirectory(user2_dir));
  const base::FilePath user2_meta = user2_dir.Append("0.0.0.0.meta");
  const base::FilePath user2_log = user2_dir.Append("0.0.0.0.log");
  ASSERT_TRUE(test_util::CreateFile(user2_meta,
                                    "payload=0.0.0.0.log\n"
                                    "done=1\n"));
  ASSERT_TRUE(test_util::CreateFile(user2_log, ""));

  // Set up the conditions so the crash reports can be sent.
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled));

  // Set up the sender.
  Sender::Options options;
  options.shell_script = base::FilePath("fake_crash_sender.sh");
  options.proxy = mock.release();
  Sender sender(std::move(metrics_lib_), options);
  ASSERT_TRUE(sender.Init());

  // Send crashes.
  EXPECT_TRUE(sender.SendCrashes(system_dir));
  EXPECT_TRUE(sender.SendUserCrashes());

  // Check the output file from fake_crash_sender.sh.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(output_file, &contents));
  std::vector<std::vector<std::string>> rows =
      ParseFakeCrashSenderOutput(contents);
  ASSERT_EQ(2, rows.size());

  // The first run should be for the meta file in the system directory.
  std::vector<std::string> row = rows[0];
  ASSERT_EQ(2, row.size());
  EXPECT_EQ(sender.temp_dir().value(), row[0]);
  EXPECT_EQ(system_meta.value(), row[1]);

  // The second run should be for the meta file in the "user2" directory.
  row = rows[1];
  ASSERT_EQ(2, row.size());
  EXPECT_EQ(sender.temp_dir().value(), row[0]);
  EXPECT_EQ(user2_meta.value(), row[1]);
}

}  // namespace util
