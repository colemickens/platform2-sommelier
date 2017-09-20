// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCONTAINER_LIBCONTAINER_UTIL_H_
#define LIBCONTAINER_LIBCONTAINER_UTIL_H_

#include <string>
#include <vector>

#include <base/callback_forward.h>
#include <base/files/file_path.h>
#include <base/logging.h>
#include <base/macros.h>
#include <libminijail.h>

#include "libcontainer/config.h"
#include "libcontainer/libcontainer.h"

namespace libcontainer {

// Simple class that saves errno.
class SaveErrno {
 public:
  SaveErrno();
  ~SaveErrno();

 private:
  const int saved_errno_;

  DISALLOW_COPY_AND_ASSIGN(SaveErrno);
};

// The comma operator will discard the SaveErrno instance, but will keep it
// alive until after the whole expression has been evaluated.
#define PLOG_PRESERVE(verbose_level) \
  ::libcontainer::SaveErrno(), PLOG(verbose_level)

// WaitablePipe provides a way for one process to wait on another. This only
// uses the read(2) and close(2) syscalls, so it can work even in a restrictive
// environment. Each process must call only one of Wait() and Signal() exactly
// once.
struct WaitablePipe {
  WaitablePipe();
  ~WaitablePipe();

  WaitablePipe(WaitablePipe&&);

  // Waits for Signal() to be called.
  void Wait();

  // Notifies the process that called Wait() to continue running.
  void Signal();

  int pipe_fds[2];

 private:
  DISALLOW_COPY_AND_ASSIGN(WaitablePipe);
};

// HookState holds two WaitablePipes so that the container can wait for its
// parent to run prestart hooks just prior to calling execve(2).
class HookState {
 public:
  HookState();
  ~HookState();

  HookState(HookState&& state);

  // Initializes this HookState so that WaitForHookAndRun() can be invoked and
  // waited upon when |j| reaches |event|. Returns true on success.
  bool InstallHook(minijail* j, minijail_hook_event_t event);

  // Waits for the event specified in InstallHook() and invokes |callbacks| in
  // the caller process. Returns true if all callbacks succeeded.
  bool WaitForHookAndRun(const std::vector<HookCallback>& callbacks,
                         pid_t container_pid);

 private:
  // A function that can be passed to minijail_add_hook() that blocks the
  // process in the container until the parent has finished running whatever
  // operations are needed outside the container. This is not expected to be
  // called directly.
  static int WaitHook(void* payload);

  bool installed_ = false;
  WaitablePipe reached_pipe_;
  WaitablePipe ready_pipe_;

  DISALLOW_COPY_AND_ASSIGN(HookState);
};

// Given a uid/gid map of "inside1 outside1 length1, ...", and an id inside of
// the user namespace, return the equivalent outside id, or return < 0 on error.
int GetUsernsOutsideId(const std::string& map, int id);

int MakeDir(const base::FilePath& path, int uid, int gid, int mode);

int TouchFile(const base::FilePath& path, int uid, int gid, int mode);

// Find a free loop device and attach it.
int LoopdevSetup(const base::FilePath& source,
                 base::FilePath* loopdev_path_out);

// Detach the specified loop device.
int LoopdevDetach(const base::FilePath& loopdev);

// Create a new device mapper target for the source.
int DeviceMapperSetup(const base::FilePath& source,
                      const std::string& verity_cmdline,
                      base::FilePath* dm_path_out,
                      std::string* dm_name_out);

// Tear down the device mapper target.
int DeviceMapperDetach(const std::string& dm_name);

// Match mount_one in minijail, mount one mountpoint with
// consideration for combination of MS_BIND/MS_RDONLY flag.
int MountExternal(const std::string& src,
                  const std::string& dest,
                  const std::string& type,
                  unsigned long flags,
                  const std::string& data);

// Wraps a callback to be run in a subset of the container's namespaces.
HookCallback AdaptCallbackToRunInNamespaces(HookCallback callback,
                                            std::vector<int> nstypes);

}  // namespace libcontainer

#endif  // LIBCONTAINER_LIBCONTAINER_UTIL_H_
