// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bootstat.h"

#include <errno.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include <gtest/gtest.h>

namespace {

using std::string;

static const char kMostVoluminousEventName[] =
  //             16              32              48              64
  "event-6789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" //  64
  "event-6789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 128
  "event-6789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 191
  "event-6789abcdef_123456789ABCDEF.123456789abcdef0123456789abcdef" // 256
  ;

static const string kUptimeFileNamePrefix("/tmp/uptime-");
static const string kDiskStatFileNamePrefix("/tmp/disk-");

static void TestEventFileAccess(const string& file_name) {
  int rv = access(file_name.c_str(), R_OK | W_OK);
  EXPECT_EQ(rv, 0) << "access to " << file_name
                   << " failed: " << strerror(errno);
  (void) unlink(file_name.c_str());
}

static void TestEventByName(const string& event_name) {
  bootstat_log(event_name.c_str());
  string truncated_event(event_name.substr(0, BOOTSTAT_MAX_EVENT_LEN - 1));
  TestEventFileAccess(kUptimeFileNamePrefix + truncated_event);
  TestEventFileAccess(kDiskStatFileNamePrefix + truncated_event);
}

// Tests that name truncation of logged events works as advertised
TEST(BoostatTest, EventNameTruncation) {
  string very_long(kMostVoluminousEventName);

  TestEventByName(very_long);
  TestEventByName(very_long.substr(0, 1));
  TestEventByName(very_long.substr(0, 16));
  TestEventByName(very_long.substr(0, BOOTSTAT_MAX_EVENT_LEN - 1));
  TestEventByName(very_long.substr(0, BOOTSTAT_MAX_EVENT_LEN));
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
