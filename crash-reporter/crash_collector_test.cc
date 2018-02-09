// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_collector_test.h"

#include <unistd.h>
#include <utility>

#include <base/files/file_util.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <brillo/syslog_logging.h>
#include <gtest/gtest.h>

#include "crash-reporter/crash_collector.h"

using base::FilePath;
using base::StringPrintf;
using brillo::FindLog;
using ::testing::Invoke;
using ::testing::Return;

namespace {

void CountCrash() {
  ADD_FAILURE();
}

bool IsMetrics() {
  ADD_FAILURE();
  return false;
}

bool GetActiveUserSessionsImpl(std::map<std::string, std::string>* sessions) {
  char kUser[] = "chicken@butt.com";
  char kHash[] = "hashcakes";
  sessions->insert(std::pair<std::string, std::string>(kUser, kHash));
  return true;
}

}  // namespace

class CrashCollectorTest : public ::testing::Test {
 public:
  void SetUp() {
    EXPECT_CALL(collector_, SetUpDBus()).WillRepeatedly(Return());

    collector_.Initialize(CountCrash, IsMetrics);
    test_dir_ = FilePath("test");
    base::CreateDirectory(test_dir_);
    brillo::ClearLog();
  }

  void TearDown() { base::DeleteFile(test_dir_, true); }

  bool CheckHasCapacity();

 protected:
  CrashCollectorMock collector_;
  FilePath test_dir_;
};

TEST_F(CrashCollectorTest, Initialize) {
  ASSERT_TRUE(CountCrash == collector_.count_crash_function_);
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

TEST_F(CrashCollectorTest, StripSensitiveDataBasic) {
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

TEST_F(CrashCollectorTest, StripSensitiveDataBulk) {
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
  collector_.StripSensitiveData(&lotsa_macs);
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
  collector_.StripSensitiveData(&crash_with_macs);
  EXPECT_EQ(kCrashWithMacsStripped, crash_with_macs);
}

TEST_F(CrashCollectorTest, GetCrashDirectoryInfo) {
  FilePath path;
  const int kRootUid = 0;
  const int kRootGid = 0;
  const int kNtpUid = 5;
  const int kChronosUid = 1000;
  const int kChronosGid = 1001;
  const mode_t kExpectedSystemMode = 01755;
  const mode_t kExpectedUserMode = 0755;

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

  EXPECT_CALL(collector_, GetActiveUserSessions(testing::_))
      .WillOnce(Invoke(&GetActiveUserSessionsImpl));

  EXPECT_EQ(collector_.IsUserSpecificDirectoryEnabled(), true);

  path = collector_.GetCrashDirectoryInfo(kChronosUid, kChronosUid, kChronosGid,
                                          &directory_mode, &directory_owner,
                                          &directory_group);
  EXPECT_EQ("/home/user/hashcakes/crash", path.value());
  EXPECT_EQ(kExpectedUserMode, directory_mode);
  EXPECT_EQ(kChronosUid, directory_owner);
  EXPECT_EQ(kChronosGid, directory_group);
}

TEST_F(CrashCollectorTest, FormatDumpBasename) {
  struct tm tm = {0};
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

bool CrashCollectorTest::CheckHasCapacity() {
  static const char kFullMessage[] = "Crash directory test already full";
  bool has_capacity = collector_.CheckHasCapacity(test_dir_);
  bool has_message = FindLog(kFullMessage);
  EXPECT_EQ(has_message, !has_capacity);
  return has_capacity;
}

TEST_F(CrashCollectorTest, CheckHasCapacityUsual) {
  // Test kMaxCrashDirectorySize - 1 non-meta files can be added.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf("file%d.core", i)), "", 0);
    EXPECT_TRUE(CheckHasCapacity());
  }

  // Test an additional kMaxCrashDirectorySize - 1 meta files fit.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf("file%d.meta", i)), "", 0);
    EXPECT_TRUE(CheckHasCapacity());
  }

  // Test an additional kMaxCrashDirectorySize meta files don't fit.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf("overage%d.meta", i)), "", 0);
    EXPECT_FALSE(CheckHasCapacity());
  }
}

TEST_F(CrashCollectorTest, CheckHasCapacityCorrectBasename) {
  // Test kMaxCrashDirectorySize - 1 files can be added.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 1; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf("file.%d.core", i)), "", 0);
    EXPECT_TRUE(CheckHasCapacity());
  }
  base::WriteFile(test_dir_.Append("file.last.core"), "", 0);
  EXPECT_FALSE(CheckHasCapacity());
}

TEST_F(CrashCollectorTest, CheckHasCapacityStrangeNames) {
  // Test many files with different extensions and same base fit.
  for (int i = 0; i < 5 * CrashCollector::kMaxCrashDirectorySize; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf("a.%d", i)), "", 0);
    EXPECT_TRUE(CheckHasCapacity());
  }
  // Test dot files are treated as individual files.
  for (int i = 0; i < CrashCollector::kMaxCrashDirectorySize - 2; ++i) {
    base::WriteFile(test_dir_.Append(StringPrintf(".file%d", i)), "", 0);
    EXPECT_TRUE(CheckHasCapacity());
  }
  base::WriteFile(test_dir_.Append("normal.meta"), "", 0);
  EXPECT_FALSE(CheckHasCapacity());
}

TEST_F(CrashCollectorTest, MetaData) {
  const char kMetaFileBasename[] = "generated.meta";
  FilePath meta_file = test_dir_.Append(kMetaFileBasename);
  FilePath lsb_release = test_dir_.Append("lsb-release");
  FilePath payload_file = test_dir_.Append("payload-file");
  std::string contents;
  collector_.set_lsb_release_for_test(lsb_release);
  const char kLsbContents[] =
      "CHROMEOS_RELEASE_BOARD=lumpy\n"
      "CHROMEOS_RELEASE_VERSION=6727.0.2015_01_26_0853\n"
      "CHROMEOS_RELEASE_NAME=Chromium OS\n";
  ASSERT_TRUE(base::WriteFile(lsb_release, kLsbContents, strlen(kLsbContents)));
  const char kPayload[] = "foo";
  ASSERT_TRUE(base::WriteFile(payload_file, kPayload, strlen(kPayload)));
  collector_.AddCrashMetaData("foo", "bar");
  collector_.WriteCrashMetaData(meta_file, "kernel", payload_file.value());
  EXPECT_TRUE(base::ReadFileToString(meta_file, &contents));
  const char kExpectedMeta[] =
      "foo=bar\n"
      "exec_name=kernel\n"
      "ver=6727.0.2015_01_26_0853\n"
      "payload=test/payload-file\n"
      "payload_size=3\n"
      "done=1\n";
  EXPECT_EQ(kExpectedMeta, contents);

  // Test target of symlink is not overwritten.
  payload_file = test_dir_.Append("payload2-file");
  ASSERT_TRUE(base::WriteFile(payload_file, kPayload, strlen(kPayload)));
  FilePath meta_symlink_path = test_dir_.Append("symlink.meta");
  ASSERT_EQ(0, symlink(kMetaFileBasename, meta_symlink_path.value().c_str()));
  ASSERT_TRUE(base::PathExists(meta_symlink_path));
  brillo::ClearLog();
  collector_.WriteCrashMetaData(meta_symlink_path, "kernel",
                                payload_file.value());
  // Target metadata contents should have stayed the same.
  contents.clear();
  EXPECT_TRUE(base::ReadFileToString(meta_file, &contents));
  EXPECT_EQ(kExpectedMeta, contents);
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
  ASSERT_TRUE(
      base::WriteFile(config_file, kConfigContents, strlen(kConfigContents)));
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

TEST_F(CrashCollectorTest, TruncatedLog) {
  FilePath config_file = test_dir_.Append("crash_config");
  FilePath output_file = test_dir_.Append("crash_log");
  const char kConfigContents[] = "foobar=echo These are log contents.";
  ASSERT_TRUE(
      base::WriteFile(config_file, kConfigContents, strlen(kConfigContents)));
  base::DeleteFile(FilePath(output_file), false);
  collector_.max_log_size_ = 10;
  EXPECT_TRUE(collector_.GetLogContents(config_file, "foobar", output_file));
  ASSERT_TRUE(base::PathExists(output_file));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(output_file, &contents));
  EXPECT_EQ("These are \n<TRUNCATED>\n", contents);
}
