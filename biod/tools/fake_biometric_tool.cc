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

#include "biod/fake_biometrics_manager_common.h"
#include "biod/proto_bindings/constants.pb.h"

#ifndef VCSID
#define VCSID "<not set>"
#endif

int main(int argc, char* argv[]) {
  DEFINE_string(fake_input, "/tmp/fake_biometric",
                "FIFO special file used to poke the fake biometric device");
  DEFINE_bool(failure, false,
              "signal a general failure of the biometric device");
  DEFINE_int32(scan, -1, "signal a scan with the given scan result code");
  DEFINE_bool(scan_done, false,
              "when used with --scan, also causes the device to indicate "
              "scanning is done");
  DEFINE_int32(attempt, -1,
               "signal an authentication attempt with the given scan result "
               "code; user IDs and associated record IDs are specified with "
               "the remaining arguments and each user ID/record ID set is "
               "delimited with '-', for example '0001 Record1 - 0002 Record2 "
               "Record3'.");

  brillo::FlagHelper::Init(argc, argv,
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
    // Biod EnrollScanDone signal cannot accept scan result outside the defined
    // range.
    CHECK_LT(FLAGS_scan,
             static_cast<int32_t>(biod::ScanResult::SCAN_RESULT_MAX));
    uint8_t cmd[] = {FAKE_BIOMETRIC_MAGIC_BYTES, 'S',
                     static_cast<uint8_t>(FLAGS_scan),
                     static_cast<uint8_t>(FLAGS_scan_done)};
    CHECK(write(fake_input.get(), &cmd, sizeof(cmd)) == sizeof(cmd));
  }

  if (FLAGS_attempt >= 0) {
    std::unordered_map<std::string, std::vector<std::string>> matches;
    bool new_match = true;
    std::vector<std::string>* record_ids = nullptr;
    for (const auto& arg : base::CommandLine::ForCurrentProcess()->GetArgs()) {
      if (arg == "-") {
        new_match = true;
        record_ids = nullptr;
        continue;
      }

      if (new_match) {
        if (matches.size() >= UINT8_MAX) {
          LOG(WARNING) << "Only " << UINT8_MAX << " matches can be sent at "
                       << " once. The remaining matches will be truncated.";
          break;
        }

        auto emplace_result = matches.emplace(arg, std::vector<std::string>());
        if (!emplace_result.second)
          LOG(WARNING) << "User ID " << arg << " was repeated.";
        record_ids = &emplace_result.first->second;
        new_match = false;
        continue;
      }

      if (!record_ids)
        continue;

      if (record_ids->size() >= UINT8_MAX) {
        LOG(WARNING) << "Only " << UINT8_MAX
                     << " record IDs per match can be sent. "
                     << "The remaining record IDs will be truncated.";
        continue;
      }

      record_ids->emplace_back(arg);
      if (record_ids->back().size() > UINT8_MAX) {
        LOG(WARNING) << "Record ID \"" << arg << "\" is longer than "
                     << UINT8_MAX << ". This Record ID will be truncated.";
        record_ids->back().resize(UINT8_MAX);
      }
    }
    std::vector<uint8_t> cmd = {FAKE_BIOMETRIC_MAGIC_BYTES, 'A',
                                static_cast<uint8_t>(FLAGS_attempt),
                                static_cast<uint8_t>(matches.size())};
    for (const auto& match : matches) {
      const std::string& user_id = match.first;
      const std::vector<std::string>& record_ids = match.second;
      cmd.push_back(static_cast<uint8_t>(user_id.size()));
      cmd.insert(cmd.end(), user_id.begin(), user_id.end());
      cmd.push_back(static_cast<uint8_t>(record_ids.size()));
      for (const auto& record_id : record_ids) {
        cmd.push_back(static_cast<uint8_t>(record_id.size()));
        cmd.insert(cmd.end(), record_id.begin(), record_id.end());
      }
    }
    CHECK(write(fake_input.get(), cmd.data(), cmd.size()) ==
          static_cast<int>(cmd.size()));
  }

  return 0;
}
