// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRASH_REPORTER_CRASH_COLLECTOR_H_
#define CRASH_REPORTER_CRASH_COLLECTOR_H_

#include <sys/stat.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>
#include <base/time/clock.h>
#include <base/time/time.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST
#include <session_manager/dbus-proxies.h>

// User crash collector.
class CrashCollector {
 public:
  typedef bool (*IsFeedbackAllowedFunction)();

  explicit CrashCollector(const std::string& collector_name);
  explicit CrashCollector(const std::string& collector_name,
                          bool force_user_crash_dir);

  virtual ~CrashCollector();

  void set_lsb_release_for_test(const base::FilePath& lsb_release) {
    lsb_release_ = lsb_release;
  }

  // For testing, set the directory always returned by
  // GetCreatedCrashDirectoryByEuid.
  void set_crash_directory_for_test(const base::FilePath& forced_directory) {
    forced_crash_directory_ = forced_directory;
  }

  // For testing, set the directory where cached files are stored instead of
  // kCrashReporterStatePath.
  void set_reporter_state_directory_for_test(
      const base::FilePath& forced_directory) {
    crash_reporter_state_path_ = forced_directory;
  }

  // For testing, set the log config file path instead of kDefaultLogConfig.
  void set_log_config_path(const std::string& path) {
    log_config_path_ = base::FilePath(path);
  }

  // For testing, set the clock to use to get the report timestamp.
  void set_test_clock(std::unique_ptr<base::Clock> test_clock) {
    test_clock_ = std::move(test_clock);
  }

  // Initialize the crash collector for detection of crashes, given a
  // metrics collection enabled oracle.
  void Initialize(IsFeedbackAllowedFunction is_metrics_allowed);

  // Initialize the system crash paths.
  static bool InitializeSystemCrashDirectories();

 protected:
  friend class CrashCollectorTest;
  FRIEND_TEST(ArcContextTest, GetAndroidVersion);
  FRIEND_TEST(ChromeCollectorTest, HandleCrash);
  FRIEND_TEST(CrashCollectorTest, CheckHasCapacityCorrectBasename);
  FRIEND_TEST(CrashCollectorTest, CheckHasCapacityStrangeNames);
  FRIEND_TEST(CrashCollectorTest, CheckHasCapacityUsual);
  FRIEND_TEST(CrashCollectorTest, CreateDirectoryWithSettingsMode);
  FRIEND_TEST(CrashCollectorTest, CreateDirectoryWithSettingsNonDir);
  FRIEND_TEST(CrashCollectorTest, CreateDirectoryWithSettingsSubdir);
  FRIEND_TEST(CrashCollectorTest, CreateDirectoryWithSettingsSymlinks);
  FRIEND_TEST(CrashCollectorTest, ForkExecAndPipe);
  FRIEND_TEST(CrashCollectorTest, FormatDumpBasename);
  FRIEND_TEST(CrashCollectorTest, GetCrashDirectoryInfo);
  FRIEND_TEST(CrashCollectorTest, GetCrashPath);
  FRIEND_TEST(CrashCollectorTest, GetLogContents);
  FRIEND_TEST(CrashCollectorTest, GetProcessTree);
  FRIEND_TEST(CrashCollectorTest, GetUptime);
  FRIEND_TEST(CrashCollectorTest, Initialize);
  FRIEND_TEST(CrashCollectorTest, MetaData);
  FRIEND_TEST(CrashCollectorTest, ParseProcessTicksFromStat);
  FRIEND_TEST(CrashCollectorTest, Sanitize);
  FRIEND_TEST(CrashCollectorTest, StripMacAddressesBasic);
  FRIEND_TEST(CrashCollectorTest, StripMacAddressesBulk);
  FRIEND_TEST(CrashCollectorTest, StripSensitiveDataSample);
  FRIEND_TEST(CrashCollectorTest, StripEmailAddresses);
  FRIEND_TEST(CrashCollectorTest, TruncatedLog);
  FRIEND_TEST(CrashCollectorTest, WriteNewFile);
  FRIEND_TEST(CrashCollectorTest, WriteNewCompressedFile);
  FRIEND_TEST(CrashCollectorTest, WriteNewCompressedFileFailsIfFileExists);
  FRIEND_TEST(ForkExecAndPipeTest, BadExecutable);
  FRIEND_TEST(ForkExecAndPipeTest, BadOutputFile);
  FRIEND_TEST(ForkExecAndPipeTest, Basic);
  FRIEND_TEST(ForkExecAndPipeTest, ExistingOutputFile);
  FRIEND_TEST(ForkExecAndPipeTest, NULLParam);
  FRIEND_TEST(ForkExecAndPipeTest, NoParams);
  FRIEND_TEST(ForkExecAndPipeTest, NonZeroReturnValue);
  FRIEND_TEST(ForkExecAndPipeTest, SegFaultHandling);
  FRIEND_TEST(ForkExecAndPipeTest, StderrCaptured);

  // Default value if OS version/description cannot be determined.
  static const char* const kUnknownValue;

  // Set maximum enqueued crashes in a crash directory.
  static const int kMaxCrashDirectorySize;

  // UID for root account.
  static const uid_t kRootUid;

  // Set up D-Bus.
  virtual void SetUpDBus();

  // Writes |data| of |size| to |filename|, which must be a new file.
  // If the file already exists or writing fails, return a negative value.
  // Otherwise returns the number of bytes written.
  int WriteNewFile(const base::FilePath& filename, const char* data, int size);

  // Writes |data| of |size| to |filename|, which must be a new file ending in
  // ".gz". File will be a gzip-compressed file. Returns true on success,
  // false on failure.
  bool WriteNewCompressedFile(const base::FilePath& filename,
                              const char* data,
                              size_t size);

  // Return a filename that has only [a-z0-1_] characters by mapping
  // all others into '_'.
  std::string Sanitize(const std::string& name);

  // Strip any data that the user might not want sent up to the crash server.
  // |contents| is modified in-place.
  void StripSensitiveData(std::string* contents);
  void StripMacAddresses(std::string* contents);
  void StripEmailAddresses(std::string* contents);

  bool GetUserCrashDirectories(std::vector<base::FilePath>* directories);
  base::FilePath GetUserCrashDirectory();
  base::FilePath GetCrashDirectoryInfo(uid_t process_euid,
                                       uid_t default_user_id,
                                       gid_t default_user_group,
                                       mode_t* mode,
                                       uid_t* directory_owner,
                                       gid_t* directory_group);

  // Determines the crash directory for given euid, and creates the
  // directory if necessary with appropriate permissions.  If
  // |out_of_capacity| is not nullptr, it is set to indicate if the call
  // failed due to not having capacity in the crash directory. Returns
  // true whether or not directory needed to be created, false on any
  // failure.  If the crash directory is at capacity, returns false.
  bool GetCreatedCrashDirectoryByEuid(uid_t euid,
                                      base::FilePath* crash_file_path,
                                      bool* out_of_capacity);

  // Create a directory using the specified mode/user/group, and make sure it
  // is actually a directory with the specified permissions.
  static bool CreateDirectoryWithSettings(const base::FilePath& dir,
                                          mode_t mode,
                                          uid_t owner,
                                          gid_t group,
                                          int* dir_fd);

  // Format crash name based on components.
  std::string FormatDumpBasename(const std::string& exec_name,
                                 time_t timestamp,
                                 pid_t pid);

  // Create a file path to a file in |crash_directory| with the given
  // |basename| and |extension|.
  base::FilePath GetCrashPath(const base::FilePath& crash_directory,
                              const std::string& basename,
                              const std::string& extension);

  // Returns the path /proc/<pid>.
  static base::FilePath GetProcessPath(pid_t pid);

  static bool GetUptime(base::TimeDelta* uptime);
  static bool GetUptimeAtProcessStart(pid_t pid, base::TimeDelta* uptime);

  virtual bool GetExecutableBaseNameFromPid(pid_t pid, std::string* base_name);

  // Check given crash directory still has remaining capacity for another
  // crash.
  bool CheckHasCapacity(const base::FilePath& crash_directory);
  bool CheckHasCapacity(const base::FilePath& crash_directory,
                        const std::string& display_path);

  // Write a log applicable to |exec_name| to |output_file| based on the
  // log configuration file at |config_path|. If |output_file| ends in .gz, it
  // will be compressed in gzip format, otherwise it will be plaintext.
  bool GetLogContents(const base::FilePath& config_path,
                      const std::string& exec_name,
                      const base::FilePath& output_file);

  // Write details about the process tree of |pid| to |output_file|.
  bool GetProcessTree(pid_t pid, const base::FilePath& output_file);

  // Add non-standard meta data to the crash metadata file.  Call
  // before calling WriteCrashMetaData.  Key must not contain "=" or
  // "\n" characters.  Value must not contain "\n" characters.
  void AddCrashMetaData(const std::string& key, const std::string& value);

  // Add a file to be uploaded to the crash reporter server. The file must
  // persist until the crash report is sent; ideally it should live in the same
  // place as the .meta file, so it can be cleaned up automatically.
  void AddCrashMetaUploadFile(const std::string& key, const std::string& path);

  // Add non-standard meta data to the crash metadata file.
  // Data added though this call will be uploaded to the crash reporter server,
  // appearing as a form field. Virtual for testing.
  virtual void AddCrashMetaUploadData(const std::string& key,
                                      const std::string& value);

  // Like AddCrashMetaUploadData, but loads the value from the file at |path|.
  // The file is not uploaded as an attachment, unlike AddCrashMetaUploadFile.
  void AddCrashMetaUploadText(const std::string& key, const std::string& path);

  // Gets the corresponding value for |key| from the lsb-release file.
  std::string GetLsbReleaseValue(const std::string& key) const;

  // Returns the OS version written to the metadata file.
  virtual std::string GetOsVersion() const;

  // Returns the OS description written to the metadata file.
  virtual std::string GetOsDescription() const;

  // Write a file of metadata about crash.
  void WriteCrashMetaData(const base::FilePath& meta_path,
                          const std::string& exec_name,
                          const std::string& payload_name);

  // Returns true if chrome crashes should be handled.
  bool ShouldHandleChromeCrashes();

  IsFeedbackAllowedFunction is_feedback_allowed_function_;
  std::string extra_metadata_;
  base::FilePath forced_crash_directory_;
  base::FilePath lsb_release_;
  base::FilePath system_crash_path_;
  base::FilePath crash_reporter_state_path_;
  base::FilePath log_config_path_;
  size_t max_log_size_;
  std::unique_ptr<base::Clock> test_clock_;

  scoped_refptr<dbus::Bus> bus_;

  // D-Bus proxy for session manager interface.
  std::unique_ptr<org::chromium::SessionManagerInterfaceProxyInterface>
      session_manager_proxy_;

  // Hash a string to a number.  We define our own hash function to not
  // be dependent on a C++ library that might change.
  static unsigned HashString(base::StringPiece input);

 private:
  static bool ParseProcessTicksFromStat(base::StringPiece stat,
                                        uint64_t* ticks);

  // True if reports should always be stored in the user crash directory.
  const bool force_user_crash_dir_;

  // Creates a new file and returns a file descriptor to it. Helper for
  // WriteNewFile() and WriteNewCompressedFile().
  base::ScopedFD GetNewFileHandle(const base::FilePath& filename);

  DISALLOW_COPY_AND_ASSIGN(CrashCollector);
};

#endif  // CRASH_REPORTER_CRASH_COLLECTOR_H_
