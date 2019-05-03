// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crash-reporter/crash_sender_util.h"

#include <inttypes.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <base/environment.h>
#include <base/files/file_enumerator.h>
#include <base/files/file_util.h>
#include <base/guid.h>
#include <base/rand_util.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <base/time/time.h>
#include <brillo/flag_helper.h>
#include <brillo/http/http_proxy.h>
#include <brillo/http/http_transport.h>
#include <brillo/http/http_utils.h>

#include "crash-reporter/crash_sender_paths.h"
#include "crash-reporter/paths.h"
#include "crash-reporter/util.h"

namespace util {

namespace {

// URL to send official build crash reports to.
constexpr char kReportUploadProdUrl[] = "https://clients2.google.com/cr/report";
constexpr char kUndefined[] = "undefined";
constexpr char kChromeOsProduct[] = "ChromeOS";
constexpr char kUploadVarPrefix[] = "upload_var_";
constexpr char kUploadTextPrefix[] = "upload_text_";
constexpr char kUploadFilePrefix[] = "upload_file_";

// Length of the client ID. This is a standard GUID which has the dashes
// removed.
constexpr size_t kClientIdLength = 32U;

// getenv() wrapper that returns an empty string, if the environment variable is
// not defined.
std::string GetEnv(const std::string& name) {
  const char* value = getenv(name.c_str());
  return value ? value : "";
}

// Shows the usage of crash_sender and exits the process as a success.
void ShowUsageAndExit() {
  printf(
      "Usage: crash_sender [options]\n"
      "Options:\n"
      " -e <var>=<val>     Set env |var| to |val| (only some vars)\n");
  exit(EXIT_SUCCESS);
}


// Returns true if the given report kind is known.
// TODO(satorux): Move collector constants to a common file.
bool IsKnownKind(const std::string& kind) {
  return (kind == "minidump" || kind == "kcrash" || kind == "log" ||
          kind == "devcore" || kind == "eccrash" || kind == "bertdump");
}

// Returns true if the given key is valid for crash metadata.
bool IsValidKey(const std::string& key) {
  if (key.empty())
    return false;

  for (const char c : key) {
    if (!(base::IsAsciiAlpha(c) || base::IsAsciiDigit(c) || c == '_' ||
          c == '-' || c == '.')) {
      return false;
    }
  }

  return true;
}

// Converts metadata into CrashInfo.
void MetadataToCrashInfo(const brillo::KeyValueStore& metadata,
                         CrashInfo* info) {
  info->payload_file = GetBaseNameFromMetadata(metadata, "payload");
  info->payload_kind = GetKindFromPayloadPath(info->payload_file);
}

}  // namespace

void ParseCommandLine(int argc,
                      const char* const* argv,
                      CommandLineFlags* flags) {
  std::map<std::string, std::string> env_vars;
  for (const EnvPair& pair : kEnvironmentVariables) {
    // Honor the existing value if it's already set.
    const char* value = getenv(pair.name);
    env_vars[pair.name] = value ? value : pair.value;
  }

  // Process -e options, and collect other options.
  std::vector<const char*> new_argv;
  new_argv.push_back(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "-e") {
      if (i + 1 < argc) {
        ++i;
        std::string name_value = argv[i];
        std::vector<std::string> pair = base::SplitString(
            name_value, "=", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
        if (pair.size() == 2) {
          if (env_vars.count(pair[0]) == 0) {
            LOG(ERROR) << "Unknown variable name: " << pair[0];
            exit(EXIT_FAILURE);
          }
          env_vars[pair[0]] = pair[1];
        } else {
          LOG(ERROR) << "Malformed value for -e: " << name_value;
          exit(EXIT_FAILURE);
        }
      } else {
        LOG(ERROR) << "Value for -e is missing";
        exit(EXIT_FAILURE);
      }
    } else {
      new_argv.push_back(argv[i]);
    }
  }
  // argv[argc] should be a null pointer per the C standard.
  new_argv.push_back(nullptr);

  // Process the remaining flags.
  DEFINE_bool(h, false, "Show this help and exit");
  DEFINE_int32(max_spread_time, kMaxSpreadTimeInSeconds,
               "Max time in secs to sleep before sending (0 to send now)");
  DEFINE_string(crash_directory, "",
                "If set, upload only crashes in this directory.");
  const std::string ignore_rate_limits_description = base::StringPrintf(
      "Ignore normal limit of %d crash uploads per day", kMaxCrashRate);
  DEFINE_bool(ignore_rate_limits, false,
              ignore_rate_limits_description.c_str());
  const std::string ignore_hold_off_time_description = base::StringPrintf(
      "Assume all crash reports are completely written to disk. Do not "
      "wait %" PRId64 " seconds after meta file is written to start sending.",
      kMaxHoldOffTime.InSeconds());
  DEFINE_bool(ignore_hold_off_time, false,
              ignore_hold_off_time_description.c_str());
  brillo::FlagHelper::Init(new_argv.size() - 1, new_argv.data(),
                           "Chromium OS Crash Sender");
  // TODO(satorux): Remove this once -e option is gone.
  if (FLAGS_h)
    ShowUsageAndExit();

  if (FLAGS_max_spread_time < 0) {
    LOG(ERROR) << "Invalid value for max spread time: "
               << FLAGS_max_spread_time;
    exit(EXIT_FAILURE);
  }
  flags->max_spread_time = base::TimeDelta::FromSeconds(FLAGS_max_spread_time);
  flags->crash_directory = FLAGS_crash_directory;
  flags->ignore_rate_limits = FLAGS_ignore_rate_limits;
  flags->ignore_hold_off_time = FLAGS_ignore_hold_off_time;

  // Set the predefined environment variables.
  for (const auto& it : env_vars)
    setenv(it.first.c_str(), it.second.c_str(), 1 /* overwrite */);
}

bool IsMock() {
  return base::PathExists(
      paths::GetAt(paths::kSystemRunStateDirectory, paths::kMockCrashSending));
}

bool IsMockSuccessful() {
  int64_t file_size;
  return base::GetFileSize(paths::GetAt(paths::kSystemRunStateDirectory,
                                        paths::kMockCrashSending),
                           &file_size) &&
         !file_size;
}

bool ShouldPauseSending() {
  return (base::PathExists(paths::Get(paths::kPauseCrashSending)) &&
          GetEnv("OVERRIDE_PAUSE_SENDING") == "0");
}

std::string GetImageType() {
  if (util::IsTestImage())
    return "test";
  else if (util::IsDeveloperImage())
    return "dev";
  else if (util::IsForceOfficialSet())
    return "force-official";
  else if (IsMock() && !IsMockSuccessful())
    return "mock-fail";
  else
    return "";
}

base::FilePath GetBasePartOfCrashFile(const base::FilePath& file_name) {
  std::vector<std::string> components;
  file_name.GetComponents(&components);

  std::vector<std::string> parts = base::SplitString(
      components.back(), ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() < 4) {
    LOG(ERROR) << "Unexpected file name format: " << file_name.value();
    return file_name;
  }

  parts.resize(4);
  const std::string base_name = base::JoinString(parts, ".");

  if (components.size() == 1)
    return base::FilePath(base_name);
  return file_name.DirName().Append(base_name);
}

void RemoveOrphanedCrashFiles(const base::FilePath& crash_dir) {
  base::FileEnumerator iter(crash_dir, true /* recursive */,
                            base::FileEnumerator::FILES, "*");
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    // Get the meta data file path.
    const base::FilePath meta_file =
        base::FilePath(GetBasePartOfCrashFile(file).value() + ".meta");

    // Check how old the file is.
    base::File::Info info;
    if (!base::GetFileInfo(file, &info)) {
      PLOG(WARNING) << "Failed to get file info: " << file.value();
      continue;
    }
    base::TimeDelta delta = base::Time::Now() - info.last_modified;

    if (!base::PathExists(meta_file) && delta.InHours() >= 24) {
      LOG(INFO) << "Removing old orphaned file: " << file.value();
      if (!base::DeleteFile(file, false /* recursive */))
        PLOG(WARNING) << "Failed to remove " << file.value();
    }
  }
}

Action ChooseAction(const base::FilePath& meta_file,
                    MetricsLibraryInterface* metrics_lib,
                    std::string* reason,
                    CrashInfo* info) {
  if (!IsMock() && !IsOfficialImage()) {
    *reason = "Not an official OS version";
    return kRemove;
  }

  // AreMetricsEnabled() returns false in guest mode, thus IsGuestMode() should
  // also be checked here (otherwise, all crash files are deleted in guest
  // mode).
  //
  // Note that this check is slightly racey, but should be rare enough for us
  // not to care:
  //
  // - crash_sender checks IsGuestMode() and it returns false
  // - User logs in to guest mode
  // - crash_sender checks AreMetricsEnabled() and it's now false
  // - Reports are deleted
  if (!metrics_lib->IsGuestMode() && !metrics_lib->AreMetricsEnabled()) {
    *reason = "Crash reporting is disabled";
    return kRemove;
  }

  std::string raw_metadata;
  if (!base::ReadFileToString(meta_file, &raw_metadata)) {
    PLOG(WARNING) << "Igonoring: metadata file is inaccessible";
    return kIgnore;
  }

  if (!ParseMetadata(raw_metadata, &info->metadata)) {
    *reason = "Corrupted metadata: " + raw_metadata;
    return kRemove;
  }

  MetadataToCrashInfo(info->metadata, info);

  if (info->payload_file.empty()) {
    *reason = "Payload is not found in the meta data: " + raw_metadata;
    return kRemove;
  }

  // Make it an absolute path.
  info->payload_file = meta_file.DirName().Append(info->payload_file);

  if (!base::PathExists(info->payload_file)) {
    // TODO(satorux): logging_CrashSender.py expects "Missing payload" in the
    // error message. Revise the autotest once the rewrite to C++ is complete.
    *reason = "Missing payload: " + info->payload_file.value();
    return kRemove;
  }

  if (!IsKnownKind(info->payload_kind)) {
    *reason = "Unknown kind: " + info->payload_kind;
    return kRemove;
  }

  base::File::Info file_info;
  if (!base::GetFileInfo(meta_file, &file_info)) {
    // Should not happen since it succeeded to read the file.
    *reason = "Failed to get file info";
    return kIgnore;
  }

  info->last_modified = file_info.last_modified;
  if (!IsCompleteMetadata(info->metadata)) {
    const base::TimeDelta delta = base::Time::Now() - file_info.last_modified;
    if (delta.InHours() >= 24) {
      // TODO(satorux): logging_CrashSender.py expects the following string as
      // error message. Revise the autotest once the rewrite to C++ is complete.
      *reason = "Removing old incomplete metadata";
      return kRemove;
    } else {
      *reason = "Recent incomplete metadata";
      return kIgnore;
    }
  }

  if (info->payload_kind == "devcore" && !IsDeviceCoredumpUploadAllowed()) {
    *reason = "Device coredump upload not allowed";
    return kIgnore;
  }

  return kSend;
}

void SortReports(std::vector<MetaFile>* reports) {
  std::sort(reports->begin(), reports->end(),
            [](const MetaFile& m1, const MetaFile& m2) {
              // Send older reports first to avoid starvation if there is a
              // constant stream of crashes (that is, if thing A is producing
              // crash reports constantly, and thing B produces one crash
              // report, make sure thing B's crash report gets sent eventually.)
              return m1.second.last_modified < m2.second.last_modified;
            });
}

void RemoveReportFiles(const base::FilePath& meta_file) {
  if (meta_file.Extension() != ".meta") {
    LOG(ERROR) << "Not a meta file: " << meta_file.value();
    return;
  }

  const std::string pattern =
      meta_file.BaseName().RemoveExtension().value() + ".*";

  base::FileEnumerator iter(meta_file.DirName(), false /* recursive */,
                            base::FileEnumerator::FILES, pattern);
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    if (!base::DeleteFile(file, false /* recursive */))
      PLOG(WARNING) << "Failed to remove " << file.value();
  }
}

std::vector<base::FilePath> GetMetaFiles(const base::FilePath& crash_dir) {
  std::vector<base::FilePath> meta_files;
  if (!base::DirectoryExists(crash_dir)) {
    // Directory not existing is not an error.
    return meta_files;
  }

  base::FileEnumerator iter(crash_dir, false /* recursive */,
                            base::FileEnumerator::FILES, "*.meta");
  std::vector<std::pair<base::Time, base::FilePath>> time_meta_pairs;
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    base::File::Info info;
    if (!base::GetFileInfo(file, &info)) {
      PLOG(WARNING) << "Failed to get file info: " << file.value();
      continue;
    }
    time_meta_pairs.push_back(std::make_pair(info.last_modified, file));
  }
  std::sort(time_meta_pairs.begin(), time_meta_pairs.end());

  for (const auto& pair : time_meta_pairs)
    meta_files.push_back(pair.second);
  return meta_files;
}

base::FilePath GetBaseNameFromMetadata(const brillo::KeyValueStore& metadata,
                                       const std::string& key) {
  std::string value;
  if (!metadata.GetString(key, &value))
    return base::FilePath();

  return base::FilePath(value).BaseName();
}

std::string GetKindFromPayloadPath(const base::FilePath& payload_path) {
  std::vector<std::string> parts =
      base::SplitString(payload_path.BaseName().value(), ".",
                        base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  // Suppress "gz".
  if (parts.size() >= 2 && parts.back() == "gz")
    parts.pop_back();

  if (parts.size() <= 1)
    return "";

  std::string extension = parts.back();
  if (extension == "dmp")
    return "minidump";

  return extension;
}

bool ParseMetadata(const std::string& raw_metadata,
                   brillo::KeyValueStore* metadata) {
  metadata->Clear();
  if (!metadata->LoadFromString(raw_metadata))
    return false;

  for (const auto& key : metadata->GetKeys()) {
    if (!IsValidKey(key))
      return false;
  }

  return true;
}

bool IsCompleteMetadata(const brillo::KeyValueStore& metadata) {
  // *.meta files always end with done=1 so we can tell if they are complete.
  std::string value;
  if (!metadata.GetString("done", &value))
    return false;
  return value == "1";
}

bool IsTimestampNewEnough(const base::FilePath& timestamp_file) {
  const base::Time threshold =
      base::Time::Now() - base::TimeDelta::FromHours(24);

  base::File::Info info;
  if (!base::GetFileInfo(timestamp_file, &info)) {
    PLOG(ERROR) << "Failed to get file info: " << timestamp_file.value();
    return false;
  }

  return threshold < info.last_modified;
}

bool IsBelowRate(const base::FilePath& timestamps_dir, int max_crash_rate) {
  if (!base::CreateDirectory(timestamps_dir)) {
    PLOG(ERROR) << "Failed to create a timestamps directory: "
                << timestamps_dir.value();
    return false;
  }

  // Count the number of timestamp files, that were written in the past 24
  // hours. Remove files that are older.
  int current_rate = 0;
  base::FileEnumerator iter(timestamps_dir, false /* recursive */,
                            base::FileEnumerator::FILES, "*");
  for (base::FilePath file = iter.Next(); !file.empty(); file = iter.Next()) {
    if (IsTimestampNewEnough(file)) {
      ++current_rate;
    } else {
      if (!base::DeleteFile(file, false /* recursive */))
        PLOG(WARNING) << "Failed to remove " << file.value();
    }
  }
  LOG(INFO) << "Current send rate: " << current_rate << "sends/24hrs";

  if (current_rate < max_crash_rate) {
    // It's OK to send a new crash report now. Create a new timestamp to record
    // that a new attempt is made to send a crash report.
    base::FilePath temp_file;
    if (!base::CreateTemporaryFileInDir(timestamps_dir, &temp_file)) {
      PLOG(ERROR) << "Failed to create a file in " << timestamps_dir.value();
      return false;
    }
    return true;
  }

  return false;
}

bool GetSleepTime(const base::FilePath& meta_file,
                  const base::TimeDelta& max_spread_time,
                  const base::TimeDelta& hold_off_time,
                  base::TimeDelta* sleep_time) {
  base::File::Info info;
  if (!base::GetFileInfo(meta_file, &info)) {
    PLOG(ERROR) << "Failed to get file info: " << meta_file.value();
    return false;
  }

  // The meta file should be written *after* all to-be-uploaded files that it
  // references.  Nevertheless, as a safeguard, a hold-off time after writing
  // the meta file is ensured.  Also, sending of crash reports is spread out
  // randomly by up to |max_spread_time|. Thus, for the sleep call the greater
  // of the two delays is used. Use max() to ensure that holdoff_time is not
  // negative.
  const base::TimeDelta hold_off_time_remaining =
      std::max(info.last_modified + hold_off_time - base::Time::Now(),
               base::TimeDelta());

  const int seconds = (max_spread_time.InSeconds() <= 0
                           ? 0
                           : base::RandInt(0, max_spread_time.InSeconds()));
  const base::TimeDelta spread_time = base::TimeDelta::FromSeconds(seconds);

  *sleep_time = std::max(spread_time, hold_off_time_remaining);

  return true;
}

std::string GetClientId() {
  std::string client_id;
  base::FilePath client_id_dir = paths::Get(paths::kCrashSenderStateDirectory);
  if (!base::CreateDirectory(client_id_dir)) {
    PLOG(ERROR) << "Failed to create directory: " << client_id_dir.value();
    return "";
  }
  base::FilePath client_id_file = client_id_dir.Append(paths::kClientId);
  if (base::PathExists(client_id_file)) {
    if (!base::ReadFileToString(client_id_file, &client_id)) {
      PLOG(ERROR) << "Error reading client ID file: " << client_id_file.value();
    } else if (client_id.length() != kClientIdLength) {
      // Don't log what this is, otherwise we may need to scrub it.
      LOG(ERROR) << "Client ID has wrong format, regenerate it";
    } else {
      return client_id;
    }
  }
  client_id = base::GenerateGUID();
  // Strip out the dashes, we don't want those.
  base::RemoveChars(client_id, "-", &client_id);

  if (base::WriteFile(client_id_file, client_id.c_str(), client_id.length()) !=
      client_id.length()) {
    PLOG(ERROR) << "Error writing out client ID to file: "
                << client_id_file.value();
  }

  return client_id;
}

Sender::Sender(std::unique_ptr<MetricsLibraryInterface> metrics_lib,
               const Sender::Options& options)
    : metrics_lib_(std::move(metrics_lib)),
      proxy_(options.proxy),
      form_data_boundary_(options.form_data_boundary),
      always_write_uploads_log_(options.always_write_uploads_log),
      max_crash_rate_(options.max_crash_rate),
      max_spread_time_(options.max_spread_time),
      hold_off_time_(options.hold_off_time),
      sleep_function_(options.sleep_function) {}

bool Sender::Init() {
  if (!scoped_temp_dir_.CreateUniqueTempDir()) {
    PLOG(ERROR) << "Failed to create a temporary directory";
    return false;
  }

  return true;
}

void Sender::RemoveAndPickCrashFiles(const base::FilePath& crash_dir,
                                     std::vector<MetaFile>* to_send) {
  std::vector<base::FilePath> meta_files = GetMetaFiles(crash_dir);

  for (const auto& meta_file : meta_files) {
    LOG(INFO) << "Checking metadata: " << meta_file.value();

    std::string reason;
    CrashInfo info;
    switch (ChooseAction(meta_file, metrics_lib_.get(), &reason, &info)) {
      case kRemove:
        LOG(INFO) << "Removing: " << reason;
        RemoveReportFiles(meta_file);
        break;
      case kIgnore:
        LOG(INFO) << "Ignoring: " << reason;
        break;
      case kSend:
        to_send->push_back(std::make_pair(meta_file, std::move(info)));
        break;
      default:
        NOTREACHED();
    }
  }
}

void Sender::SendCrashes(const std::vector<MetaFile>& crash_meta_files) {
  if (crash_meta_files.empty())
    return;

  std::string client_id = GetClientId();

  for (const auto& pair : crash_meta_files) {
    const base::FilePath& meta_file = pair.first;
    const CrashInfo& info = pair.second;
    LOG(INFO) << "Evaluating crash report: " << meta_file.value();

    // This should be checked inside of the loop, since the device can enter
    // guest mode while sending crash reports with an interval up to
    // max_spread_time_ between sends.
    if (metrics_lib_->IsGuestMode()) {
      LOG(INFO) << "Guest mode has been entered. Delaying crash sending";
      return;
    }

    const base::FilePath timestamps_dir =
        paths::Get(paths::kTimestampsDirectory);
    if (!IsBelowRate(timestamps_dir, max_crash_rate_)) {
      LOG(INFO) << "Cannot send more crashes. Sending " << meta_file.value()
                << " would exceed the max rate: " << max_crash_rate_;
      return;
    }

    base::TimeDelta sleep_time;
    if (!GetSleepTime(meta_file, max_spread_time_, hold_off_time_,
                      &sleep_time)) {
      LOG(WARNING) << "Failed to compute sleep time for " << meta_file.value();
      continue;
    }

    LOG(INFO) << "Scheduled to send in " << sleep_time.InSeconds() << "s";
    if (!IsMock()) {
      base::PlatformThread::Sleep(sleep_time);
    } else if (!sleep_function_.is_null()) {
      sleep_function_.Run(sleep_time);
    }

    // User-specific crash reports become inaccessible if the user signs out
    // while sleeping, thus we need to check if the metadata is still
    // accessible.
    if (!base::PathExists(meta_file)) {
      LOG(INFO) << "Metadata is no longer accessible: " << meta_file.value();
      continue;
    }

    const CrashDetails details = {
        .meta_file = meta_file,
        .payload_file = info.payload_file,
        .payload_kind = info.payload_kind,
        .client_id = client_id,
        .metadata = info.metadata,
    };
    if (!RequestToSendCrash(details)) {
      LOG(WARNING) << "Failed to send " << meta_file.value()
                   << ", not removing; will retry later";
      continue;
    }
    LOG(INFO) << "Successfully sent crash " << meta_file.value()
              << " and removing.";
    RemoveReportFiles(meta_file);
  }
}

std::vector<base::FilePath> Sender::GetUserCrashDirectories() {
  // Set up the session manager proxy if it's not given from the options.
  if (!proxy_) {
    EnsureDBusIsReady();
    proxy_.reset(new org::chromium::SessionManagerInterfaceProxy(bus_));
  }

  std::vector<base::FilePath> directories;
  util::GetUserCrashDirectories(proxy_.get(), &directories);

  return directories;
}

std::unique_ptr<brillo::http::FormData> Sender::CreateCrashFormData(
    const CrashDetails& details, std::string* product_name_out) {
  std::unique_ptr<brillo::http::FormData> form_data =
      std::make_unique<brillo::http::FormData>(form_data_boundary_);

  std::string exec_name;
  if (!details.metadata.GetString("exec_name", &exec_name))
    exec_name = kUndefined;
  form_data->AddTextField("exec_name", exec_name);

  std::string board;
  if (!GetCachedKeyValueDefault(base::FilePath(paths::kLsbRelease),
                                "CHROMEOS_RELEASE_BOARD", &board)) {
    board = kUndefined;
  }
  form_data->AddTextField("board", board);

  std::string hwclass = util::GetHardwareClass();
  form_data->AddTextField("hwclass", hwclass);

  // When uploading Chrome reports we need to report the right product and
  // version. If the meta file does not specify it we try to examine os-release
  // content. If not available there product gets assigned default product name
  // and version is derived from CHROMEOS_RELEASE_VERSION in /etc/lsb-release.
  std::string product;
  if (!details.metadata.GetString("upload_var_prod", &product)) {
    product =
        GetOsReleaseValue({"GOOGLE_CRASH_ID", "ID"}).value_or(kChromeOsProduct);
  }
  form_data->AddTextField("prod", product);

  std::string version;
  if (!details.metadata.GetString("upload_var_ver", &version)) {
    if (!details.metadata.GetString("ver", &version)) {
      version = GetOsReleaseValue(
                    {"GOOGLE_CRASH_VERSION_ID", "BUILD_ID", "VERSION_ID"})
                    .value_or(kUndefined);
    }
  }
  form_data->AddTextField("ver", version);

  std::string sig;
  if (details.metadata.GetString("sig", &sig)) {
    form_data->AddTextField("sig", sig);
    form_data->AddTextField("sig2", sig);
  }
  brillo::ErrorPtr file_error;
  if (!form_data->AddFileField("upload_file_" + details.payload_kind,
                               details.payload_file, {}, &file_error)) {
    LOG(ERROR) << "Failed adding payload file as attachment: "
               << file_error->GetMessage();
    return nullptr;
  }

  // TODO(crbug.com/391887): Apply the changes from crrev.com/c/1581125 to
  // remove this when we also update the testing to check this correctly too.
  std::string log_value;
  if (details.metadata.GetString("log", &log_value)) {
    brillo::ErrorPtr error;
    base::FilePath log_file = details.meta_file.DirName().Append(
        base::FilePath(log_value).BaseName());
    if (base::PathExists(log_file) &&
        !form_data->AddFileField("log", log_file, {}, &error)) {
      LOG(ERROR) << "Failed attaching log file " << log_file.value()
                 << " of: " << error->GetMessage();
    }
  }

  for (const auto& key : details.metadata.GetKeys()) {
    if (!base::StartsWith(key, "upload_", base::CompareCase::SENSITIVE) ||
        key == "upload_var_prod" || key == "upload_var_ver" ||
        key == "upload_var_guid") {
      continue;
    }
    std::string value;
    details.metadata.GetString(key, &value);
    brillo::ErrorPtr error;
    if (base::StartsWith(key, kUploadVarPrefix, base::CompareCase::SENSITIVE)) {
      form_data->AddTextField(key.substr(sizeof(kUploadVarPrefix) - 1), value);
    } else if (base::StartsWith(key, kUploadTextPrefix,
                                base::CompareCase::SENSITIVE)) {
      std::string value_content;
      if (base::ReadFileToString(base::FilePath(value), &value_content)) {
        form_data->AddTextField(key.substr(sizeof(kUploadTextPrefix) - 1),
                                value_content);
      } else {
        LOG(ERROR) << "Failed attaching file contents from " << value
                   << " of: " << error->GetMessage();
      }
    } else if (base::StartsWith(key, kUploadFilePrefix,
                                base::CompareCase::SENSITIVE)) {
      base::FilePath value_file(value);
      if (base::PathExists(value_file) &&
          !form_data->AddFileField(key.substr(sizeof(kUploadFilePrefix) - 1),
                                   value_file, {}, &error)) {
        LOG(ERROR) << "Failed attaching file " << value
                   << " of: " << error->GetMessage();
      }
    }
  }

  std::string image_type = GetImageType();
  if (!image_type.empty())
    form_data->AddTextField("image_type", image_type);

  std::string boot_mode = util::GetBootModeString();
  if (!boot_mode.empty())
    form_data->AddTextField("boot_mode", boot_mode);

  std::string error_type;
  if (details.metadata.GetString("error_type", &error_type))
    form_data->AddTextField("error_type", error_type);

  LOG(INFO) << "Sending crash:";
  if (product != kChromeOsProduct)
    LOG(INFO) << "  Sending crash report on behalf of " << product;
  LOG(INFO) << "  Metadata: " << details.meta_file.value() << " ("
            << details.payload_kind << ")";
  LOG(INFO) << "  Payload: " << details.payload_file.value();
  LOG(INFO) << "  Version: " << version;
  if (!image_type.empty())
    LOG(INFO) << "  Image type: " << image_type;
  if (!boot_mode.empty())
    LOG(INFO) << "  Boot mode: " << boot_mode;
  if (IsMock()) {
    LOG(INFO) << "  Product: " << product;
    LOG(INFO) << "  URL: " << kReportUploadProdUrl;
    LOG(INFO) << "  Board: " << board;
    LOG(INFO) << "  HWClass: " << hwclass;
    if (!log_value.empty())
      LOG(INFO) << "  log: @" << log_value;
    if (!sig.empty())
      LOG(INFO) << "  sig: " << sig;
  }

  LOG(INFO) << "  Exec name: " << exec_name;
  if (!error_type.empty())
    LOG(INFO) << "  Error type: " << error_type;

  form_data->AddTextField("guid", details.client_id);

  if (product_name_out)
    *product_name_out = product;

  return form_data;
}

bool Sender::RequestToSendCrash(const CrashDetails& details) {
  std::string product_name;
  std::unique_ptr<brillo::http::FormData> form_data =
      CreateCrashFormData(details, &product_name);
  std::string report_id;
  if (!IsMock()) {
    // Determine the proxy server if it's not given from the options.
    if (proxy_servers_.empty()) {
      EnsureDBusIsReady();
      brillo::http::GetChromeProxyServers(bus_, kReportUploadProdUrl,
                                          &proxy_servers_);
    }

    std::shared_ptr<brillo::http::Transport> transport;
    if (proxy_servers_.empty() || proxy_servers_[0] == "direct://") {
      transport = brillo::http::Transport::CreateDefault();
    } else {
      transport =
          brillo::http::Transport::CreateDefaultWithProxy(proxy_servers_[0]);
    }

    brillo::ErrorPtr upload_error;
    std::unique_ptr<brillo::http::Response> response =
        brillo::http::PostFormDataAndBlock(
            kReportUploadProdUrl, std::move(form_data), {} /* headers */,
            transport, &upload_error);

    if (!response) {
      LOG(ERROR) << "Crash sending failed with error: "
                 << upload_error->GetMessage();
      return false;
    }
    if (!response->IsSuccessful()) {
      LOG(ERROR) << "Crash sending failed with HTTP "
                 << response->GetStatusCode() << ": "
                 << response->GetStatusText();
      return false;
    }

    report_id = response->ExtractDataAsString();
  } else {
    if (!IsMockSuccessful()) {
      LOG(INFO) << "Mocking unsuccessful send";
      return false;
    }
    LOG(INFO) << "Mocking successful send";

    if (!always_write_uploads_log_)
      return true;

    if (!details.metadata.GetString("fake_report_id", &report_id))
      report_id = kUndefined;
  }

  int64_t timestamp = (base::Time::Now() - base::Time::UnixEpoch()).InSeconds();
  if (product_name == "Chrome_ChromeOS")
    product_name = "Chrome";
  if (!util::IsOfficialImage()) {
    base::ReplaceSubstringsAfterOffset(&product_name, 0, "Chrome", "Chromium");
  }
  std::string silent;
  details.metadata.GetString("silent", &silent);
  if (always_write_uploads_log_ || (!USE_CHROMELESS_TTY && silent != "true")) {
    base::File upload_logs_file(
        paths::Get(paths::kChromeCrashLog),
        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
    std::string upload_log_entry =
        base::StringPrintf("%" PRId64 ",%s,%s\n", timestamp, report_id.c_str(),
                           product_name.c_str());
    if (!upload_logs_file.IsValid() ||
        upload_logs_file.WriteAtCurrentPos(upload_log_entry.c_str(),
                                           upload_log_entry.size()) !=
            upload_log_entry.size()) {
      PLOG(ERROR) << "Error writing to Chrome uploads.log file";
    }
  }
  LOG(INFO) << "Crash report receipt ID " << report_id;
  return true;
}

void Sender::EnsureDBusIsReady() {
  if (!bus_) {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::Bus(options);
    CHECK(bus_->Connect());
  }
}

base::Optional<std::string> Sender::GetOsReleaseValue(
    const std::vector<std::string>& keys) {
  if (!os_release_reader_) {
    os_release_reader_ = std::make_unique<brillo::OsReleaseReader>();
    os_release_reader_->Load();
  }
  std::string value;
  for (const auto& key : keys) {
    if (os_release_reader_->GetString(key, &value))
      return base::Optional<std::string>(value);
  }
  return base::Optional<std::string>();
}

}  // namespace util
