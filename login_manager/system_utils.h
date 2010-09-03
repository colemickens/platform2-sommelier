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

  virtual int kill(pid_t pid, int signal);

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

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
