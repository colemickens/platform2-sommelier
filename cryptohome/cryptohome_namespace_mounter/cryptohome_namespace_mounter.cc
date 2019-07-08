// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file gets compiled into the 'cryptohome-namespace-mounter' executable.
// This executable performs an ephemeral mount (for Guest sessions) on behalf of
// cryptohome.
// Eventually, this executable will perform all cryptohome mounts.
// The lifetime of this executable's process matches the lifetime of the mount:
// it's launched by cryptohome when a Guest session is requested, and it's
// killed by cryptohome when the Guest session exits.

#include <sys/types.h>
#include <sysexits.h>

#include <iostream>
#include <memory>

#include <base/at_exit.h>
#include <base/callback.h>
#include <base/callback_helpers.h>
#include <base/command_line.h>
#include <base/files/file_path.h>
#include <base/files/file_util.h>
#include <base/logging.h>
#include <brillo/asynchronous_signal_handler.h>
#include <brillo/cryptohome.h>
#include <brillo/message_loops/base_message_loop.h>
#include <brillo/secure_blob.h>
#include <brillo/syslog_logging.h>

#include "cryptohome/cryptohome_common.h"
#include "cryptohome/mount_constants.h"
#include "cryptohome/mount_helper.h"
#include "cryptohome/mount_stack.h"

using base::FilePath;

namespace {
constexpr char kUserSwitch[] = "username";

constexpr size_t kSaltLength = CRYPTOHOME_DEFAULT_SALT_LENGTH;

void TearDown(cryptohome::MountHelper* mounter) {
  mounter->TearDownEphemeralMount();
}

void TearDownFromPoke(cryptohome::MountHelper* mounter) {
  VLOG(1) << "Got poke";
  TearDown(mounter);
}

bool TearDownFromSignal(cryptohome::MountHelper* mounter,
                        const struct signalfd_siginfo&) {
  VLOG(1) << "Got signal";
  TearDown(mounter);
  return true;  // unregister the handler
}

}  // namespace

int main(int argc, char** argv) {
  brillo::BaseMessageLoop message_loop;
  message_loop.SetAsCurrent();

  brillo::AsynchronousSignalHandler sig_handler;
  sig_handler.Init();

  base::CommandLine::Init(argc, argv);
  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  brillo::InitLog(brillo::kLogToSyslog);

  std::string username = cl->GetSwitchValueASCII(kUserSwitch);
  if (username == "") {
    LOG(ERROR) << "Username not provided";
    return EX_USAGE;
  }
  VLOG(1) << "username is " << username;

  constexpr uid_t uid = 1000;  // UID for 'chronos'.
  constexpr gid_t gid = 1000;  // GID for 'chronos'.
  constexpr gid_t access_gid = 1001;  // GID for 'chronos-access'.

  // Avoid passing the system salt in the command-line where it is visible in
  // 'ps' output and in /proc, so read it from stdin.
  // While the system salt is considered device-public, it should ideally not
  // be exfiltrated over the network, and avoiding it in the command-line helps
  // with that.
  // The username above is passed on the command-line because this code is
  // currently only used to mount Guest sessions, where the username is
  // hardcoded and not secret.
  // TODO(jorgelo, crbug.com/985492): Consider passing username/salt as a
  // protobuf once this code is used for non-Guest ephemeral sessions and
  // regular user sessions.
  brillo::SecureBlob system_salt(kSaltLength);
  if (!base::ReadFromFD(STDIN_FILENO, system_salt.char_data(), kSaltLength)) {
    PLOG(ERROR) << "Failed to read system salt";
    return EX_NOINPUT;
  }

  cryptohome::Platform platform;
  // TODO(jorgelo, crbug.com/985492): Pass |legacy_mount| in from cryptohome
  // instead of hardcoding it here.
  cryptohome::MountHelper mounter(
      uid, gid, access_gid, FilePath(cryptohome::kDefaultShadowRoot),
      FilePath(cryptohome::kDefaultSkeletonSource), system_salt,
      true /* legacy_mount */, &platform);

  // If PerformEphemeralMount fails, or reporting back to cryptohome fails,
  // attempt to clean up.
  base::ScopedClosureRunner tear_down_runner(
      base::Bind(&TearDown, base::Unretained(&mounter)));

  if (!mounter.PerformEphemeralMount(username)) {
    LOG(ERROR) << "PerformEphemeralMount failed";
    return EX_SOFTWARE;
  }
  VLOG(1) << "PerformEphemeralMount succeeded";

  constexpr char outdata = '0';
  if (!base::WriteFileDescriptor(STDOUT_FILENO, &outdata, sizeof(outdata))) {
    PLOG(ERROR) << "Failed to write to stdout";
    return EX_OSERR;
  }
  VLOG(1) << "Sent ack";

  // Mount and ack succeeded, release the closure without running it.
  ignore_result(tear_down_runner.Release());

  // Clean up mounts if we get signalled.
  sig_handler.RegisterHandler(
      SIGTERM, base::Bind(&TearDownFromSignal, base::Unretained(&mounter)));

  // Wait for poke from cryptohome, then clean up mounts.
  message_loop.WatchFileDescriptor(
      FROM_HERE, STDIN_FILENO, brillo::MessageLoop::WatchMode::kWatchRead,
      false /* persistent */,
      base::Bind(&TearDownFromPoke, base::Unretained(&mounter)));

  message_loop.RunOnce(true /* may_block */);

  return EX_OK;
}
