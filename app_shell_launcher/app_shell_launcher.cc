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

// User, VT, and authority file used for running the X server.
const char kXorgUser[] = "xorg";
const int kXorgVt = 1;
const char kXauthFile[] = "/var/run/x11.auth";

// Path to the app_shell binary.
const char kAppShellPath[] = "/opt/google/chrome/app_shell";

// Subdirectory under $DATA_DIR where user data should be stored.
const char kUserSubdir[] = "user";

// File in $DATA_DIR containing the name of a preferred network to connect to.
const char kPreferredNetworkFile[] = ".preferred_network";

// Subdirectory under $DATA_DIR from which an app should be loaded.
const char kAppSubdir[] = "app";

// Adds app_shell-specific flags.
void AddAppShellFlags(ChromiumCommandBuilder* builder) {
  const base::FilePath data_dir(builder->ReadEnvVar("DATA_DIR"));

  // Set --data-path to tell app_shell where to store user data.
  const base::FilePath user_path = data_dir.Append(kUserSubdir);
  CHECK(EnsureDirectoryExists(user_path, builder->uid(), builder->gid(), 0700));
  builder->AddArg("--data-path=" + user_path.value());

  std::string preferred_network;
  if (base::ReadFileToString(data_dir.Append(kPreferredNetworkFile),
                             &preferred_network)) {
    base::TrimWhitespaceASCII(
        preferred_network, base::TRIM_ALL, &preferred_network);
    builder->AddArg("--app-shell-preferred-network=" + preferred_network);
  }

  // If an app is present on disk, tell app_shell to run it.
  // TODO(derat): Pass an app ID instead once app_shell supports downloading.
  const base::FilePath app_path = data_dir.Append(kAppSubdir);
  if (base::PathExists(app_path))
    builder->AddArg("--app-shell-app-path=" + app_path.value());
}

// Does X-related setup.
void ConfigureX11(ChromiumCommandBuilder* builder) {
  const base::FilePath user_xauth_file(
      base::FilePath(builder->ReadEnvVar("DATA_DIR")).Append(".Xauthority"));
  PCHECK(base::CopyFile(base::FilePath(kXauthFile), user_xauth_file))
      << "Unable to copy " << kXauthFile << " to " << user_xauth_file.value();
  CHECK(SetPermissions(
      user_xauth_file, builder->uid(), builder->gid(), 0600));
  builder->AddEnvVar("XAUTHORITY", user_xauth_file.value());
  builder->AddEnvVar("DISPLAY", ":0.0");
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
  const bool using_x11 = builder.UseFlagIsSet("X");
  if (using_x11) {
    x_runner.reset(new XServerRunner);
    CHECK(x_runner->StartServer(
        kXorgUser, kXorgVt, builder.is_developer_end_user(),
        base::FilePath(kXauthFile)));
  }

  builder.SetUpChromium();
  builder.EnableCoreDumps();
  AddAppShellFlags(&builder);

  if (using_x11) {
    ConfigureX11(&builder);
    CHECK(x_runner->WaitForServer());
  }

  // Do not add setup code below this point. Potentially-expensive work should
  // be done between StartServer() and WaitForServer().

  // This call never returns.
  ExecAppShell(builder);
  return 1;
}
