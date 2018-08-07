// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_SENDER_UTIL_H_
#define CRASH_REPORTER_CRASH_SENDER_UTIL_H_

namespace util {

// Represents a name-value pair for an environment variable.
struct EnvPair {
  const char* name;
  const char* value;
};

// Predefined environment variables for controlling the behaviors of
// crash_sender.
//
// TODO(satorux): Remove the environment variables once the shell script is
// gone. The environment variables are handy in the shell script, but should not
// be needed in the C++ version.
constexpr EnvPair kEnvironmentVariables[] = {
    // Set this to 1 in the environment to allow uploading crash reports
    // for unofficial versions.
    {"FORCE_OFFICIAL", "0"},
    // Maximum crashes to send per day.
    {"MAX_CRASH_RATE", "32"},
    // Set this to 1 in the environment to pretend to have booted in developer
    // mode.  This is used by autotests.
    {"MOCK_DEVELOPER_MODE", "0"},
    // Ignore PAUSE_CRASH_SENDING file if set.
    {"OVERRIDE_PAUSE_SENDING", "0"},
    // Maximum time to sleep between sends.
    {"SECONDS_SEND_SPREAD", "600"},
};

// Parses the command line, and handles the command line flags.
//
// This function also sets the predefined environment valuables to the default
// values, or the values specified by -e options.
//
// On error, the process exits as a failure with an error message for the
// first-encountered error.
void ParseCommandLine(int argc, const char* const* argv);

// Returns true if mock is enabled.
bool IsMock();

// Returns true if the sending should be paused.
bool ShouldPauseSending();

}  // namespace util

#endif  // CRASH_REPORTER_CRASH_SENDER_UTIL_H_
