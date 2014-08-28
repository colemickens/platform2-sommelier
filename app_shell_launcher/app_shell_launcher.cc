// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <grp.h>
#include <sys/resource.h>
#include <sys/types.h>

#include <base/file_util.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <chromeos/ui/chromium_command_builder.h>
#include <chromeos/ui/util.h>
#include <chromeos/ui/x_server_runner.h>

using chromeos::ui::ChromiumCommandBuilder;
using chromeos::ui::XServerRunner;
using chromeos::ui::util::EnsureDirectoryExists;
using chromeos::ui::util::SetPermissions;

namespace {

// Authority file used for running the X server.
const char kXauthPath[] = "/var/run/x11.auth";

// Path to the app_shell binary.
const char kAppShellPath[] = "/opt/google/chrome/app_shell";

// Directory where data files are written at build time.
const char kReadonlyDataPath[] = "/usr/share/app_shell";

// Subdirectory under $DATA_DIR where user data should be stored.
const char kUserSubdir[] = "user";

// Files in $DATA_DIR or kReadonlyDataPath containing the ID of the app that
// should be loaded and the name of a preferred network to connect to.
const char kAppIdFile[] = "app_id";
const char kPreferredNetworkFile[] = "preferred_network";

// Subdirectory under $DATA_DIR or kReadonlyDataPath from which an app is loaded
// if kAppIdFile doesn't exist.
const char kAppSubdir[] = "app";

// Returns the first of the following paths that exists:
// - a file named |filename| within |stateful_dir|
// - a file named |filename| within |readonly_dir|
// - failing that, an empty path
base::FilePath GetDataPath(const base::FilePath& stateful_dir,
                           const base::FilePath& readonly_dir,
                           const std::string& filename) {
  base::FilePath stateful_path = stateful_dir.Append(filename);
  if (base::PathExists(stateful_path))
    return stateful_path;

  base::FilePath readonly_path = readonly_dir.Append(filename);
  if (base::PathExists(readonly_path))
    return readonly_path;

  return base::FilePath();
}

// Calls GetDataPath() and reads the file to |data_out|.
// Returns true on success.
bool ReadData(const base::FilePath& stateful_dir,
              const base::FilePath& readonly_dir,
              const std::string& filename,
              std::string* data_out) {
  base::FilePath path = GetDataPath(stateful_dir, readonly_dir, filename);
  return !path.empty() ? base::ReadFileToString(path, data_out) : false;
}

// Adds app_shell-specific flags.
void AddAppShellFlags(ChromiumCommandBuilder* builder) {
  const base::FilePath stateful_dir(builder->ReadEnvVar("DATA_DIR"));
  const base::FilePath readonly_dir(kReadonlyDataPath);

  // Set --data-path to tell app_shell where to store user data.
  const base::FilePath user_path = stateful_dir.Append(kUserSubdir);
  CHECK(EnsureDirectoryExists(user_path, builder->uid(), builder->gid(), 0700));
  builder->AddArg("--data-path=" + user_path.value());

  std::string app_id;
  if (ReadData(stateful_dir, readonly_dir, kAppIdFile, &app_id)) {
    base::TrimWhitespaceASCII(app_id, base::TRIM_ALL, &app_id);
    builder->AddArg("--app-shell-app-id=" + app_id);
  } else {
    const base::FilePath app_path =
        GetDataPath(stateful_dir, readonly_dir, kAppSubdir);
    if (!app_path.empty())
      builder->AddArg("--app-shell-app-path=" + app_path.value());
  }

  std::string network;
  if (ReadData(stateful_dir, readonly_dir, kPreferredNetworkFile, &network)) {
    base::TrimWhitespaceASCII(network, base::TRIM_ALL, &network);
    builder->AddArg("--app-shell-preferred-network=" + network);
  }
}

// Replaces the currently-running process with app_shell.
void ExecAppShell(const ChromiumCommandBuilder& builder) {
  if (setpriority(PRIO_PROCESS, 0, -20) != 0)
    PLOG(WARNING) << "setpriority() failed";

  PCHECK(initgroups(ChromiumCommandBuilder::kUser, builder.gid()) == 0);
  PCHECK(setgid(builder.gid()) == 0);
  PCHECK(setuid(builder.uid()) == 0);

  PCHECK(clearenv() == 0);
  for (ChromiumCommandBuilder::StringMap::const_iterator it =
           builder.environment_variables().begin();
       it != builder.environment_variables().end(); ++it) {
    PCHECK(setenv(it->first.c_str(), it->second.c_str(), 1) == 0);
  }

  const size_t kMaxArgs = 64;
  CHECK_LE(builder.arguments().size(), kMaxArgs);

  // One extra pointer for the initial command and one for the terminating NULL.
  char* argv[kMaxArgs + 2];
  for (size_t i = 0; i < arraysize(argv); ++i)
    argv[i] = NULL;

  argv[0] = const_cast<char*>(kAppShellPath);
  for (size_t i = 0; i < builder.arguments().size(); ++i)
    argv[i + 1] = const_cast<char*>(builder.arguments()[i].c_str());

  PCHECK(execv(argv[0], argv) == 0);
}

}  // namespace

int main(int argc, char** argv) {
  ChromiumCommandBuilder builder;
  CHECK(builder.Init());

  // Start the X server in the background before doing more-expensive setup.
  scoped_ptr<XServerRunner> x_runner;
  const base::FilePath xauth_path(kXauthPath);
  const bool using_x11 = builder.UseFlagIsSet("X");
  if (using_x11) {
    x_runner.reset(new XServerRunner);
    CHECK(x_runner->StartServer(
        XServerRunner::kDefaultUser, XServerRunner::kDefaultVt,
        builder.is_developer_end_user(), xauth_path));
  }

  builder.SetUpChromium(using_x11 ? xauth_path : base::FilePath());
  builder.EnableCoreDumps();
  AddAppShellFlags(&builder);

  if (using_x11)
    CHECK(x_runner->WaitForServer());

  // Do not add setup code below this point. Potentially-expensive work should
  // be done between StartServer() and WaitForServer().

  // This call never returns.
  ExecAppShell(builder);
  return 1;
}
