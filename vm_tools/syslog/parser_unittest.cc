// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <time.h>

#include <memory>
#include <string>

#include <base/macros.h>
#include <google/protobuf/util/message_differencer.h>
#include <gtest/gtest.h>

#include "vm_tools/syslog/parser.h"

using std::string;

namespace pb = google::protobuf;

namespace vm_tools {
namespace syslog {
namespace {

struct PriorityTestCase {
  // Buffer that holds the string to be tested.
  const char* buf;

  // Number of characters expected to be consumed from the buffer.
  size_t count;

  // Expected severity value after parsing.
  vm_tools::LogSeverity severity;
};

// Tests that the parser can handle properly and improperly formatted
// priority values.
const PriorityTestCase priority_tests[] = {
    {
        .buf = "<117>", .count = 5, .severity = vm_tools::NOTICE,
    },
    {
        .buf = "<24975>", .count = 0, .severity = vm_tools::MISSING,
    },
    {
        .buf = "<>", .count = 0, .severity = vm_tools::MISSING,
    },
    {
        .buf = "<0 hi there", .count = 0, .severity = vm_tools::MISSING,
    },
    {
        .buf = "5> kthxbye", .count = 0, .severity = vm_tools::MISSING,
    },
    {
        .buf = "\0\0\0\0\0\0\0", .count = 0, .severity = vm_tools::MISSING,
    },
    {
        .buf = "<0> this should work",
        .count = 3,
        .severity = vm_tools::EMERGENCY,
    },
};

class PriorityTest : public ::testing::TestWithParam<PriorityTestCase> {
 public:
  PriorityTest() = default;
  ~PriorityTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PriorityTest);
};

struct TimestampTestCase {
  const char* buf;
  struct tm tm;
  size_t count;
};

const TimestampTestCase timestamp_tests[] = {
    {
        .buf = "Jan 17 23:54:11",
        // clang-format off
        .tm = {
            .tm_sec = 11,
            .tm_min = 54,
            .tm_hour = 23,
            .tm_mday = 17,
            .tm_mon = 0,
        },
        // clang-format on
        .count = 15,
    },
    {
        .buf = "Oct 52 05:37:23",
        .tm = {},
        .count = 0,
    },
    {
        .buf = "Jun 2 17:15:47",
        // clang-format off
        .tm = {
            .tm_sec = 47,
            .tm_min = 15,
            .tm_hour = 17,
            .tm_mday = 2,
            .tm_mon = 5,
        },
        // clang-format on
        .count = 14,
    },
    {
        .buf = "Mar 24 kernel: [17.5694]",
        .tm = {},
        .count = 0,
    },
    {
        .buf = "Apr 12 35:18:52",
        .tm = {},
        .count = 0,
    },
    {
        .buf = "22 Feb 07:03:11",
        .tm = {},
        .count = 0,
    },
    {
        .buf = "Dec 24 18:33:58 Let the countdown begin",
        // clang-format off
        .tm = {
            .tm_sec = 58,
            .tm_min = 33,
            .tm_hour = 18,
            .tm_mday = 24,
            .tm_mon = 11,
        },
        // clang-format on
        .count = 15,
    },
};

class TimestampTest : public ::testing::TestWithParam<TimestampTestCase> {
 public:
  TimestampTest() = default;
  ~TimestampTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(TimestampTest);
};

struct EndToEndTestCase {
  const char* buf;
  struct tm tm;
  vm_tools::LogSeverity severity;
  size_t content_offset;
  size_t content_length;
};

// Tests that the end-to-end parser can properly handle all valid records.  A
// valid record is defined as any string that contains only valid UTF8 code
// points.  Taken from the RFC3164 examples section.
const EndToEndTestCase end_to_end_tests[] = {
    {
        .buf = "<34>Oct 11 22:14:15 mymachine su: 'su root' failed for lonvick "
               "on /dev/pts/8",
        // clang-format off
        .tm = {
            .tm_sec = 15,
            .tm_min = 14,
            .tm_hour = 22,
            .tm_mday = 11,
            .tm_mon = 9,
        },
        // clang-format on
        .severity = vm_tools::CRITICAL,
        .content_offset = 19,
    },
    {
        .buf = "Use the BFG!",
        .tm = {},
        .severity = vm_tools::NOTICE,
        .content_offset = 0,
    },
    {
        .buf = "<165>Aug 24 05:34:00 CST 1987 mymachine myproc[10]: %% It's "
               "time to make the do-nuts.  %%  Ingredients: Mix=OK, Jelly=OK # "
               "Devices: Mixer=OK, Jelly_Injector=OK, Frier=OK # Transport: "
               "Conveyer1=OK, Conveyer2=OK # %%",
        // clang-format off
        .tm = {
            .tm_sec = 00,
            .tm_min = 34,
            .tm_hour = 5,
            .tm_mday = 24,
            .tm_mon = 7,
        },
        // clang-format on
        .severity = vm_tools::NOTICE,
        .content_offset = 20,
    },
    {
        .buf = "<0>1990 Oct 22 10:52:01 TZ-6 scapegoat.dmz.example.org "
               "10.1.2.3 sched[0]: That's All Folks!",
        .tm = {},
        .severity = vm_tools::EMERGENCY,
        .content_offset = 3,
    },
    {
        .buf = "<34>Oct\u0021 11 22:14:15 mymachine su: 'su root' failed for "
               "lonvick on /dev\u001Cb\x0f\x7f\xf0\xff!/pts/8",
        .tm = {},
        .severity = vm_tools::CRITICAL,
        .content_offset = 4,
    },
    {
        .buf = "U\u007Cse\u008A the\xe5\xc4\x4f\x05\xb6\xfd BFG!",
        .tm = {},
        .severity = vm_tools::NOTICE,
        .content_offset = 0,
    },
    {
        .buf = "<33>Embedded \u0000 NUL\0 characters",
        .tm = {},
        .severity = vm_tools::ALERT,
        .content_offset = 4,
        .content_length = 26,
    },
};

class EndToEndSyslogTest : public ::testing::TestWithParam<EndToEndTestCase> {
 public:
  EndToEndSyslogTest() = default;
  ~EndToEndSyslogTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(EndToEndSyslogTest);
};

struct KernelTestCase {
  const char* buf;
  vm_tools::LogSeverity severity;
  int64_t micros;
  uint64_t sequence;
  size_t content_offset;
  bool success;
};

const KernelTestCase kernel_record_tests[] = {
    {
        .buf = "7,160,424069,-;pci_root PNP0A03:00: host bridge window [io  "
               "0x0000-0x0cf7] (ignored)",
        .severity = vm_tools::DEBUG,
        .micros = 424069,
        .sequence = 160,
        .content_offset = 15,
        .success = true,
    },
    {
        .buf = " SUBSYSTEM=acpi",
        .severity = vm_tools::MISSING,
        .micros = 0,
        .sequence = 0,
        .content_offset = 0,
        .success = false,
    },
    {
        .buf = "",
        .severity = vm_tools::MISSING,
        .micros = 0,
        .sequence = 0,
        .content_offset = 0,
        .success = false,
    },
    {
        .buf = "6,339,5140900,-;NET: Registered protocol family 10",
        .severity = vm_tools::INFO,
        .micros = 5140900,
        .sequence = 339,
        .content_offset = 16,
        .success = true,
    },
    {
        .buf = "30,340,5690716,-;udevd[80]: starting version 181",
        .severity = vm_tools::INFO,
        .micros = 5690716,
        .sequence = 340,
        .content_offset = 17,
        .success = true,
    },
    {
        .buf =
            "3,4,0,-;Command line: cros_secure console= loglevel=7 "
            "init=/sbin/init cros_secure oops=panic panic=-1 "
            "root=PARTUUID=ff361f8e-4de7-5b4f-9d60-8c6083102115/PARTNROFF=1 "
            "rootwait ro dm_verity.error_behavior=3 dm_verity.max_bios=-1 "
            "dm_verity.dev_wait=0 dm=\"1 vroot none ro 1,0 4096000 verity "
            "payload=ROOT_DEV hashtree=HASH_DEV hashstart=4096000 alg=sha1 "
            "root_hexdigest=09f37a0e6a875563b1ec929a1809802017e94aeb "
            "salt="
            "d75b9b5693e08552d87a10e7b531c6e2e5194665eb7864f366175d5bfe0ec907\""
            " noinitrd disablevmx=off cros_debug vt.global_cursor_default=0 "
            "kern_guid=ff361f8e-4de7-5b4f-9d60-8c6083102115 add_efi_memmap "
            "boot=local noresume noswap i915.modeset=1 tpm_tis.force=1 "
            "tpm_tis.interrupts=0 nmi_watchdog=panic,lapic i915.enable_psr=1",
        .severity = vm_tools::ERROR,
        .micros = 0,
        .sequence = 4,
        .content_offset = 8,
        .success = true,
    },
    {
        .buf = "37,5,3,cThere is no semi-colon in this line",
        .severity = vm_tools::MISSING,
        .micros = 0,
        .sequence = 5,
        .content_offset = 0,
        .success = false,
    },
    {
        .buf = ";Missing metadata",
        .severity = vm_tools::MISSING,
        .micros = 0,
        .sequence = 0,
        .content_offset = 1,
        .success = false,
    },
    {
        .buf = "4,210,504028,-;rtc_cmos rtc_cmos: only 24-hr supported\n "
               "SUBSYSTEM=platform\n DEVICE=+platform:rtc_cmos",
        .severity = vm_tools::WARNING,
        .micros = 504028,
        .sequence = 210,
        .content_offset = 15,
        .success = true,
    },
};

class KernelRecordTest : public ::testing::TestWithParam<KernelTestCase> {
 public:
  KernelRecordTest() = default;
  ~KernelRecordTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(KernelRecordTest);
};

}  // namespace

TEST_P(PriorityTest, ParsesCorrectly) {
  struct PriorityTestCase param = GetParam();

  vm_tools::LogSeverity severity = vm_tools::MISSING;
  EXPECT_EQ(param.count, ParseSyslogPriority(param.buf, &severity));
  EXPECT_EQ(param.severity, severity);
}

INSTANTIATE_TEST_CASE_P(SyslogParser,
                        PriorityTest,
                        ::testing::ValuesIn(priority_tests));

TEST_P(TimestampTest, ParsesCorrectly) {
  struct TimestampTestCase param = GetParam();

  // Get the current time to set the current year.
  struct timespec ts;
  ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts), 0);
  struct tm current_tm;
  ASSERT_TRUE(localtime_r(&ts.tv_sec, &current_tm));
  param.tm.tm_year = current_tm.tm_year;

  vm_tools::Timestamp timestamp;
  EXPECT_EQ(ParseSyslogTimestamp(param.buf, &timestamp), param.count);

  if (param.count > 0) {
    // The test case had a valid timestamp.  Make sure it was parsed correctly.
    EXPECT_EQ(timestamp.seconds(), timelocal(&param.tm));
    EXPECT_EQ(timestamp.nanos(), 0);
  }
}

INSTANTIATE_TEST_CASE_P(SyslogParser,
                        TimestampTest,
                        ::testing::ValuesIn(timestamp_tests));

TEST_P(EndToEndSyslogTest, ParsesCorrectly) {
  struct EndToEndTestCase param = GetParam();

  // Set up the MessageDifferencer.
  auto differencer = std::make_unique<pb::util::MessageDifferencer>();

  // Set up the expected protobuf.
  auto expected = std::make_unique<vm_tools::LogRecord>();
  expected->set_severity(param.severity);

  // A non-zero content_length indicates that the buffer has embedded \0
  // characters and cannot be treated as a c-string.
  if (param.content_length == 0) {
    expected->set_content(string(&param.buf[param.content_offset]));
  } else {
    expected->set_content(&param.buf[param.content_offset],
                          param.content_length);
  }

  // Get the current time to set the current year.
  struct timespec ts;
  ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts), 0);
  struct tm current_tm;
  ASSERT_TRUE(localtime_r(&ts.tv_sec, &current_tm));
  param.tm.tm_year = current_tm.tm_year;

  // We use a tm_mday value of 0 to indicate that the log record doesn't contain
  // a valid time because it's not valid for tm_mday to be 0.
  if (param.tm.tm_mday == 0) {
    expected->mutable_timestamp()->set_seconds(ts.tv_sec);
    expected->mutable_timestamp()->set_nanos(ts.tv_nsec);
    // The log record doesn't have a valid time.  Tell the differencer to ignore
    // the timestamp field.  We will manually check it later.
    differencer->IgnoreField(
        vm_tools::LogRecord::descriptor()->FindFieldByName("timestamp"));
  } else {
    // The log record does have a valid time.  Make sure it is parsed correctly.
    expected->mutable_timestamp()->set_seconds(timelocal(&param.tm));
    expected->mutable_timestamp()->set_nanos(0);
  }

  auto actual = std::make_unique<vm_tools::LogRecord>();
  size_t length = param.content_offset + param.content_length;
  if (length == param.content_offset) {
    length = strlen(param.buf);
  }
  EXPECT_TRUE(ParseSyslogRecord(param.buf, length, actual.get()));

  // Record the difference, if any, so we can print it out with the test
  // failure.
  string difference;
  differencer->ReportDifferencesToString(&difference);

  EXPECT_TRUE(differencer->Compare(*expected, *actual)) << difference;

  if (param.tm.tm_mday == 0) {
    // The timestamp nanos will definitely not match so instead we check to make
    // sure that the timestamp is approximately the same as now.
    constexpr int64_t kNanosecondsPerSecond = 1000 * 1000 * 1000;
    int64_t diff =
        (actual->timestamp().seconds() - expected->timestamp().seconds()) *
            kNanosecondsPerSecond +
        (actual->timestamp().nanos() - expected->timestamp().nanos());
    EXPECT_LT(diff, kNanosecondsPerSecond);
  }
}

INSTANTIATE_TEST_CASE_P(SyslogParser,
                        EndToEndSyslogTest,
                        ::testing::ValuesIn(end_to_end_tests));

TEST_P(KernelRecordTest, ParsesCorrectly) {
  struct KernelTestCase param = GetParam();

  // Use current time as the boot time.
  struct timespec ts;
  ASSERT_EQ(clock_gettime(CLOCK_REALTIME, &ts), 0);

  base::Time boot_time = base::Time::FromTimeSpec(ts);

  // Set up the expected proto.
  vm_tools::LogRecord expected;
  expected.set_severity(param.severity);

  struct timeval tv =
      (boot_time + base::TimeDelta::FromMicroseconds(param.micros)).ToTimeVal();
  expected.mutable_timestamp()->set_seconds(tv.tv_sec);
  expected.mutable_timestamp()->set_nanos(
      tv.tv_usec * base::Time::kNanosecondsPerMicrosecond);

  const char* end = strchr(&param.buf[param.content_offset], '\n');
  if (end == nullptr) {
    // Guaranteed to succeed because all these strings are statically allocated
    // and the compiler ensures they are properly terminated.
    end = strchr(&param.buf[param.content_offset], '\0');
  }
  expected.set_content(&param.buf[param.content_offset],
                       end - &param.buf[param.content_offset]);

  // Now parse the record and check the result.
  vm_tools::LogRecord actual;
  uint64_t sequence = 0;
  EXPECT_EQ(param.success, ParseKernelRecord(param.buf, strlen(param.buf),
                                             boot_time, &actual, &sequence));

  if (param.success) {
    pb::util::MessageDifferencer differencer;

    string difference;
    differencer.ReportDifferencesToString(&difference);

    EXPECT_TRUE(differencer.Compare(expected, actual)) << difference;
    EXPECT_EQ(sequence, param.sequence);
  }
}

INSTANTIATE_TEST_CASE_P(KernelRecordParser,
                        KernelRecordTest,
                        ::testing::ValuesIn(kernel_record_tests));

}  // namespace syslog
}  // namespace vm_tools
