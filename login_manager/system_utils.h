// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

#include <unistd.h>

#include <base/basictypes.h>

class FilePath;

namespace login_manager {
class SystemUtils {
 public:
  SystemUtils();
  virtual ~SystemUtils();

  // Sends |signal| to |pid|, with uid and euid set to |owner|.
  // NOTE: Your saved UID is kept unchanged.  If you expect to drop and regain
  // root privs, MAKE SURE YOUR suid == 0.
  virtual int kill(pid_t pid, uid_t owner, int signal);

  // Returns: true if child specified by |child_spec| exited,
  //          false if we time out.
  virtual bool ChildIsGone(pid_t child_spec, int timeout);

  virtual bool EnsureAndReturnSafeFileSize(const FilePath& file,
                                           int32* file_size_32);

  virtual bool EnsureAndReturnSafeSize(int64 size_64, int32* size_32);

  // Atomically writes the given buffer into the file, overwriting any
  // data that was previously there.  Returns the number of bytes
  // written, or -1 on error.
  virtual bool AtomicFileWrite(const FilePath& filename,
                               const char* data,
                               int size);

  virtual bool TouchResetFile();

  // TODO(cmasone): Move this to libchromeos as a part of factoring ownership
  //                API out of the session_manager.
  // http://code.google.com/p/chromium-os/issues/detail?id=5929
  //
  // Sends |signal_name| to Chromium browser, optionally adding |payload|
  // as an arg if it is not NULL.
  virtual void SendSignalToChromium(const char* signal_name,
                                    const char* payload);

  // TODO(cmasone): Move this to libchromeos as a part of factoring ownership
  //                API out of the session_manager.
  // http://code.google.com/p/chromium-os/issues/detail?id=5929
  //
  // Sends |signal_name| to power manager.
  virtual void SendSignalToPowerManager(const char* signal_name);

 private:
  // If this file exists on the next boot, the stateful partition will be wiped.
  static const char kResetFile[];

  // Sends |signal_name| to |interface, optionally adding |payload|
  // as an arg if it is not NULL.
  static void SendSignalTo(const char* interface,
                           const char* signal_name,
                           const char* payload);

  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
