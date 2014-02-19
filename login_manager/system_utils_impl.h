// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_IMPL_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_IMPL_H_

#include "login_manager/system_utils.h"

#include <time.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/files/scoped_temp_dir.h>
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>
#include <base/time.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>

namespace base {
class FilePath;
}

namespace login_manager {

class SystemUtilsImpl : public SystemUtils {
 public:
  SystemUtilsImpl();
  virtual ~SystemUtilsImpl();

  virtual int kill(pid_t pid, uid_t owner, int signal) OVERRIDE;
  virtual time_t time(time_t* t) OVERRIDE;
  virtual pid_t fork() OVERRIDE;
  virtual int IsDevMode() OVERRIDE;
  virtual bool ChildIsGone(pid_t child_spec, base::TimeDelta timeout) OVERRIDE;

  virtual bool EnsureAndReturnSafeFileSize(const base::FilePath& file,
                                           int32* file_size_32) OVERRIDE;
  virtual bool Exists(const base::FilePath& file) OVERRIDE;
  virtual bool CreateReadOnlyFileInTempDir(base::FilePath* temp_file) OVERRIDE;
  virtual bool GetUniqueFilenameInWriteOnlyTempDir(
      base::FilePath* temp_file_path) OVERRIDE;
  virtual bool RemoveFile(const base::FilePath& filename) OVERRIDE;
  virtual bool AtomicFileWrite(const base::FilePath& filename,
                               const char* data,
                               int size) OVERRIDE;
  virtual void AppendToClobberLog(const char* msg) const OVERRIDE;

 private:
  // If this file exists on the next boot, the stateful partition will be wiped.
  static const char kResetFile[];

  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(SystemUtilsImpl);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_IMPL_H_
