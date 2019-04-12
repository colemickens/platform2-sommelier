// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_SENDER_UTIL_H_
#define CRASH_REPORTER_CRASH_SENDER_UTIL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/files/scoped_temp_dir.h>
#include <base/time/time.h>
#include <brillo/key_value_store.h>
#include <metrics/metrics_library.h>
#include <session_manager/dbus-proxies.h>

namespace util {

// Maximum crashes to send per 24 hours.
constexpr int kMaxCrashRate = 32;

// Maximum time to wait for ensuring a meta file is complete.
constexpr int kMaxHoldOffTimeInSeconds = 30;

// Maximum time to sleep before attempting to send a crash report. This value is
// inclusive as an upper bound, thus 0 means a crash report can be sent
// immediately.
constexpr int kMaxSpreadTimeInSeconds = 600;

// Represents a name-value pair for an environment variable.
struct EnvPair {
  const char* name;
  const char* value;
};

// Parsed command line flags.
struct CommandLineFlags {
  base::TimeDelta max_spread_time;
  bool ignore_rate_limits = false;
};

// Crash information obtained in ChooseAction().
struct CrashInfo {
  brillo::KeyValueStore metadata;
  base::FilePath payload_file;
  std::string payload_kind;
  // Last modification time of the associated .meta file
  base::Time last_modified;
};

// Details of a crash report. Contains more information than CrashInfo, as
// additional information is extracted at a stage later stage.
struct CrashDetails {
  base::FilePath meta_file;
  base::FilePath payload_file;
  std::string payload_kind;
  std::string exec_name;
  std::string client_id;
};

// Represents a metadata file name, and its parsed metadata.
typedef std::pair<base::FilePath, CrashInfo> MetaFile;

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
};

// Parses the command line, and handles the command line flags.
//
// This function also sets the predefined environment valuables to the default
// values, or the values specified by -e options.
//
// On error, the process exits as a failure with an error message for the
// first-encountered error.
void ParseCommandLine(int argc,
                      const char* const* argv,
                      CommandLineFlags* flags);

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
// enabled, etc. The crash information will be stored in |info| for reuse.
Action ChooseAction(const base::FilePath& meta_file,
                    MetricsLibraryInterface* metrics_lib,
                    std::string* reason,
                    CrashInfo* info);

// Sort the vector of crash reports so that the report we want to send first
// is at the front of the vector.
void SortReports(std::vector<MetaFile>* reports);

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
// The original contents of |metadata| will be lost.
bool ParseMetadata(const std::string& raw_metadata,
                   brillo::KeyValueStore* metadata);

// Returns true if the metadata is complete.
bool IsCompleteMetadata(const brillo::KeyValueStore& metadata);

// Returns true if the given timestamp file is new enough, indicating that there
// was a recent attempt to send a crash report.
bool IsTimestampNewEnough(const base::FilePath& timestamp_file);

// Returns true if sending a crash report now does not exceed |max_crash_rate|
// per 24 hours.
//
// This function checks/creates/removes timestamp files in |timestamps_dir| to
// track how many attempts were made to send crash reports in that past 24
// hours. Even if sending failed, it's counted as an attempt.
bool IsBelowRate(const base::FilePath& timestamps_dir, int max_crash_rate);

// Computes a sleep time needed before attempting to send a new crash report.
// On success, returns true and stores the result in |sleep_time|. On error,
// returns false.
bool GetSleepTime(const base::FilePath& meta_file,
                  const base::TimeDelta& max_spread_time,
                  base::TimeDelta* sleep_time);

// Returns the value for the key from the key-value store, or "undefined", if
// the key is not found ("undefined" will be used in crash reports where the
// values are missing).
std::string GetValueOrUndefined(const brillo::KeyValueStore& store,
                                const std::string& key);

// Gets the client ID if it exists, otherwise it generates it, saves it and
// returns that new ID. If it is unable to create the directory for storage, the
// empty string is returned.
std::string GetClientId();

// A helper class for sending crashes. The behaviors can be customized with
// Options class for unit testing.
//
// Crash reports will be sent even when the device is on a mobile data
// connection (see crbug.com/185110 for discussion).
class Sender {
 public:
  struct Options {
    // The shell script used for sending crashes.
    base::FilePath shell_script = base::FilePath("/sbin/crash_sender.sh");

    // Session manager client for locating the user-specific crash directories.
    org::chromium::SessionManagerInterfaceProxyInterface* proxy = nullptr;

    // Maximum crashes to send per 24 hours.
    int max_crash_rate = kMaxCrashRate;

    // Maximum time to sleep before attempting to send.
    base::TimeDelta max_spread_time;

    // Alternate sleep function for unit testing.
    base::Callback<void(base::TimeDelta)> sleep_function;

    // Forced list of proxy servers for testing.
    std::vector<std::string> proxy_servers;
  };

  Sender(std::unique_ptr<MetricsLibraryInterface> metrics_lib,
         const Options& options);

  // Initializes the sender object. Returns true on success.
  bool Init();

  // Removes invalid files in |crash_dir|, that are unknown, corrupted, or
  // invalid in other ways, and picks crash reports that should be sent to the
  // server. The meta files of the latter will be stored in |to_send|.
  void RemoveAndPickCrashFiles(const base::FilePath& directory,
                               std::vector<MetaFile>* reports_to_send);

  // Sends each crash in |crash_meta_files|, in multiple steps:
  //
  // For each meta file:
  // - Sleeps to avoid overloading the network
  // - Checks if the device enters guest mode, and stops if entered.
  // - Enforces the rate limit per 24 hours.
  // - Removes crash files that are successfully uploaded.
  void SendCrashes(const std::vector<MetaFile>& crash_meta_files);

  // Get a list of all directories that might hold user-specific crashes.
  std::vector<base::FilePath> GetUserCrashDirectories();

  // Returns the temporary directory used in the object. Valid after Init() is
  // completed successfully.
  const base::FilePath& temp_dir() const { return scoped_temp_dir_.GetPath(); }

 private:
  // Requests the shell script to send a crash report represented with the given
  // crash details.
  bool RequestToSendCrash(const CrashDetails& details);

  // Makes sure we have the DBus object initialized and connected.
  void EnsureDBusIsReady();

  std::unique_ptr<MetricsLibraryInterface> metrics_lib_;
  const base::FilePath shell_script_;
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxyInterface> proxy_;
  std::vector<std::string> proxy_servers_;
  base::ScopedTempDir scoped_temp_dir_;
  const int max_crash_rate_;
  const base::TimeDelta max_spread_time_;
  base::Callback<void(base::TimeDelta)> sleep_function_;
  scoped_refptr<dbus::Bus> bus_;

  DISALLOW_COPY_AND_ASSIGN(Sender);
};

}  // namespace util

#endif  // CRASH_REPORTER_CRASH_SENDER_UTIL_H_
