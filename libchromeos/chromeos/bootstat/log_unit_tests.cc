// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bootstat.h"
#include "bootstat_test.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using std::string;

static void RemoveFile(const string& file_path) {
  EXPECT_EQ(0, unlink(file_path.c_str()))
      << "can't unlink file " << file_path << ": "
      << strerror(errno);
}


// Class to track and test the data associated with a single event.
// The primary function is TestLogEvent():  This method wraps calls
// to bootstat_log() with code to track the expected contents of the
// event files.  After logging, the expected content is tested
// against the actual content.
class EventTracker {
  public:
    EventTracker(const string& name, const string& uptime_prefix,
               const string& disk_prefix);
    void TestLogEvent(const string& uptime, const string& diskstats);
    void Reset();

  private:
    string event_name_;
    string uptime_file_name_;
    string uptime_content_;
    string diskstats_file_name_;
    string diskstats_content_;
};


EventTracker::EventTracker(const string& name,
                           const string& uptime_prefix,
                           const string& diskstats_prefix)
    : event_name_(name),
      uptime_content_(""),
      diskstats_content_("") {
  string truncated_name =
      event_name_.substr(0, BOOTSTAT_MAX_EVENT_LEN - 1);
  uptime_file_name_ = uptime_prefix + truncated_name;
  diskstats_file_name_ = diskstats_prefix + truncated_name;
}


// Basic helper function to test whether the contents of the
// specified file exactly match the given contents string.
static void ValidateEventFileContents(const string& file_name,
                                      const string& file_contents) {
  int rv = access(file_name.c_str(), W_OK);
  EXPECT_EQ(0, rv) << file_name << " is not writable: "
                   << strerror(errno);
  rv = access(file_name.c_str(), R_OK);
  ASSERT_EQ(0, rv) << file_name << " is not readable: "
                   << strerror(errno);
  char *buffer = new char[file_contents.length() + 1];
  int fd = open(file_name.c_str(), O_RDONLY);
  rv = read(fd, buffer, file_contents.length());
  EXPECT_EQ(file_contents.length(), rv);
  buffer[file_contents.length()] = '\0';
  string actual_contents(buffer);
  EXPECT_EQ(file_contents, actual_contents);
  rv = read(fd, buffer, 1);
  EXPECT_EQ(0, rv) << "found data in event file past expected EOF";
  (void)close(fd);
  delete buffer;
}


// Call bootstat_log() once, and update the expected content for
// this event.  Test that the new content of the event's files
// matches the updated expected content.
void EventTracker::TestLogEvent(const string& uptime,
                                const string& diskstats) {
  bootstat_log(event_name_.c_str());
  uptime_content_ += uptime;
  diskstats_content_ += diskstats;
  ValidateEventFileContents(uptime_file_name_, uptime_content_);
  ValidateEventFileContents(diskstats_file_name_, diskstats_content_);
}


// Reset event state back to initial conditions, by deleting the
// associated event files, and clearing the expected contents.
void EventTracker::Reset() {
  uptime_content_.clear();
  diskstats_content_.clear();
  RemoveFile(diskstats_file_name_);
  RemoveFile(uptime_file_name_);
}


// Bootstat test class.  We use this class to override the
// dependencies in bootstat_log() on the file paths for /proc/uptime
// and /sys/block/<device>/stat.
//
// The class uses test-specific interfaces that change the default
// paths from the kernel statistics psuedo-files to temporary paths
// selected by this test.  This class also redirects the location for
// the event files created by bootstat_log() to a temporary directory.
class BootstatTest : public ::testing::Test {
  protected:
    virtual void SetUp();
    virtual void TearDown();

    EventTracker MakeEvent(const string& event_name) {
      return EventTracker(event_name, uptime_event_prefix_,
                          disk_event_prefix_);
    }

    void SetStatsContent(const char* uptime_content,
                         const char* disk_content);
    void TestLogEvent(EventTracker& event);

  private:
    string stats_output_dir_;
    string uptime_event_prefix_;
    string disk_event_prefix_;

    string uptime_stats_file_name_;
    string uptime_stats_content_;
    string disk_stats_file_name_;
    string disk_stats_content_;
};


void BootstatTest::SetUp() {
  char dir_template[] = "bootstat_test_XXXXXX";
  stats_output_dir_ = string(mkdtemp(dir_template));
  uptime_event_prefix_ = stats_output_dir_ + "/uptime-";
  disk_event_prefix_ = stats_output_dir_ + "/disk-";
  uptime_stats_file_name_ = stats_output_dir_ + "/proc_uptime";
  disk_stats_file_name_ = stats_output_dir_ + "/block_stats";
  bootstat_set_output_directory(stats_output_dir_.c_str());
  bootstat_set_uptime_file_name(uptime_stats_file_name_.c_str());
  bootstat_set_disk_file_name(disk_stats_file_name_.c_str());
}


void BootstatTest::TearDown() {
  bootstat_set_uptime_file_name(NULL);
  bootstat_set_disk_file_name(NULL);
  bootstat_set_output_directory(NULL);
  RemoveFile(uptime_stats_file_name_);
  RemoveFile(disk_stats_file_name_);
  EXPECT_EQ(0, rmdir(stats_output_dir_.c_str()))
      << "Can't remove directory " << stats_output_dir_
      << ": " << strerror(errno) << ".";
}


static void WriteStatsContent(const string& content, const string& file_path) {
  const int kFileOpenFlags = O_WRONLY | O_TRUNC | O_CREAT;
  const mode_t kFileCreationMode =
      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH;
  int fd = open(file_path.c_str(), kFileOpenFlags, kFileCreationMode);
  int nwrite = write(fd, content.c_str(), content.length());
  EXPECT_EQ(content.length(), nwrite)
      << "write to stats file " << file_path
      << " failed: " << strerror(errno);
  (void)close(fd);
}


// Set the content of the files mocking the contents of the kernel's
// statistics pseudo-files.  The strings provided here will be the
// ones recorded for subsequent calls to bootstat_log() for all
// events.
void BootstatTest::SetStatsContent(const char* uptime_stats,
                                   const char* disk_stats) {
  uptime_stats_content_ = string(uptime_stats);
  WriteStatsContent(uptime_stats_content_, uptime_stats_file_name_);
  disk_stats_content_ = string(disk_stats);
  WriteStatsContent(disk_stats_content_, disk_stats_file_name_);
}


void BootstatTest::TestLogEvent(EventTracker& event) {
  event.TestLogEvent(uptime_stats_content_, disk_stats_content_);
}


// Test data to be used as input to SetStatsContent().
//
// The structure of this array is pairs of strings, terminated by a
// single NULL.  The first string in the pair is content for
// /proc/uptime, the second for /sys/block/<device>/stat.
//
// This data is taken directly from a development system, and is
// representative of valid stats content, though not typical of what
// would be seen immediately after boot.
static const char* bootstat_data[] = {
/*  0  */
/* uptime */  "691448.42 11020440.26\n",
/*  disk  */  " 1417116    14896 55561564 10935990  4267850 78379879"
                  " 661568738 1635920520      158 17856450 1649520570\n",
/*  1  */
/* uptime */  "691623.71 11021372.99\n",
/*  disk  */  " 1420714    14918 55689988 11006390  4287385 78594261"
                  " 663441564 1651579200      152 17974280 1665255160\n",
/* EOT */     NULL
};


// Tests that event file content matches expectations when an
// event is logged multiple times.
TEST_F(BootstatTest, ContentGeneration) {
  EventTracker ev = MakeEvent(string("test_event"));
  int i = 0;
  while (bootstat_data[i] != NULL) {
    SetStatsContent(bootstat_data[i], bootstat_data[i+1]);
    TestLogEvent(ev);
    i += 2;
  }
  ev.Reset();
}


// Tests that name truncation of logged events works as advertised.
TEST_F(BootstatTest, EventNameTruncation) {
  static const char kMostVoluminousEventName[] =
    //             16              32              48              64
    "event-6789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" //  64
    "=064+56789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 128
    "=128+56789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 191
    "=191+56789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 256
    ;

  string very_long(kMostVoluminousEventName);
  SetStatsContent(bootstat_data[0], bootstat_data[1]);

  EventTracker ev = MakeEvent(very_long);
  TestLogEvent(ev);
  ev.Reset();
  ev = MakeEvent(very_long.substr(0, 1));
  TestLogEvent(ev);
  ev.Reset();
  ev = MakeEvent(very_long.substr(0, BOOTSTAT_MAX_EVENT_LEN - 1));
  TestLogEvent(ev);
  ev.Reset();
  ev = MakeEvent(very_long.substr(0, BOOTSTAT_MAX_EVENT_LEN));
  TestLogEvent(ev);
  ev.Reset();
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
