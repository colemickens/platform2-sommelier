// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_collector_test.h"

#include <inttypes.h>
#include <unistd.h>
#include <utility>

#include <base/files/file_util.h>
#include <base/files/scoped_temp_dir.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/test/simple_test_clock.h>
#include <brillo/syslog_logging.h>
#include <gtest/gtest.h>

#include "crash-reporter/crash_collector.h"
#include "crash-reporter/test_util.h"

using base::FilePath;
using base::StringPrintf;
using brillo::FindLog;
using ::testing::Return;

namespace {

constexpr int64_t kFakeNow = 123456789LL;

bool IsMetrics() {
  ADD_FAILURE();
  return false;
}

}  // namespace

class CrashCollectorTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_CALL(collector_, SetUpDBus()).WillRepeatedly(Return());

    collector_.Initialize(IsMetrics);

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    test_dir_ = scoped_temp_dir_.GetPath();

    brillo::ClearLog();
  }

  bool CheckHasCapacity();

 protected:
  CrashCollectorMock collector_;
  FilePath test_dir_;
  base::ScopedTempDir scoped_temp_dir_;
};

TEST_F(CrashCollectorTest, Initialize) {
  ASSERT_TRUE(IsMetrics == collector_.is_feedback_allowed_function_);
}

TEST_F(CrashCollectorTest, WriteNewFile) {
  FilePath test_file = test_dir_.Append("test_new");
  const char kBuffer[] = "buffer";
  EXPECT_EQ(strlen(kBuffer),
            collector_.WriteNewFile(test_file, kBuffer, strlen(kBuffer)));
  EXPECT_LT(collector_.WriteNewFile(test_file, kBuffer, strlen(kBuffer)), 0);
}

TEST_F(CrashCollectorTest, Sanitize) {
  EXPECT_EQ("chrome", collector_.Sanitize("chrome"));
  EXPECT_EQ("CHROME", collector_.Sanitize("CHROME"));
  EXPECT_EQ("1chrome2", collector_.Sanitize("1chrome2"));
  EXPECT_EQ("chrome__deleted_", collector_.Sanitize("chrome (deleted)"));
  EXPECT_EQ("foo_bar", collector_.Sanitize("foo.bar"));
  EXPECT_EQ("", collector_.Sanitize(""));
  EXPECT_EQ("_", collector_.Sanitize(" "));
}

TEST_F(CrashCollectorTest, StripMacAddressesBasic) {
  // Basic tests of StripSensitiveData...

  // Make sure we work OK with a string w/ no MAC addresses.
  const std::string kCrashWithNoMacsOrig =
      "<7>[111566.131728] PM: Entering mem sleep\n";
  std::string crash_with_no_macs(kCrashWithNoMacsOrig);
  collector_.StripSensitiveData(&crash_with_no_macs);
  EXPECT_EQ(kCrashWithNoMacsOrig, crash_with_no_macs);

  // Make sure that we handle the case where there's nothing before/after the
  // MAC address.
  const std::string kJustAMacOrig = "11:22:33:44:55:66";
  const std::string kJustAMacStripped = "00:00:00:00:00:01";
  std::string just_a_mac(kJustAMacOrig);
  collector_.StripSensitiveData(&just_a_mac);
  EXPECT_EQ(kJustAMacStripped, just_a_mac);

  // Test MAC addresses crammed together to make sure it gets both of them.
  //
  // I'm not sure that the code does ideal on these two test cases (they don't
  // look like two MAC addresses to me), but since we don't see them I think
  // it's OK to behave as shown here.
  const std::string kCrammedMacs1Orig = "11:22:33:44:55:66:11:22:33:44:55:66";
  const std::string kCrammedMacs1Stripped =
      "00:00:00:00:00:01:00:00:00:00:00:01";
  std::string crammed_macs_1(kCrammedMacs1Orig);
  collector_.StripSensitiveData(&crammed_macs_1);
  EXPECT_EQ(kCrammedMacs1Stripped, crammed_macs_1);

  const std::string kCrammedMacs2Orig = "11:22:33:44:55:6611:22:33:44:55:66";
  const std::string kCrammedMacs2Stripped =
      "00:00:00:00:00:0100:00:00:00:00:01";
  std::string crammed_macs_2(kCrammedMacs2Orig);
  collector_.StripSensitiveData(&crammed_macs_2);
  EXPECT_EQ(kCrammedMacs2Stripped, crammed_macs_2);

  // Test case-sensitiveness (we shouldn't be case-senstive).
  const std::string kCapsMacOrig = "AA:BB:CC:DD:EE:FF";
  const std::string kCapsMacStripped = "00:00:00:00:00:01";
  std::string caps_mac(kCapsMacOrig);
  collector_.StripSensitiveData(&caps_mac);
  EXPECT_EQ(kCapsMacStripped, caps_mac);

  const std::string kLowerMacOrig = "aa:bb:cc:dd:ee:ff";
  const std::string kLowerMacStripped = "00:00:00:00:00:01";
  std::string lower_mac(kLowerMacOrig);
  collector_.StripSensitiveData(&lower_mac);
  EXPECT_EQ(kLowerMacStripped, lower_mac);
}

TEST_F(CrashCollectorTest, StripMacAddressesBulk) {
  // Test calling StripSensitiveData w/ lots of MAC addresses in the "log".

  // Test that stripping code handles more than 256 unique MAC addresses, since
  // that overflows past the last byte...
  // We'll write up some code that generates 258 unique MAC addresses.  Sorta
  // cheating since the code is very similar to the current code in
  // StripSensitiveData(), but would catch if someone changed that later.
  std::string lotsa_macs_orig;
  std::string lotsa_macs_stripped;
  int i;
  for (i = 0; i < 258; i++) {
    lotsa_macs_orig +=
        StringPrintf(" 11:11:11:11:%02X:%02x", (i & 0xff00) >> 8, i & 0x00ff);
    lotsa_macs_stripped += StringPrintf(
        " 00:00:00:00:%02X:%02x", ((i + 1) & 0xff00) >> 8, (i + 1) & 0x00ff);
  }
  std::string lotsa_macs(lotsa_macs_orig);
  collector_.StripMacAddresses(&lotsa_macs);
  EXPECT_EQ(lotsa_macs_stripped, lotsa_macs);
}

TEST_F(CrashCollectorTest, StripSensitiveDataSample) {
  // Test calling StripSensitiveData w/ some actual lines from a real crash;
  // included two MAC addresses (though replaced them with some bogusness).
  const std::string kCrashWithMacsOrig =
      "<6>[111567.195339] ata1.00: ACPI cmd ef/10:03:00:00:00:a0 (SET FEATURES)"
      " filtered out\n"
      "<7>[108539.540144] wlan0: authenticate with 11:22:33:44:55:66 (try 1)\n"
      "<7>[108539.554973] wlan0: associate with 11:22:33:44:55:66 (try 1)\n"
      "<6>[110136.587583] usb0: register 'QCUSBNet2k' at usb-0000:00:1d.7-2,"
      " QCUSBNet Ethernet Device, 99:88:77:66:55:44\n"
      "<7>[110964.314648] wlan0: deauthenticated from 11:22:33:44:55:66"
      " (Reason: 6)\n"
      "<7>[110964.325057] phy0: Removed STA 11:22:33:44:55:66\n"
      "<7>[110964.325115] phy0: Destroyed STA 11:22:33:44:55:66\n"
      "<6>[110969.219172] usb0: register 'QCUSBNet2k' at usb-0000:00:1d.7-2,"
      " QCUSBNet Ethernet Device, 99:88:77:66:55:44\n"
      "<7>[111566.131728] PM: Entering mem sleep\n";
  const std::string kCrashWithMacsStripped =
      "<6>[111567.195339] ata1.00: ACPI cmd ef/10:03:00:00:00:a0 (SET FEATURES)"
      " filtered out\n"
      "<7>[108539.540144] wlan0: authenticate with 00:00:00:00:00:01 (try 1)\n"
      "<7>[108539.554973] wlan0: associate with 00:00:00:00:00:01 (try 1)\n"
      "<6>[110136.587583] usb0: register 'QCUSBNet2k' at usb-0000:00:1d.7-2,"
      " QCUSBNet Ethernet Device, 00:00:00:00:00:02\n"
      "<7>[110964.314648] wlan0: deauthenticated from 00:00:00:00:00:01"
      " (Reason: 6)\n"
      "<7>[110964.325057] phy0: Removed STA 00:00:00:00:00:01\n"
      "<7>[110964.325115] phy0: Destroyed STA 00:00:00:00:00:01\n"
      "<6>[110969.219172] usb0: register 'QCUSBNet2k' at usb-0000:00:1d.7-2,"
      " QCUSBNet Ethernet Device, 00:00:00:00:00:02\n"
      "<7>[111566.131728] PM: Entering mem sleep\n";
  std::string crash_with_macs(kCrashWithMacsOrig);
  collector_.StripMacAddresses(&crash_with_macs);
  EXPECT_EQ(kCrashWithMacsStripped, crash_with_macs);
}

TEST_F(CrashCollectorTest, StripEmailAddresses) {
  std::string logs =
      "Lorem ipsum dolor sit amet, consectetur adipiscing elit,"
      " sed do eiusmod tempor incididunt ut labore et dolore \n"
      "magna aliqua. Ut enim ad minim veniam, quis nostrud "
      "exercitation ullamco foo.bar+baz@secret.com laboris \n"
      "nisi ut aliquip ex ea commodo consequat. Duis aute "
      "irure dolor in reprehenderit (support@example.com) in \n"
      "voluptate velit esse cillum dolore eu fugiat nulla "
      "pariatur. Excepteur sint occaecat:abuse@dev.reallylong,\n"
      "cupidatat non proident, sunt in culpa qui officia "
      "deserunt mollit anim id est laborum.";
  collector_.StripEmailAddresses(&logs);
  EXPECT_EQ(0, logs.find("Lorem ipsum"));
  EXPECT_EQ(std::string::npos, logs.find("foo.bar"));
  EXPECT_EQ(std::string::npos, logs.find("secret"));
  EXPECT_EQ(std::string::npos, logs.find("support"));
  EXPECT_EQ(std::string::npos, logs.find("example.com"));
  EXPECT_EQ(std::string::npos, logs.find("abuse"));
  EXPECT_EQ(std::string::npos, logs.find("dev.reallylong"));
}

TEST_F(CrashCollectorTest, GetCrashDirectoryInfo) {
  FilePath path;
  const int kRootUid = 0;
  const int kRootGid = 0;
  const int kNtpUid = 5;
  const int kChronosUid = 1000;
  const int kChronosGid = 1001;
  const mode_t kExpectedSystemMode = 0700;
  const mode_t kExpectedUserMode = 0700;

  mode_t directory_mode;
  uid_t directory_owner;
  gid_t directory_group;

  path = collector_.GetCrashDirectoryInfo(kRootUid, kChronosUid, kChronosGid,
                                          &directory_mode, &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/var/spool/crash", path.value());
  EXPECT_EQ(kExpectedSystemMode, directory_mode);
  EXPECT_EQ(kRootUid, directory_owner);
  EXPECT_EQ(kRootGid, directory_group);

  path = collector_.GetCrashDirectoryInfo(kNtpUid, kChronosUid, kChronosGid,
                                          &directory_mode, &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/var/spool/crash", path.value());
  EXPECT_EQ(kExpectedSystemMode, directory_mode);
  EXPECT_EQ(kRootUid, directory_owner);
  EXPECT_EQ(kRootGid, directory_group);

  auto* mock = new org::chromium::SessionManagerInterfaceProxyMock;
  test_util::SetActiveSessions(mock, {{"user", "hashcakes"}});
  collector_.session_manager_proxy_.reset(mock);

  path = collector_.GetCrashDirectoryInfo(kChronosUid, kChronosUid, kChronosGid,
                                          &directory_mode, &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/home/user/hashcakes/crash", path.value());
  EXPECT_EQ(kExpectedUserMode, directory_mode);
  EXPECT_EQ(kChronosUid, directory_owner);
  EXPECT_EQ(kChronosGid, directory_group);
}

TEST_F(CrashCollectorTest, FormatDumpBasename) {
  struct tm tm = {};
  tm.tm_sec = 15;
  tm.tm_min = 50;
  tm.tm_hour = 13;
  tm.tm_mday = 23;
  tm.tm_mon = 4;
  tm.tm_year = 110;
  tm.tm_isdst = -1;
  std::string basename = collector_.FormatDumpBasename("foo", mktime(&tm), 100);
  ASSERT_EQ("foo.20100523.135015.100", basename);
}

TEST_F(CrashCollectorTest, GetCrashPath) {
  EXPECT_EQ("/var/spool/crash/myprog.20100101.1200.1234.core",
            collector_
                .GetCrashPath(FilePath("/var/spool/crash"),
                              "myprog.20100101.1200.1234", "core")
                .value());
  EXPECT_EQ("/home/chronos/user/crash/chrome.20100101.1200.1234.dmp",
            collector_
                .GetCrashPath(FilePath("/home/chronos/user/crash"),
                              "chrome.20100101.1200.1234", "dmp")
                .value());
}

TEST_F(CrashCollectorTest, ParseProcessTicksFromStat) {
  uint64_t ticks;
  EXPECT_FALSE(CrashCollector::ParseProcessTicksFromStat("", &ticks));
  EXPECT_FALSE(CrashCollector::ParseProcessTicksFromStat("123 (foo)", &ticks));

  constexpr char kTruncatedStat[] =
      "234641 (cat) R 234581 234641 234581 34821 234641 4194304 117 0 0 0 0 0 "
      "0 0 20 0 1 0";

  EXPECT_FALSE(
      CrashCollector::ParseProcessTicksFromStat(kTruncatedStat, &ticks));

  constexpr char kInvalidStat[] =
      "234641 (cat) R 234581 234641 234581 34821 234641 4194304 117 0 0 0 0 0 "
      "0 0 20 0 1 0 foo";

  EXPECT_FALSE(CrashCollector::ParseProcessTicksFromStat(kInvalidStat, &ticks));

  // Executable name is ") (".
  constexpr char kStat[] =
      "234641 () () R 234581 234641 234581 34821 234641 4194304 117 0 0 0 0 0 "
      "0 0 20 0 1 0 2092891 6090752 182 18446744073709551615 94720364494848 "
      "94720364525584 140735323062016 0 0 0 0 0 0 0 0 0 17 32 0 0 0 0 0 "
      "94720366623824 94720366625440 94720371765248 140735323070153 "
      "140735323070173 140735323070173 140735323074543 0";

  EXPECT_TRUE(CrashCollector::ParseProcessTicksFromStat(kStat, &ticks));
  EXPECT_EQ(2092891, ticks);
}

TEST_F(CrashCollectorTest, GetUptime) {
  base::TimeDelta uptime_at_process_start;
  EXPECT_TRUE(CrashCollector::GetUptimeAtProcessStart(
      getpid(), &uptime_at_process_start));

  base::TimeDelta uptime;
  EXPECT_TRUE(CrashCollector::GetUptime(&uptime));

  EXPECT_GT(uptime, uptime_at_process_start);
}

bool CrashCollectorTest::CheckHasCapacity() {
  std::string full_message = StringPrintf("Crash directory %s already full",
                                          test_dir_.value().c_str());
  bool has_capacity = collector_.CheckHasCapacity(test_dir_);
  bool has_message = FindLog(full_message.c_str());
  EXPECT_EQ(has_message, !has_capacity);
  return has_capacity;
}

TEST_F(CrashCollectorTest, CheckHasCapacityUsual) {
  // Test kMaxCrashDirectorySize - 1 non-meta files can be added.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf("file%d.core", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }

  // Test supplemental files fit with longer names.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf("file%d.log.gz", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }

  // Test an additional kMaxCrashDirectorySize - 1 meta files fit.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf("file%d.meta", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }

  // Test an additional kMaxCrashDirectorySize meta files don't fit.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf("overage%d.meta", i)), ""));
    EXPECT_FALSE(CheckHasCapacity());
  }
}

TEST_F(CrashCollectorTest, CheckHasCapacityCorrectBasename) {
  // Test kMaxCrashDirectorySize - 1 files can be added.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf("file.%d.core", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }
  ASSERT_TRUE(test_util::CreateFile(test_dir_.Append("file.last.core"), ""));
  EXPECT_FALSE(CheckHasCapacity());
}

TEST_F(CrashCollectorTest, CheckHasCapacityStrangeNames) {
  // Test many files with different extensions and same base fit.
  for (int i = 0; i < 5 * CrashCollector::kMaxCrashDirectorySize; ++i) {
    ASSERT_TRUE(
        test_util::CreateFile(test_dir_.Append(StringPrintf("a.%d", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }
  // Test dot files are treated as individual files.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 2; ++i) {
    ASSERT_TRUE(test_util::CreateFile(
        test_dir_.Append(StringPrintf(".file%d", i)), ""));
    EXPECT_TRUE(CheckHasCapacity());
  }
  ASSERT_TRUE(test_util::CreateFile(test_dir_.Append("normal.meta"), ""));
  EXPECT_TRUE(CheckHasCapacity());
}

TEST_F(CrashCollectorTest, MetaData) {
  const char kMetaFileBasename[] = "generated.meta";
  FilePath meta_file = test_dir_.Append(kMetaFileBasename);
  FilePath lsb_release = test_dir_.Append("lsb-release");
  FilePath payload_file = test_dir_.Append("payload-file");
  FilePath payload_full_path;
  std::string contents;
  collector_.set_lsb_release_for_test(lsb_release);
  const char kLsbContents[] =
      "CHROMEOS_RELEASE_BOARD=lumpy\n"
      "CHROMEOS_RELEASE_VERSION=6727.0.2015_01_26_0853\n"
      "CHROMEOS_RELEASE_NAME=Chromium OS\n"
      "CHROMEOS_RELEASE_DESCRIPTION=6727.0.2015_01_26_0853 (Test Build - foo)";
  ASSERT_TRUE(test_util::CreateFile(lsb_release, kLsbContents));
  const char kPayload[] = "foo";
  ASSERT_TRUE(test_util::CreateFile(payload_file, kPayload));
  collector_.AddCrashMetaData("foo", "bar");
  ASSERT_TRUE(NormalizeFilePath(payload_file, &payload_full_path));
  std::unique_ptr<base::SimpleTestClock> test_clock =
      std::make_unique<base::SimpleTestClock>();
  test_clock->SetNow(base::Time::UnixEpoch() +
                     base::TimeDelta::FromMilliseconds(kFakeNow));
  collector_.set_test_clock(std::move(test_clock));
  collector_.WriteCrashMetaData(meta_file, "kernel", payload_file.value());
  EXPECT_TRUE(base::ReadFileToString(meta_file, &contents));
  std::string expected_meta = StringPrintf(
      "foo=bar\n"
      "upload_var_reportTimeMillis=%" PRId64
      "\n"
      "upload_var_lsb-release=6727.0.2015_01_26_0853 (Test Build - foo)\n"
      "exec_name=kernel\n"
      "ver=6727.0.2015_01_26_0853\n"
      "payload=%s\n"
      "payload_size=3\n"
      "done=1\n",
      kFakeNow, payload_full_path.value().c_str());
  EXPECT_EQ(expected_meta, contents);

  // Test target of symlink is not overwritten.
  payload_file = test_dir_.Append("payload2-file");
  ASSERT_TRUE(test_util::CreateFile(payload_file, kPayload));
  FilePath meta_symlink_path = test_dir_.Append("symlink.meta");
  ASSERT_EQ(0, symlink(kMetaFileBasename, meta_symlink_path.value().c_str()));
  ASSERT_TRUE(base::PathExists(meta_symlink_path));
  brillo::ClearLog();
  collector_.WriteCrashMetaData(meta_symlink_path, "kernel",
                                payload_file.value());
  // Target metadata contents should have stayed the same.
  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(meta_file, &contents));
  EXPECT_EQ(expected_meta, contents);
  EXPECT_TRUE(FindLog("Unable to write"));

  // Test target of dangling symlink is not created.
  base::DeleteFile(meta_file, false);
  ASSERT_FALSE(base::PathExists(meta_file));
  brillo::ClearLog();
  collector_.WriteCrashMetaData(meta_symlink_path, "kernel",
                                payload_file.value());
  EXPECT_FALSE(base::PathExists(meta_file));
  EXPECT_TRUE(FindLog("Unable to write"));
}

TEST_F(CrashCollectorTest, GetLogContents) {
  FilePath config_file = test_dir_.Append("crash_config");
  FilePath output_file = test_dir_.Append("crash_log");
  const char kConfigContents[] =
      "foobar=echo hello there | \\\n  sed -e \"s/there/world/\"";
  ASSERT_TRUE(test_util::CreateFile(config_file, kConfigContents));
  base::DeleteFile(FilePath(output_file), false);
  EXPECT_FALSE(collector_.GetLogContents(config_file, "barfoo", output_file));
  EXPECT_FALSE(base::PathExists(output_file));
  base::DeleteFile(FilePath(output_file), false);
  EXPECT_TRUE(collector_.GetLogContents(config_file, "foobar", output_file));
  ASSERT_TRUE(base::PathExists(output_file));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(output_file, &contents));
  EXPECT_EQ("hello world\n", contents);
}

TEST_F(CrashCollectorTest, GetProcessTree) {
  const FilePath output_file = test_dir_.Append("log");
  std::string contents;

  ASSERT_TRUE(collector_.GetProcessTree(getpid(), output_file));
  ASSERT_TRUE(base::PathExists(output_file));
  EXPECT_TRUE(base::ReadFileToString(output_file, &contents));
  EXPECT_LT(300, contents.size());
  base::DeleteFile(FilePath(output_file), false);

  ASSERT_TRUE(collector_.GetProcessTree(0, output_file));
  ASSERT_TRUE(base::PathExists(output_file));
  EXPECT_TRUE(base::ReadFileToString(output_file, &contents));
  EXPECT_GT(100, contents.size());
}

TEST_F(CrashCollectorTest, TruncatedLog) {
  FilePath config_file = test_dir_.Append("crash_config");
  FilePath output_file = test_dir_.Append("crash_log");
  const char kConfigContents[] = "foobar=echo These are log contents.";
  ASSERT_TRUE(test_util::CreateFile(config_file, kConfigContents));
  base::DeleteFile(FilePath(output_file), false);
  collector_.max_log_size_ = 10;
  EXPECT_TRUE(collector_.GetLogContents(config_file, "foobar", output_file));
  ASSERT_TRUE(base::PathExists(output_file));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(output_file, &contents));
  EXPECT_EQ("These are \n<TRUNCATED>\n", contents);
}

// Check that the mode is reset properly.
TEST_F(CrashCollectorTest, CreateDirectoryWithSettingsMode) {
  int mode;
  EXPECT_TRUE(base::SetPosixFilePermissions(test_dir_, 0700));
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(
      test_dir_, 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::GetPosixFilePermissions(test_dir_, &mode));
  EXPECT_EQ(0755, mode);
}

// Check non-dir handling.
TEST_F(CrashCollectorTest, CreateDirectoryWithSettingsNonDir) {
  const base::FilePath file = test_dir_.Append("file");

  // Do not walk past a non-dir.
  ASSERT_TRUE(test_util::CreateFile(file, ""));
  EXPECT_FALSE(CrashCollector::CreateDirectoryWithSettings(
      file.Append("subdir"), 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::PathExists(file));
  EXPECT_FALSE(base::DirectoryExists(file));

  // Remove files and create dirs.
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(file, 0755, getuid(),
                                                          getgid(), nullptr));
  EXPECT_TRUE(base::DirectoryExists(file));
}

// Check we only create a single subdir.
TEST_F(CrashCollectorTest, CreateDirectoryWithSettingsSubdir) {
  const base::FilePath subdir = test_dir_.Append("sub");
  const base::FilePath subsubdir = subdir.Append("subsub");

  // Accessing sub/subsub/ should fail.
  EXPECT_FALSE(CrashCollector::CreateDirectoryWithSettings(
      subsubdir, 0755, getuid(), getgid(), nullptr));
  EXPECT_FALSE(base::PathExists(subdir));

  // Accessing sub/ should work.
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(
      subdir, 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::DirectoryExists(subdir));

  // Accessing sub/subsub/ should now work.
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(
      subsubdir, 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::DirectoryExists(subsubdir));
}

// Check symlink handling.
TEST_F(CrashCollectorTest, CreateDirectoryWithSettingsSymlinks) {
  base::FilePath td;

  // Do not walk an intermediate symlink (final target doesn't exist).
  // test/sub/
  // test/sym -> sub
  // Then access test/sym/subsub/.
  td = test_dir_.Append("1");
  EXPECT_TRUE(base::CreateDirectory(td.Append("sub")));
  EXPECT_TRUE(
      base::CreateSymbolicLink(base::FilePath("sub"), td.Append("sym")));
  EXPECT_FALSE(CrashCollector::CreateDirectoryWithSettings(
      td.Append("sym1/subsub"), 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::IsLink(td.Append("sym")));
  EXPECT_FALSE(base::PathExists(td.Append("sub/subsub")));

  // Do not walk an intermediate symlink (final target exists).
  // test/sub/subsub/
  // test/sym -> sub
  // Then access test/sym/subsub/.
  td = test_dir_.Append("2");
  EXPECT_TRUE(base::CreateDirectory(td.Append("sub/subsub")));
  EXPECT_TRUE(
      base::CreateSymbolicLink(base::FilePath("sub"), td.Append("sym")));
  EXPECT_FALSE(CrashCollector::CreateDirectoryWithSettings(
      td.Append("sym/subsub"), 0755, getuid(), getgid(), nullptr));
  EXPECT_TRUE(base::IsLink(td.Append("sym")));

  // If the final path is a symlink, we should remove it and make a dir.
  // test/sub/
  // test/sub/sym -> subsub
  td = test_dir_.Append("3");
  EXPECT_TRUE(base::CreateDirectory(td.Append("sub/subsub")));
  EXPECT_TRUE(
      base::CreateSymbolicLink(base::FilePath("subsub"), td.Append("sub/sym")));
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(
      td.Append("sub/sym"), 0755, getuid(), getgid(), nullptr));
  EXPECT_FALSE(base::IsLink(td.Append("sub/sym")));
  EXPECT_TRUE(base::DirectoryExists(td.Append("sub/sym")));

  // If the final path is a symlink, we should remove it and make a dir.
  // test/sub/subsub
  // test/sub/sym -> subsub
  td = test_dir_.Append("4");
  EXPECT_TRUE(base::CreateDirectory(td.Append("sub")));
  EXPECT_TRUE(
      base::CreateSymbolicLink(base::FilePath("subsub"), td.Append("sub/sym")));
  EXPECT_TRUE(CrashCollector::CreateDirectoryWithSettings(
      td.Append("sub/sym"), 0755, getuid(), getgid(), nullptr));
  EXPECT_FALSE(base::IsLink(td.Append("sub/sym")));
  EXPECT_TRUE(base::DirectoryExists(td.Append("sub/sym")));
  EXPECT_FALSE(base::PathExists(td.Append("sub/subsub")));
}
