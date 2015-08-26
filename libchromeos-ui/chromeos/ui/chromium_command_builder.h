// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_UI_CHROMIUM_COMMAND_BUILDER_H_
#define LIBCHROMEOS_CHROMEOS_UI_CHROMIUM_COMMAND_BUILDER_H_

#include <sys/types.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <base/files/file_path.h>
#include <base/macros.h>

namespace chromeos {
namespace ui {

// ChromiumCommandBuilder facilitates building a command line for running a
// Chromium-derived binary and performing related setup.
class ChromiumCommandBuilder {
 public:
  typedef std::map<std::string, std::string> StringMap;
  typedef std::vector<std::string> StringVector;

  // Name of user account used to run the binary.
  static const char kUser[];

  // Location of the file containing newline-separated USE flags that were set
  // when the system was built.
  static const char kUseFlagsPath[];

  // Location of the file containing .info files describing Pepper plugins.
  static const char kPepperPluginsPath[];

  // Location of the lsb-release file describing the system image.
  static const char kLsbReleasePath[];

  // Location of the user-writable target of the /etc/localtime symlink.
  static const char kTimeZonePath[];

  // Default zoneinfo file used if the time zone hasn't been explicitly set.
  static const char kDefaultZoneinfoPath[];

  // Deep-memory-profiler-related files.
  static const char kDeepMemoryProfilerPrefixPath[];
  static const char kDeepMemoryProfilerTimeIntervalPath[];

  ChromiumCommandBuilder();
  ~ChromiumCommandBuilder();

  uid_t uid() const { return uid_; }
  gid_t gid() const { return gid_; }
  bool is_chrome_os_hardware() const { return is_chrome_os_hardware_; }
  bool is_developer_end_user() const { return is_developer_end_user_; }
  const StringMap& environment_variables() const {
    return environment_variables_;
  }
  const StringVector& arguments() const { return arguments_; }

  void set_base_path_for_testing(const base::FilePath& path) {
    base_path_for_testing_ = path;
  }

  // Performs just the basic initialization needed before UseFlagIsSet() and
  // IsBoard() can be used. Returns true on success.
  bool Init();

  // Determines the environment variables and arguments that should be set for
  // all Chromium-derived binaries and updates |environment_variables_| and
  // |arguments_| accordingly. Also creates necessary directories, sets resource
  // limits, etc.
  //
  // If |xauth_path| is non-empty, Chromium will be configured to connect to an
  // X server at :0. The authority file will be copied to a |uid_|-owned file
  // within the data dir.
  //
  // Returns true on success.
  bool SetUpChromium(const base::FilePath& xauth_path);

  // Configures the environment so a core dump will be written when the binary
  // crashes.
  void EnableCoreDumps();

  // Reads a user-supplied file requesting modifications to the current set of
  // arguments. The following directives are supported:
  //
  //   # This is a comment.
  //     Lines beginning with '#' are skipped.
  //
  //   --some-flag=some-value
  //     Calls AddArg("--some-flag=some-value").
  //
  //   !--flag-prefix
  //     Remove all arguments beginning with "--flag-prefix".
  //
  //   NAME=VALUE
  //     Calls AddEnvVar("NAME", "VALUE").
  //
  // Returns true on success.
  bool ApplyUserConfig(const base::FilePath& path);

  // Returns true if a USE flag named |flag| was set when the system image was
  // built.
  bool UseFlagIsSet(const std::string& flag) const;

  // Returns true if the system image was compiled for |board|.
  bool IsBoard(const std::string& board) const;

  // Adds an environment variable to |environment_variables_|. Note that this
  // method does not call setenv(); it is the caller's responsibility to
  // actually export the variables.
  void AddEnvVar(const std::string& name, const std::string& value);

  // Returns the value of an environment variable previously added via
  // AddEnvVar(). Crashes if the variable isn't set. Note that this method does
  // not call getenv().
  std::string ReadEnvVar(const std::string& name) const;

  // Adds a command-line argument.
  void AddArg(const std::string& arg);

  // Adds |pattern| to the --vmodule flag in |arguments_|.
  void AddVmodulePattern(const std::string& pattern);

 private:
  // Converts absolute path |path| into a base::FilePath, rooting it under
  // |base_path_for_testing_| if it's non-empty.
  base::FilePath GetPath(const std::string& path) const;

  // Performs X11-specific setup and returns true on success. Called by
  // InitChromium().
  bool SetUpX11(const base::FilePath& xauth_path);

  // Checks if an ASAN or deep-memory-profiler build was requested, doing
  // appropriate initialization and returning true if so. Called by
  // InitChromium().
  bool SetUpASAN();
  bool SetUpDeepMemoryProfiler();

  // Reads .info files in |pepper_plugins_path_| and adds the appropriate
  // arguments to |arguments_|. Called by InitChromium().
  void SetUpPepperPlugins();

  // Add UI- and compositing-related flags to |arguments_|.
  void AddUiFlags();

  // Path under which files are created when running in a test.
  base::FilePath base_path_for_testing_;

  // UID and GID of the user used to run the binary.
  uid_t uid_;
  gid_t gid_;

  // USE flags that were set when the system was built.
  std::set<std::string> use_flags_;

  // True if official Chrome OS hardware is being used.
  bool is_chrome_os_hardware_;

  // True if this is a developer system, per the is_developer_end_user command.
  bool is_developer_end_user_;

  // Environment variables that the caller should export before starting the
  // executable.
  StringMap environment_variables_;

  // Command-line arguments that the caller should pass to the executable.
  StringVector arguments_;

  // Index in |arguments_| of the --vmodule flag. -1 if the flag hasn't been
  // set.
  int vmodule_argument_index_;

  DISALLOW_COPY_AND_ASSIGN(ChromiumCommandBuilder);
};

}  // namespace ui
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_UI_CHROMIUM_COMMAND_BUILDER_H_
