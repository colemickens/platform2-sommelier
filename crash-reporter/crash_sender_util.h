// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_SENDER_UTIL_H_
#define CRASH_REPORTER_CRASH_SENDER_UTIL_H_

#include <memory>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <brillo/key_value_store.h>
#include <metrics/metrics_library.h>
#include <session_manager/dbus-proxies.h>

namespace util {

// Maximum crashes to send per 24 hours.
constexpr int kMaxCrashRate = 32;

// Represents a name-value pair for an environment variable.
struct EnvPair {
  const char* name;
  const char* value;
};

// Actions returned by ChooseAction().
enum Action {
  kRemove,  // Should remove the crash report.
  kIgnore,  // Should ignore (keep) the crash report.
  kSend,    // Should send the crash report.
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

// Checks if the dependencies used in the shell script exist. On error, returns
// false, and saves the first path that was missing in |missing_path|.
// TODO(satorux): Remove this once rewriting to C++ is complete.
bool CheckDependencies(base::FilePath* missing_path);

// Gets the base part of a crash report file, such as name.01234.5678.9012 from
// name.01234.5678.9012.meta or name.01234.5678.9012.log.tar.xz.  We make sure
// "name" is sanitized in CrashCollector::Sanitize to not include any periods.
// The directory part will be preserved.
base::FilePath GetBasePartOfCrashFile(const base::FilePath& file_name);

// Removes orphaned files in |crash_dir|, that are files 24 hours old or older,
// without corresponding meta file.
void RemoveOrphanedCrashFiles(const base::FilePath& crash_dir);

// Chooses an action to take for the crash report associated with the given meta
// file, and reports the reason. |metrics_lib| is used to check if metrics are
// enabled, etc.
Action ChooseAction(const base::FilePath& meta_file,
                    MetricsLibraryInterface* metrics_lib,
                    std::string* reason);

// Removes invalid files in |crash_dir|, that are unknown, corrupted, or invalid
// in other ways, and picks crash reports that should be sent to the server. The
// meta files of the latter will be stored in |to_send|. See ChooseAction() for
// |metrics_lib|.
void RemoveAndPickCrashFiles(const base::FilePath& crash_dir,
                             MetricsLibraryInterface* metrics_lib,
                             std::vector<base::FilePath>* to_send);

// Removes report files associated with the given meta file.
// More specifically, if "foo.meta" is given, "foo.*" will be removed.
void RemoveReportFiles(const base::FilePath& meta_file);

// Returns the list of meta data files (files with ".meta" suffix), sorted by
// the timestamp in the old-to-new order.
std::vector<base::FilePath> GetMetaFiles(const base::FilePath& crash_dir);

// Gets the base name of the path pointed by |key| in the given metadata.
// Returns an empty path if the key is not found.
base::FilePath GetBaseNameFromMetadata(const brillo::KeyValueStore& metadata,
                                       const std::string& key);

// Returns which kind of report from the given payload path. Returns an empty
// string if the kind is unknown.
std::string GetKindFromPayloadPath(const base::FilePath& payload_path);

// Parses |raw_metadata| into |metadata|. Keys in metadata are validated (keys
// should consist of expected characters). Returns true on success.
bool ParseMetadata(const std::string& raw_metadata,
                   brillo::KeyValueStore* metadata);

// Returns true if the metadata is complete.
bool IsCompleteMetadata(const brillo::KeyValueStore& metadata);

// Returns true if the given timestamp file is new enough, indicating that there
// was a recent attempt to send a crash report.
bool IsTimestampNewEnough(const base::FilePath& timestamp_file);

// Returns true if sending a crash report now does not exceed |max_crash_rate|
// per 24 hours.. Reports the current rate (# of reports sent in the past 24
// hours) in |current_rate| regardless of the return value.
//
// This function checks/creates/removes timestamp files in |timestamps_dir| to
// track how many attempts were made to send crash reports in that past 24
// hours. Even if sending failed, it's counted as an attempt.
bool IsBelowRate(const base::FilePath& timestamps_dir,
                 int max_crash_rate,
                 int* current_rate);

// A helper class for sending crashes. The behaviors can be customized with
// Options class for unit testing.
class Sender {
 public:
  struct Options {
    // The shell script used for sending crashes.
    base::FilePath shell_script = base::FilePath("/sbin/crash_sender.sh");

    // Session manager client for locating the user-specific crash directories.
    org::chromium::SessionManagerInterfaceProxyInterface* proxy = nullptr;

    // Maximum crashes to send per 24 hours.
    int max_crash_rate = kMaxCrashRate;
  };

  Sender(std::unique_ptr<MetricsLibraryInterface> metrics_lib,
         const Options& options);

  // Initializes the sender object. Returns true on success.
  bool Init();

  // Sends crashes in |crash_dir|, in multiple steps:
  //
  // Before sending:
  // - Removes crash files that are orphaned or invalid.
  // - Removes all crash files if crash reporting is not enabled.
  //
  // While sending:
  // - Checks if the device enters guest mode, and stops if entered.
  // - TODO(satorux): The followings are done in the shell script.
  // - Enforces the rate limit per 24 hours.
  // - Removes crash files that are successfully uploaded.
  //
  // Returns true if no error occurred, or |crash_dir| does not exist.
  bool SendCrashes(const base::FilePath& crash_dir);

  // Sends the user-specific crashes. This function calls SendCrashes() for each
  // user specific crash directory. Returns true if no error occurred.
  bool SendUserCrashes();

  // Returns the temporary directory used in the object. Valid after Init() is
  // completed successfully.
  const base::FilePath& temp_dir() const { return scoped_temp_dir_.GetPath(); }

 private:
  // Requests the shell script to send a crash report associated with the given
  // meta file. The shell script may decide not to send, because there is still
  // some logic in the script for skipping crash files.
  // TODO(satorux): Move that logic from the shell script to C++.
  bool RequestToSendCrash(const base::FilePath& meta_file);

  std::unique_ptr<MetricsLibraryInterface> metrics_lib_;
  const base::FilePath shell_script_;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxyInterface> proxy_;
  base::ScopedTempDir scoped_temp_dir_;
  int max_crash_rate_;

  DISALLOW_COPY_AND_ASSIGN(Sender);
};

}  // namespace util

#endif  // CRASH_REPORTER_CRASH_SENDER_UTIL_H_
