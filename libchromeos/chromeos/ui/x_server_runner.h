// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_X_SERVER_RUNNER_H_
#define CHROMEOS_UI_X_SERVER_RUNNER_H_

#include <sys/types.h>

#include <string>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/macros.h>

namespace chromeos {
namespace ui {

// XServerRunner can be used to start the X server asynchronously and later
// block until the server is ready to accept connections from clients.
//
// In more detail:
//
// - StartServer() performs necessary setup and forks |child_pid_|.
// - |child_pid_| setuids to |user| and forks another process |x_pid|.
// - |x_pid| execs the X server.
// - The X server sends SIGUSR1 to |child_pid_| after initialization.
// - |child_pid_| exits, resulting in the original process receiving SIGCHLD.
// - WaitForServer() blocks until SIGCHLD has been received.
class XServerRunner {
 public:
  // Various hard-coded paths exposed here for tests.
  static const char kSocketDir[];
  static const char kIceDir[];
  static const char kLogFile[];
  static const char kXkbDir[];

  XServerRunner();
  ~XServerRunner();

  void set_base_path_for_testing(const base::FilePath& path) {
    base_path_for_testing_ = path;
  }
  void set_callback_for_testing(const base::Closure& callback) {
    callback_for_testing_ = callback;
  }

  // Creates necessary directories and starts the X server in the background
  // running as |user| on |vt|. |xauth_file| will be created to permit
  // connections to the server. Returns true if the setup was successful and the
  // child process that starts the server was forked sucessfully.
  bool StartServer(const std::string& user,
                   int vt,
                   bool allow_vt_switching,
                   const base::FilePath& xauth_file);

  // Blocks until the previously-started X server is ready to accept
  // connections. Handles announcing the server's readiness to Upstart and
  // recording a bootstat event.
  bool WaitForServer();

 private:
  // Converts absolute path |path| into a base::FilePath, rooting it under
  // |base_path_for_testing_| if it's non-empty.
  base::FilePath GetPath(const std::string& path) const;

  // Path under which files are created when running in a test.
  base::FilePath base_path_for_testing_;

  // If non-null, run instead of actually starting the X server.
  base::Closure callback_for_testing_;

  // PID of the child process that will exit once the X server is ready to
  // accept connections.
  pid_t child_pid_;

  DISALLOW_COPY_AND_ASSIGN(XServerRunner);
};

}  // namespace ui
}  // namespace chromeos

#endif  // CHROMEOS_UI_X_SERVER_RUNNER_H_
