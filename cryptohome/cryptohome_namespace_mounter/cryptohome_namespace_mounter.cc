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
#include "cryptohome/mount_utils.h"

#include "cryptohome/namespace_mounter_ipc.pb.h"

using base::FilePath;

namespace {
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

  brillo::InitLog(brillo::kLogToSyslog);

  constexpr uid_t uid = 1000;  // UID for 'chronos'.
  constexpr gid_t gid = 1000;  // GID for 'chronos'.
  constexpr gid_t access_gid = 1001;  // GID for 'chronos-access'.

  cryptohome::OutOfProcessMountRequest request;
  if (!cryptohome::ReadProtobuf(STDIN_FILENO, &request)) {
    LOG(ERROR) << "Failed to read request protobuf";
    return EX_NOINPUT;
  }

  brillo::SecureBlob system_salt(request.system_salt());

  cryptohome::Platform platform;
  cryptohome::MountHelper mounter(
      uid, gid, access_gid, FilePath(cryptohome::kDefaultShadowRoot),
      FilePath(cryptohome::kDefaultSkeletonSource), system_salt,
      request.legacy_home(), &platform);

  // If PerformEphemeralMount fails, or reporting back to cryptohome fails,
  // attempt to clean up.
  base::ScopedClosureRunner tear_down_runner(
      base::Bind(&TearDown, base::Unretained(&mounter)));

  if (!mounter.PerformEphemeralMount(request.username())) {
    LOG(ERROR) << "PerformEphemeralMount failed";
    return EX_SOFTWARE;
  }
  VLOG(1) << "PerformEphemeralMount succeeded";

  cryptohome::OutOfProcessMountResponse response;
  for (const auto& path : mounter.MountedPaths()) {
    response.add_paths(path.value());
  }

  if (!cryptohome::WriteProtobuf(STDOUT_FILENO, response)) {
    LOG(ERROR) << "Failed to write response protobuf";
    return EX_OSERR;
  }
  VLOG(1) << "Sent protobuf";

  // Mount and ack succeeded, release the closure without running it.
  ignore_result(tear_down_runner.Release());

  // Clean up mounts if we get signalled.
  sig_handler.RegisterHandler(
      SIGTERM, base::Bind(&TearDownFromSignal, base::Unretained(&mounter)));

  // Wait for poke from cryptohome, then clean up mounts.
  std::unique_ptr<base::FileDescriptorWatcher::Controller> watcher =
      base::FileDescriptorWatcher::WatchReadable(
          STDIN_FILENO,
          base::Bind(&TearDownFromPoke, base::Unretained(&mounter)));

  message_loop.RunOnce(true /* may_block */);

  return EX_OK;
}
