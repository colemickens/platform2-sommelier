// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_util.h"

#include <stdlib.h>

#include <string>
#include <utility>
#include <vector>

#include <base/command_line.h>
#include <base/files/file_enumerator.h>
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
using testing::UnorderedElementsAre;

namespace util {
namespace {

// Enum types for setting the runtime conditions.
enum BuildType { kOfficialBuild, kUnofficialBuild };
enum SessionType { kSignInMode, kGuestMode };
enum MetricsFlag { kMetricsEnabled, kMetricsDisabled };

constexpr char kFakeClientId[] = "00112233445566778899aabbccddeeff";

// Parses the Chrome uploads.log file from Sender to a vector of items per line.
// Example:
//
// foo1,foo2
// bar1,bar2
//
// => [["foo1", "foo2"], ["bar1, "bar2"]]
//
std::vector<std::vector<std::string>> ParseChromeUploadsLog(
    const std::string& contents) {
  std::vector<std::vector<std::string>> rows;

  std::vector<std::string> lines = base::SplitString(
      contents, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const auto& line : lines) {
    std::vector<std::string> items = base::SplitString(
        line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
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

// Creates a file that indicates uploading of device coredumps is allowed.
bool CreateDeviceCoredumpUploadAllowedFile() {
  return test_util::CreateFile(
      paths::GetAt(paths::kCrashReporterStateDirectory,
                   paths::kDeviceCoredumpUploadAllowed),
      "");
}

// Creates the client ID file and stores the fake client ID in it.
bool CreateClientIdFile() {
  return test_util::CreateFile(
      paths::GetAt(paths::kCrashSenderStateDirectory, paths::kClientId),
      kFakeClientId);
}

// Returns file names found in |directory|.
std::vector<base::FilePath> GetFileNamesIn(const base::FilePath& directory) {
  std::vector<base::FilePath> files;
  base::FileEnumerator iter(directory, false /* recursive */,
                            base::FileEnumerator::FILES, "*");
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next())
    files.push_back(file);
  return files;
}

// Fake sleep function that records the requested sleep time.
void FakeSleep(std::vector<base::TimeDelta>* sleep_times,
               base::TimeDelta duration) {
  sleep_times->push_back(duration);
}

// Set the file flag which indicates we are mocking crash sending, either
// successfully or as a a failure. This also creates the directory where
// uploads.log is written to since Chrome would normally be doing that.
bool SetMockCrashSending(bool success) {
  return test_util::CreateFile(paths::GetAt(paths::kSystemRunStateDirectory,
                                            paths::kMockCrashSending),
                               success ? "" : "0") &&
         base::CreateDirectory(paths::Get(paths::kChromeCrashLog).DirName());
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

    // These should be ignored, if uploading of device coredumps is not allowed.
    devcore_meta_ = crash_directory.Append("devcore.meta");
    devcore_devcore_ = crash_directory.Append("devcore.devcore");
    if (!test_util::CreateFile(devcore_meta_,
                               "payload=devcore.devcore\n"
                               "done=1\n"))
      return false;
    if (!test_util::CreateFile(devcore_devcore_, ""))
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
    if (!TouchFileHelper(good_meta_, now - hour * 2))
      return false;
    if (!TouchFileHelper(absolute_meta_, now - hour))
      return false;
    if (!TouchFileHelper(devcore_meta_, now))
      return false;

    return true;
  }

  // Sets the runtime conditions that affect behaviors of ChooseAction().
  // Returns true on success.
  bool SetConditions(BuildType build_type,
                     SessionType session_type,
                     MetricsFlag metrics_flag) {
    return SetConditions(build_type, session_type, metrics_flag,
                         metrics_lib_.get());
  }

  // Version of SetConditions useful for tests that need to create a Sender.
  // Sender owns the MetricsLibraryInterface pointer, so metrics_lib_ is
  // usually nullptr in these tests.
  static bool SetConditions(BuildType build_type,
                            SessionType session_type,
                            MetricsFlag metrics_flag,
                            MetricsLibraryMock* metrics_lib) {
    if (!CreateLsbReleaseFile(build_type))
      return false;

    metrics_lib->set_guest_mode(session_type == kGuestMode);
    metrics_lib->set_metrics_enabled(metrics_flag == kMetricsEnabled);

    return true;
  }

  std::unique_ptr<MetricsLibraryMock> metrics_lib_;
  base::ScopedTempDir temp_dir_;
  base::FilePath test_dir_;

  base::FilePath good_meta_;
  base::FilePath good_log_;
  base::FilePath absolute_meta_;
  base::FilePath absolute_log_;
  base::FilePath devcore_meta_;
  base::FilePath devcore_devcore_;
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
  CommandLineFlags flags;
  EXPECT_DEATH(ParseCommandLine(arraysize(argv), argv, &flags),
               "Malformed value for -e: WHATEVER");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_UnknownVariable) {
  const char* argv[] = {"crash_sender", "-e", "FOO=123"};
  CommandLineFlags flags;
  EXPECT_DEATH(ParseCommandLine(arraysize(argv), argv, &flags),
               "Unknown variable name: FOO");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_NoFlags) {
  const char* argv[] = {"crash_sender"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  // By default, the value is 0.
  EXPECT_STREQ("0", getenv("FORCE_OFFICIAL"));
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_HonorExistingValue) {
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  const char* argv[] = {"crash_sender"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_STREQ("1", getenv("FORCE_OFFICIAL"));
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_OverwriteDefaultValue) {
  const char* argv[] = {"crash_sender", "-e", "FORCE_OFFICIAL=1"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_STREQ("1", getenv("FORCE_OFFICIAL"));
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_OverwriteExistingValue) {
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  const char* argv[] = {"crash_sender", "-e", "FORCE_OFFICIAL=2"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_STREQ("2", getenv("FORCE_OFFICIAL"));
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_Usage) {
  const char* argv[] = {"crash_sender", "-h"};
  // The third parameter is empty because EXPECT_EXIT does not capture stdout
  // where the usage message is written to.
  CommandLineFlags flags;
  EXPECT_EXIT(ParseCommandLine(arraysize(argv), argv, &flags),
              testing::ExitedWithCode(EXIT_SUCCESS), "");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_InvalidMaxSpreadTime) {
  const char* argv[] = {"crash_sender", "--max_spread_time=-1"};
  CommandLineFlags flags;
  EXPECT_EXIT(ParseCommandLine(arraysize(argv), argv, &flags),
              testing::ExitedWithCode(EXIT_FAILURE), "Invalid");
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_ValidMaxSpreadTime) {
  const char* argv[] = {"crash_sender", "--max_spread_time=0"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), flags.max_spread_time);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_IgnoreRateLimits) {
  const char* argv[] = {"crash_sender", "--ignore_rate_limits"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_TRUE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_IgnoreHoldOffTime) {
  const char* argv[] = {"crash_sender", "--ignore_hold_off_time"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_TRUE(flags.crash_directory.empty());
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_TRUE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, ParseCommandLine_CrashDirectory) {
  const char* argv[] = {"crash_sender", "--crash_directory=/tmp"};
  CommandLineFlags flags;
  ParseCommandLine(arraysize(argv), argv, &flags);
  EXPECT_EQ(flags.max_spread_time.InSeconds(), kMaxSpreadTimeInSeconds);
  EXPECT_EQ(flags.crash_directory, "/tmp");
  EXPECT_FALSE(flags.ignore_rate_limits);
  EXPECT_FALSE(flags.ignore_hold_off_time);
}

TEST_F(CrashSenderUtilTest, IsMock) {
  EXPECT_FALSE(IsMock());
  ASSERT_TRUE(SetMockCrashSending(false));
  EXPECT_TRUE(IsMock());
  EXPECT_FALSE(IsMockSuccessful());
  ASSERT_TRUE(SetMockCrashSending(true));
  EXPECT_TRUE(IsMock());
  EXPECT_TRUE(IsMockSuccessful());
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

TEST_F(CrashSenderUtilTest, GetImageType) {
  EXPECT_EQ("", GetImageType());
  ASSERT_TRUE(SetMockCrashSending(false));
  EXPECT_EQ("mock-fail", GetImageType());
  setenv("FORCE_OFFICIAL", "1", 1 /* overwrite */);
  EXPECT_EQ("force-official", GetImageType());
  unsetenv("FORCE_OFFICIAL");
  ASSERT_TRUE(test_util::CreateFile(paths::Get(paths::kLeaveCoreFile), ""));
  EXPECT_EQ("dev", GetImageType());
  ASSERT_TRUE(test_util::CreateFile(
      paths::GetAt(paths::kEtcDirectory, paths::kLsbRelease),
      "CHROMEOS_RELEASE_TRACK=testimage-channel"));
  EXPECT_EQ("test", GetImageType());
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

  CrashInfo info;
  // The following files should be sent.
  EXPECT_EQ(kSend,
            ChooseAction(good_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_EQ(kSend,
            ChooseAction(absolute_meta_, metrics_lib_.get(), &reason, &info));
  // Sanity check that the valid crash info is returned.
  std::string value;
  EXPECT_EQ(absolute_log_.value(), info.payload_file.value());
  EXPECT_EQ("log", info.payload_kind);
  EXPECT_TRUE(info.metadata.GetString("payload", &value));

  // The following files should be ignored.
  EXPECT_EQ(kIgnore, ChooseAction(new_incomplete_meta_, metrics_lib_.get(),
                                  &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Recent incomplete metadata"));

  // Device coredump should be ignored by default.
  EXPECT_EQ(kIgnore,
            ChooseAction(devcore_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Device coredump upload not allowed"));

  // Device coredump should be sent, if uploading is allowed.
  CreateDeviceCoredumpUploadAllowedFile();
  EXPECT_EQ(kSend,
            ChooseAction(devcore_meta_, metrics_lib_.get(), &reason, &info));

  // The following files should be removed.
  EXPECT_EQ(kRemove,
            ChooseAction(empty_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Payload is not found"));

  EXPECT_EQ(kRemove,
            ChooseAction(corrupted_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Corrupted metadata"));

  EXPECT_EQ(kRemove, ChooseAction(nonexistent_meta_, metrics_lib_.get(),
                                  &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Missing payload"));

  EXPECT_EQ(kRemove,
            ChooseAction(unknown_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Unknown kind"));

  EXPECT_EQ(kRemove, ChooseAction(old_incomplete_meta_, metrics_lib_.get(),
                                  &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Removing old incomplete metadata"));

  ASSERT_TRUE(SetConditions(kUnofficialBuild, kSignInMode, kMetricsEnabled));
  EXPECT_EQ(kRemove,
            ChooseAction(good_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Not an official OS version"));

  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsDisabled));
  EXPECT_EQ(kRemove,
            ChooseAction(good_meta_, metrics_lib_.get(), &reason, &info));
  EXPECT_THAT(reason, HasSubstr("Crash reporting is disabled"));

  // Valid crash files should be kept in the guest mode.
  ASSERT_TRUE(SetConditions(kOfficialBuild, kGuestMode, kMetricsDisabled));
  EXPECT_EQ(kSend,
            ChooseAction(good_meta_, metrics_lib_.get(), &reason, &info));
}

TEST_F(CrashSenderUtilTest, RemoveAndPickCrashFiles) {
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();
  test_util::SetActiveSessions(mock.get(),
                               {{"user1", "hash1"}, {"user2", "hash2"}});
  MetricsLibraryMock* raw_metrics_lib = metrics_lib_.get();

  Sender::Options options;
  options.proxy = mock.release();
  Sender sender(std::move(metrics_lib_), options);
  ASSERT_TRUE(sender.Init());

  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled,
                            raw_metrics_lib));

  const base::FilePath crash_directory =
      paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(CreateDirectory(crash_directory));
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));

  std::vector<MetaFile> to_send;
  sender.RemoveAndPickCrashFiles(crash_directory, &to_send);

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
  EXPECT_EQ(good_meta_.value(), to_send[0].first.value());
  EXPECT_EQ(absolute_meta_.value(), to_send[1].first.value());

  // Sanity check that the valid crash info is returned.
  std::string value;
  EXPECT_EQ(good_log_.value(), to_send[0].second.payload_file.value());
  EXPECT_EQ("log", to_send[0].second.payload_kind);
  EXPECT_TRUE(to_send[0].second.metadata.GetString("payload", &value));

  // All crash files should be removed for an unofficial build.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kUnofficialBuild, kSignInMode, kMetricsEnabled,
                            raw_metrics_lib));
  to_send.clear();
  sender.RemoveAndPickCrashFiles(crash_directory, &to_send);
  EXPECT_TRUE(base::IsDirectoryEmpty(crash_directory));
  EXPECT_TRUE(to_send.empty());

  // All crash files should be removed if metrics are disabled.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsDisabled,
                            raw_metrics_lib));
  to_send.clear();
  sender.RemoveAndPickCrashFiles(crash_directory, &to_send);
  EXPECT_TRUE(base::IsDirectoryEmpty(crash_directory));
  EXPECT_TRUE(to_send.empty());

  // Valid crash files should be kept in the guest mode, thus the directory
  // won't be empty.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kOfficialBuild, kGuestMode, kMetricsDisabled,
                            raw_metrics_lib));
  to_send.clear();
  sender.RemoveAndPickCrashFiles(crash_directory, &to_send);
  EXPECT_FALSE(base::IsDirectoryEmpty(crash_directory));
  // TODO(satorux): This will become zero, once we move the "skip in guest mode"
  // logic to C++.
  ASSERT_EQ(2, to_send.size());
  EXPECT_EQ(good_meta_.value(), to_send[0].first.value());
  EXPECT_EQ(absolute_meta_.value(), to_send[1].first.value());

  // devcore_meta_ should be included in to_send, if uploading of device
  // coredumps is allowed.
  ASSERT_TRUE(CreateTestCrashFiles(crash_directory));
  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled,
                            raw_metrics_lib));
  CreateDeviceCoredumpUploadAllowedFile();
  to_send.clear();
  sender.RemoveAndPickCrashFiles(crash_directory, &to_send);
  ASSERT_EQ(3, to_send.size());
  EXPECT_EQ(devcore_meta_.value(), to_send[2].first.value());
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
  std::string value;
  EXPECT_TRUE(ParseMetadata("", &metadata));
  EXPECT_TRUE(ParseMetadata("log=test.log\n", &metadata));
  EXPECT_TRUE(ParseMetadata("#comment\nlog=test.log\n", &metadata));

  EXPECT_TRUE(metadata.GetString("log", &value));
  // This will clear the previouly parsed data.
  EXPECT_TRUE(ParseMetadata("payload=test.dmp\n", &metadata));
  EXPECT_FALSE(metadata.GetString("log", &value));

  // Underscores, dashes, and periods should allowed, as Chrome uses them.
  // https://crbug.com/821530.
  EXPECT_TRUE(ParseMetadata("abcABC012_.-=test.log\n", &metadata));
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

TEST_F(CrashSenderUtilTest, IsTimestampNewEnough) {
  base::FilePath file;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(test_dir_, &file));

  // Should be new enough as it's just created.
  ASSERT_TRUE(IsTimestampNewEnough(file));

  // Make it older than 24 hours.
  const base::Time now = base::Time::Now();
  ASSERT_TRUE(TouchFileHelper(file, now - base::TimeDelta::FromHours(25)));

  // Should be no longer new enough.
  ASSERT_FALSE(IsTimestampNewEnough(file));
}

TEST_F(CrashSenderUtilTest, IsBelowRate) {
  const int kMaxRate = 3;

  EXPECT_TRUE(IsBelowRate(test_dir_, kMaxRate));
  EXPECT_TRUE(IsBelowRate(test_dir_, kMaxRate));
  EXPECT_TRUE(IsBelowRate(test_dir_, kMaxRate));

  // Should not pass the rate limit.
  EXPECT_FALSE(IsBelowRate(test_dir_, kMaxRate));

  // Three files should be created for tracking timestamps.
  std::vector<base::FilePath> files = GetFileNamesIn(test_dir_);
  ASSERT_EQ(3, files.size());

  const base::Time now = base::Time::Now();

  // Make one of them older than 24 hours.
  ASSERT_TRUE(TouchFileHelper(files[0], now - base::TimeDelta::FromHours(25)));

  // It should now pass the rate limit.
  EXPECT_TRUE(IsBelowRate(test_dir_, kMaxRate));
  // The old file should now be gone. However, it's possible that the file
  // that's just deleted with its random name is randomly picked again to create
  // the new timestamp file.
  EXPECT_TRUE(!base::PathExists(files[0]) ||
              (base::PathExists(files[0]) && IsTimestampNewEnough(files[0])));

  // There should be three files now since the last call to IsBelowRate() should
  // create a new timestamp file.
  ASSERT_EQ(3, GetFileNamesIn(test_dir_).size());
}

TEST_F(CrashSenderUtilTest, GetSleepTime) {
  const base::FilePath meta_file = test_dir_.Append("test.meta");
  base::TimeDelta max_spread_time = base::TimeDelta::FromSeconds(0);

  // This should fail since meta_file does not exist.
  base::TimeDelta sleep_time;
  EXPECT_FALSE(
      GetSleepTime(meta_file, max_spread_time, kMaxHoldOffTime, &sleep_time));

  ASSERT_TRUE(test_util::CreateFile(meta_file, ""));

  // sleep_time should be close enough to kMaxHoldOffTime since the meta file
  // was just created, but 10% error is allowed just in case.
  EXPECT_TRUE(
      GetSleepTime(meta_file, max_spread_time, kMaxHoldOffTime, &sleep_time));
  EXPECT_NEAR(kMaxHoldOffTime.InSecondsF(), sleep_time.InSecondsF(),
              kMaxHoldOffTime.InSecondsF() * 0.1);

  // Zero hold-off time and zero sleep time should always give zero sleep time.
  EXPECT_TRUE(GetSleepTime(meta_file, max_spread_time,
                           base::TimeDelta::FromSeconds(0) /*hold_off_time*/,
                           &sleep_time));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), sleep_time);

  // Even if file is new, a zero hold-off time means we choose a time between
  // 0 and max_spread_time.
  ASSERT_TRUE(TouchFileHelper(meta_file, base::Time::Now()));
  EXPECT_TRUE(GetSleepTime(
      meta_file, base::TimeDelta::FromSeconds(60) /*max_spread_time*/,
      base::TimeDelta::FromSeconds(0) /*hold_off_time*/, &sleep_time));
  EXPECT_LE(base::TimeDelta::FromSeconds(0), sleep_time);
  EXPECT_GE(base::TimeDelta::FromSeconds(60), sleep_time);

  // Make the meta file old enough so hold-off time is not necessary.
  const base::Time now = base::Time::Now();
  ASSERT_TRUE(TouchFileHelper(meta_file, now - kMaxHoldOffTime));

  // sleep_time should always be 0, since max_spread_time is set to 0.
  EXPECT_TRUE(
      GetSleepTime(meta_file, max_spread_time, kMaxHoldOffTime, &sleep_time));
  EXPECT_EQ(base::TimeDelta::FromSeconds(0), sleep_time);

  // sleep_time should be in range [0, 10].
  max_spread_time = base::TimeDelta::FromSeconds(10);
  EXPECT_TRUE(
      GetSleepTime(meta_file, max_spread_time, kMaxHoldOffTime, &sleep_time));
  EXPECT_LE(base::TimeDelta::FromSeconds(0), sleep_time);
  EXPECT_GE(base::TimeDelta::FromSeconds(10), sleep_time);

  // If the meta file is current, the minimum sleep time should be
  // kMaxHoldOffTime but the maximum is still max_spread_time.
  max_spread_time = base::TimeDelta::FromSeconds(60);
  ASSERT_TRUE(TouchFileHelper(meta_file, base::Time::Now()));
  EXPECT_TRUE(
      GetSleepTime(meta_file, max_spread_time, kMaxHoldOffTime, &sleep_time));
  // 0.9 in case we got preempted for 3 seconds between the file touch and the
  // GetSleepTime().
  EXPECT_LE(kMaxHoldOffTime * 0.9, sleep_time);
  EXPECT_GE(base::TimeDelta::FromSeconds(60), sleep_time);
}

TEST_F(CrashSenderUtilTest, SortReports) {
  // Crashes from oldest to youngest will be a, b, c.
  CrashInfo crash_info_a;
  EXPECT_TRUE(base::Time::FromString("15 Nov 2018 12:45:26 GMT",
                                     &crash_info_a.last_modified));
  crash_info_a.metadata.SetString("order", "1");
  MetaFile file_a(base::FilePath("a"), std::move(crash_info_a));

  CrashInfo crash_info_b;
  EXPECT_TRUE(base::Time::FromString("7 Feb 2019 12:45:26 GMT",
                                     &crash_info_b.last_modified));
  crash_info_b.metadata.SetString("order", "2");
  MetaFile file_b(base::FilePath("b"), std::move(crash_info_b));

  CrashInfo crash_info_c;
  EXPECT_TRUE(base::Time::FromString("7 Feb 2019 12:48:26 GMT",
                                     &crash_info_c.last_modified));
  crash_info_c.metadata.SetString("order", "3");
  MetaFile file_c(base::FilePath("c"), std::move(crash_info_c));

  // Add out of order
  std::vector<MetaFile> crashes;
  crashes.emplace_back(std::move(file_c));
  crashes.emplace_back(std::move(file_b));
  crashes.emplace_back(std::move(file_a));
  SortReports(&crashes);

  ASSERT_EQ(crashes.size(), 3);

  EXPECT_EQ(crashes[0].first, base::FilePath("a"));
  std::string order_string;
  EXPECT_TRUE(crashes[0].second.metadata.GetString("order", &order_string));
  EXPECT_EQ(order_string, "1");

  EXPECT_EQ(crashes[1].first, base::FilePath("b"));
  EXPECT_TRUE(crashes[1].second.metadata.GetString("order", &order_string));
  EXPECT_EQ(order_string, "2");

  EXPECT_EQ(crashes[2].first, base::FilePath("c"));
  EXPECT_TRUE(crashes[2].second.metadata.GetString("order", &order_string));
  EXPECT_EQ(order_string, "3");
}

TEST_F(CrashSenderUtilTest, GetUserCrashDirectories) {
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();
  test_util::SetActiveSessions(mock.get(),
                               {{"user1", "hash1"}, {"user2", "hash2"}});
  Sender::Options options;
  options.proxy = mock.release();
  Sender sender(std::move(metrics_lib_), options);
  ASSERT_TRUE(sender.Init());

  EXPECT_THAT(sender.GetUserCrashDirectories(),
              UnorderedElementsAre(paths::Get("/home/user/hash1/crash"),
                                   paths::Get("/home/user/hash2/crash")));
}

TEST_F(CrashSenderUtilTest, CreateCrashFormData) {
  const base::FilePath system_dir = paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_dir));

  const base::FilePath payload_file = system_dir.Append("0.0.0.0.payload");
  const std::string payload_contents = "foobar_payload";
  ASSERT_TRUE(test_util::CreateFile(payload_file, payload_contents));

  const base::FilePath log_file = system_dir.Append("0.0.0.0.log");
  const std::string log_contents = "foobar_log";
  ASSERT_TRUE(test_util::CreateFile(log_file, log_contents));

  const base::FilePath text_var_file = system_dir.Append("data.txt");
  const std::string text_var_contents = "upload_text_contents";
  ASSERT_TRUE(test_util::CreateFile(text_var_file, text_var_contents));

  const base::FilePath file_var_file = system_dir.Append("data.bin");
  const std::string file_var_contents = "upload_file_contents";
  ASSERT_TRUE(test_util::CreateFile(file_var_file, file_var_contents));

  brillo::KeyValueStore metadata;
  metadata.SetString("exec_name", "fake_exec_name");
  metadata.SetString("ver", "fake_chromeos_ver");
  metadata.SetString("upload_var_prod", "fake_product");
  metadata.SetString("upload_var_ver", "fake_version");
  metadata.SetString("sig", "fake_sig");
  metadata.SetString("upload_var_guid", "SHOULD_NOT_BE_USED");
  metadata.SetString("upload_var_foovar", "bar");
  metadata.SetString("upload_text_footext", text_var_file.value());
  metadata.SetString("upload_file_log", log_file.value());
  metadata.SetString("upload_file_foofile", file_var_file.value());
  metadata.SetString("error_type", "fake_error");

  CrashDetails details = {
      .meta_file = base::FilePath(system_dir).Append("0.0.0.0.meta"),
      .payload_file = payload_file,
      .payload_kind = "fake_payload",
      .client_id = kFakeClientId,
      .metadata = metadata,
  };

  Sender::Options options;
  options.form_data_boundary = "boundary";

  Sender sender(nullptr, options);

  std::unique_ptr<brillo::http::FormData> form_data =
      sender.CreateCrashFormData(details, nullptr);

  brillo::StreamPtr stream = form_data->ExtractDataStream();
  std::vector<uint8_t> data(stream->GetSize());
  ASSERT_TRUE(stream->ReadAllBlocking(data.data(), data.size(), nullptr));

  const char expected_data[] =
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"exec_name\"\r\n"
      "\r\n"
      "fake_exec_name\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"board\"\r\n"
      "\r\n"
      "undefined\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"hwclass\"\r\n"
      "\r\n"
      "undefined\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"prod\"\r\n"
      "\r\n"
      "fake_product\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"ver\"\r\n"
      "\r\n"
      "fake_version\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"sig\"\r\n"
      "\r\n"
      "fake_sig\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"sig2\"\r\n"
      "\r\n"
      "fake_sig\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"upload_file_fake_payload\"; "
      "filename=\"0.0.0.0.payload\"\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "foobar_payload\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"foofile\"; "
      "filename=\"data.bin\"\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "upload_file_contents\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"log\"; "
      "filename=\"0.0.0.0.log\"\r\n"
      "Content-Transfer-Encoding: binary\r\n"
      "\r\n"
      "foobar_log\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"footext\"\r\n"
      "\r\n"
      "upload_text_contents\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"foovar\"\r\n"
      "\r\n"
      "bar\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"boot_mode\"\r\n"
      "\r\n"
      "missing-crossystem\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"error_type\"\r\n"
      "\r\n"
      "fake_error\r\n"
      "--boundary\r\n"
      "Content-Disposition: form-data; name=\"guid\"\r\n"
      "\r\n"
      "00112233445566778899aabbccddeeff\r\n"
      "--boundary--";

  EXPECT_EQ(expected_data, std::string(data.begin(), data.end()));
}

TEST_F(CrashSenderUtilTest, SendCrashes) {
  // Set up the mock session manager client.
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();
  test_util::SetActiveSessions(mock.get(), {{"user", "hash"}});
  std::vector<MetaFile> crashes_to_send;

  // Establish the client ID.
  ASSERT_TRUE(CreateClientIdFile());

  // Create the system crash directory, and crash files in it.
  const base::FilePath system_dir = paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_dir));
  const base::FilePath system_meta_file = system_dir.Append("0.0.0.0.meta");
  const base::FilePath system_log = system_dir.Append("0.0.0.0.log");
  const char system_meta[] =
      "payload=0.0.0.0.log\n"
      "exec_name=exec_foo\n"
      "fake_report_id=123\n"
      "upload_var_prod=foo\n"
      "done=1\n";
  ASSERT_TRUE(test_util::CreateFile(system_meta_file, system_meta));
  ASSERT_TRUE(test_util::CreateFile(system_log, ""));
  CrashInfo system_info;
  EXPECT_TRUE(system_info.metadata.LoadFromString(system_meta));
  system_info.payload_file = system_log;
  system_info.payload_kind = "log";
  EXPECT_TRUE(base::Time::FromString("25 Apr 2018 1:23:44 GMT",
                                     &system_info.last_modified));
  crashes_to_send.emplace_back(system_meta_file, std::move(system_info));

  // Create a user crash directory, and crash files in it.
  const base::FilePath user_dir = paths::Get("/home/user/hash/crash");
  ASSERT_TRUE(base::CreateDirectory(user_dir));
  const base::FilePath user_meta_file = user_dir.Append("0.0.0.0.meta");
  const base::FilePath user_log = user_dir.Append("0.0.0.0.log");
  const char user_meta[] =
      "payload=0.0.0.0.log\n"
      "exec_name=exec_bar\n"
      "fake_report_id=456\n"
      "upload_var_prod=bar\n"
      "done=1\n";
  ASSERT_TRUE(test_util::CreateFile(user_meta_file, user_meta));
  ASSERT_TRUE(test_util::CreateFile(user_log, ""));
  CrashInfo user_info;
  EXPECT_TRUE(user_info.metadata.LoadFromString(user_meta));
  user_info.payload_file = user_log;
  user_info.payload_kind = "log";
  EXPECT_TRUE(base::Time::FromString("25 Apr 2018 1:24:01 GMT",
                                     &user_info.last_modified));
  crashes_to_send.emplace_back(user_meta_file, std::move(user_info));

  // Create another user crash in "user". This will be skipped since the max
  // crash rate will be set to 2.
  const base::FilePath user_meta_file2 = user_dir.Append("1.1.1.1.meta");
  const base::FilePath user_log2 = user_dir.Append("1.1.1.1.log");
  const char user_meta2[] =
      "payload=1.1.1.1.log\n"
      "exec_name=exec_baz\n"
      "fake_report_id=789\n"
      "upload_var_prod=baz\n"
      "done=1\n";
  ASSERT_TRUE(test_util::CreateFile(user_meta_file2, user_meta2));
  ASSERT_TRUE(test_util::CreateFile(user_log2, ""));
  CrashInfo user_info2;
  EXPECT_TRUE(user_info2.metadata.LoadFromString(user_meta2));
  user_info2.payload_file = user_log2;
  user_info2.payload_kind = "log";
  EXPECT_TRUE(base::Time::FromString("25 Apr 2018 1:24:05 GMT",
                                     &user_info2.last_modified));
  crashes_to_send.emplace_back(user_meta_file2, std::move(user_info2));

  // Set up the conditions to emulate a device in guest mode.
  ASSERT_TRUE(SetConditions(kOfficialBuild, kGuestMode, kMetricsEnabled));
  // Keep the raw pointer, that's needed to exit from guest mode later.
  MetricsLibraryMock* raw_metrics_lib = metrics_lib_.get();

  // Set up the crash sender so that it succeeds.
  ASSERT_TRUE(SetMockCrashSending(true));

  // Set up the sender.
  std::vector<base::TimeDelta> sleep_times;
  Sender::Options options;
  options.proxy = mock.release();
  options.max_crash_rate = 2;
  options.sleep_function = base::Bind(&FakeSleep, &sleep_times);
  options.always_write_uploads_log = true;
  Sender sender(std::move(metrics_lib_), options);
  ASSERT_TRUE(sender.Init());

  // Send crashes.
  sender.SendCrashes(crashes_to_send);

  // The Chrome uploads.log file shouldn't exist because we had nothing to
  // upload.
  EXPECT_FALSE(base::PathExists(paths::Get(paths::kChromeCrashLog)));
  EXPECT_TRUE(sleep_times.empty());

  // Exit from guest mode, and send crashes again.
  raw_metrics_lib->set_guest_mode(false);
  sender.SendCrashes(crashes_to_send);

  // Check the upload log from crash_sender.
  std::string contents;
  ASSERT_TRUE(
      base::ReadFileToString(paths::Get(paths::kChromeCrashLog), &contents));
  std::vector<std::vector<std::string>> rows = ParseChromeUploadsLog(contents);
  // Should only contain two results, since max_crash_rate is set to 2.
  // FakeSleep should be called twice for the two crash reports.
  ASSERT_EQ(2, rows.size());
  EXPECT_EQ(2, sleep_times.size());

  // Each line of the uploads.log file is "timestamp,report_id,product".
  // The first run should be for the meta file in the system directory.
  std::vector<std::string> row = rows[0];
  ASSERT_EQ(3, row.size());
  EXPECT_EQ("123", row[1]);
  EXPECT_EQ("foo", row[2]);

  // The second run should be for the meta file in the "user" directory.
  row = rows[1];
  ASSERT_EQ(3, row.size());
  EXPECT_EQ("456", row[1]);
  EXPECT_EQ("bar", row[2]);

  // The uploaded crash files should be removed now.
  EXPECT_FALSE(base::PathExists(system_meta_file));
  EXPECT_FALSE(base::PathExists(system_log));
  EXPECT_FALSE(base::PathExists(user_meta_file));
  EXPECT_FALSE(base::PathExists(user_log));

  // The following should be kept since the crash report was not uploaded.
  EXPECT_TRUE(base::PathExists(user_meta_file2));
  EXPECT_TRUE(base::PathExists(user_log2));
}

TEST_F(CrashSenderUtilTest, SendCrashes_Fail) {
  // Set up the mock sesssion manager client.
  auto mock =
      std::make_unique<org::chromium::SessionManagerInterfaceProxyMock>();
  test_util::SetActiveSessions(mock.get(), {{"user", "hash"}});
  std::vector<MetaFile> crashes_to_send;

  // Create the system crash directory, and crash files in it.
  const base::FilePath system_dir = paths::Get(paths::kSystemCrashDirectory);
  ASSERT_TRUE(base::CreateDirectory(system_dir));
  const base::FilePath system_meta_file = system_dir.Append("0.0.0.0.meta");
  const base::FilePath system_log = system_dir.Append("0.0.0.0.log");
  const char system_meta[] =
      "payload=0.0.0.0.log\n"
      "done=1\n";
  ASSERT_TRUE(test_util::CreateFile(system_meta_file, system_meta));
  ASSERT_TRUE(test_util::CreateFile(system_log, ""));
  CrashInfo system_info;
  EXPECT_TRUE(system_info.metadata.LoadFromString(system_meta));
  system_info.payload_file = system_log;
  system_info.payload_kind = "log";
  EXPECT_TRUE(base::Time::FromString("25 Apr 2018 1:23:44 GMT",
                                     &system_info.last_modified));
  crashes_to_send.emplace_back(system_meta_file, std::move(system_info));

  ASSERT_TRUE(SetConditions(kOfficialBuild, kSignInMode, kMetricsEnabled));

  // Set up the crash sender so that it fails.
  ASSERT_TRUE(SetMockCrashSending(false));

  // Set up the sender.
  std::vector<base::TimeDelta> sleep_times;
  Sender::Options options;
  options.proxy = mock.release();
  options.max_crash_rate = 2;
  options.sleep_function = base::Bind(&FakeSleep, &sleep_times);
  options.always_write_uploads_log = true;
  Sender sender(std::move(metrics_lib_), options);
  ASSERT_TRUE(sender.Init());

  sender.SendCrashes(crashes_to_send);

  // The followings should be kept since the crash report was not uploaded.
  EXPECT_TRUE(base::PathExists(system_meta_file));
  EXPECT_TRUE(base::PathExists(system_log));

  // The Chrome uploads.log file shouldn't exist because we had nothing to
  // report.
  EXPECT_FALSE(base::PathExists(paths::Get(paths::kChromeCrashLog)));
}

TEST_F(CrashSenderUtilTest, CreateClientId) {
  std::string client_id = GetClientId();
  EXPECT_EQ(client_id.length(), 32);
  // Make sure it returns the same one multiple times.
  EXPECT_EQ(client_id, GetClientId());
}

TEST_F(CrashSenderUtilTest, RetrieveClientId) {
  CreateClientIdFile();
  EXPECT_EQ(kFakeClientId, GetClientId());
}

}  // namespace util
