// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <base/command_line.h>
#include <base/files/file_util.h>
#include <brillo/flag_helper.h>

#include "biod/fake_biometric_common.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

int main(int argc, char* argv[]) {
  DEFINE_string(fake_input,
                "/tmp/fake_biometric",
                "FIFO special file used to poke the fake biometric device");
  DEFINE_bool(
      failure, false, "signal a general failure of the biometric device");
  DEFINE_int32(scan, -1, "signal a scan with the given scan result code");
  DEFINE_bool(scan_done,
              false,
              "when used with --scan, also causes the device to indicate "
              "scanning is done");
  DEFINE_int32(attempt,
               -1,
               "signal an authentication attempt with the given scan result "
               "code; recognized user IDs are the set of non-flag arguments");

  brillo::FlagHelper::Init(argc,
                           argv,
                           "fake_biometric_tool, used to poke the fake "
                           "biometric device embedded in biod.");

  LOG(INFO) << "vcsid " << VCSID;

  int cmd_count = (FLAGS_failure ? 1 : 0) + (FLAGS_scan != -1 ? 1 : 0) +
                  (FLAGS_attempt != -1 ? 1 : 0);
  if (cmd_count != 1) {
    LOG(ERROR) << "Expected exactly one command to be given";
    return 1;
  }

  base::ScopedFD fake_input =
      base::ScopedFD(open(FLAGS_fake_input.c_str(), O_WRONLY | O_NONBLOCK));
  CHECK(fake_input.get() >= 0) << "Failed to open fake biometric input";

  if (FLAGS_failure) {
    uint8_t cmd[] = {FAKE_BIOMETRIC_MAGIC_BYTES, 'F'};
    CHECK(write(fake_input.get(), &cmd, sizeof(cmd)) == sizeof(cmd));
  }

  if (FLAGS_scan >= 0) {
    uint8_t cmd[] = {FAKE_BIOMETRIC_MAGIC_BYTES,
                     'S',
                     (uint8_t)FLAGS_scan,
                     (uint8_t)FLAGS_scan_done};
    CHECK(write(fake_input.get(), &cmd, sizeof(cmd)) == sizeof(cmd));
  }

  if (FLAGS_attempt >= 0) {
    base::CommandLine::StringVector recognized_user_ids =
        base::CommandLine::ForCurrentProcess()->GetArgs();
    if (recognized_user_ids.size() > UINT8_MAX)
      recognized_user_ids.resize(UINT8_MAX);
    std::vector<uint8_t> cmd = {FAKE_BIOMETRIC_MAGIC_BYTES,
                                'A',
                                (uint8_t)FLAGS_attempt,
                                (uint8_t)recognized_user_ids.size()};
    for (auto& user_id : recognized_user_ids) {
      if (user_id.size() > UINT8_MAX)
        user_id.resize(UINT8_MAX);
      cmd.push_back((uint8_t)user_id.size());
      cmd.insert(cmd.end(), user_id.begin(), user_id.end());
    }
    CHECK(write(fake_input.get(), cmd.data(), cmd.size()) ==
          static_cast<int>(cmd.size()));
  }

  return 0;
}
