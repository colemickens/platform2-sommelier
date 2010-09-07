// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/system_utils.h"

#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <limits>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/file_util.h>
#include <base/logging.h>
#include "base/scoped_temp_dir.h"
#include <base/time.h>

namespace login_manager {

SystemUtils::SystemUtils() {}
SystemUtils::~SystemUtils() {}

int SystemUtils::kill(pid_t pid, int signal) {
  LOG(INFO) << "Sending " << signal << " to " << pid;
  return ::kill(pid, signal);
}

bool SystemUtils::ChildIsGone(pid_t child_spec, int timeout) {
  base::Time start = base::Time::Now();
  base::TimeDelta max_elapsed = base::TimeDelta::FromSeconds(timeout);
  base::TimeDelta elapsed;
  int ret;

  alarm(timeout);
  do {
    errno = 0;
    ret = ::waitpid(child_spec, NULL, 0);
    elapsed = base::Time::Now() - start;
  } while (ret > 0 || (errno == EINTR && elapsed < max_elapsed));

  // Once we exit the loop, we know there was an error.
  alarm(0);
  return errno == ECHILD;  // EINTR means we timed out.
}

bool SystemUtils::EnsureAndReturnSafeFileSize(const FilePath& file,
                                              int32* file_size_32) {
  // Get the file size (must fit in a 32 bit int for NSS).
  int64 file_size;
  if (!file_util::GetFileSize(file, &file_size)) {
    LOG(ERROR) << "Could not get size of " << file.value();
    return false;
  }
  if (file_size > static_cast<int64>(std::numeric_limits<int>::max())) {
    LOG(ERROR) << file.value() << "is "
               << file_size << "bytes!!!  Too big!";
    return false;
  }
  *file_size_32 = static_cast<int32>(file_size);
  return true;
}

bool SystemUtils::EnsureAndReturnSafeSize(int64 size_64, int32* size_32) {
  if (size_64 > static_cast<int64>(std::numeric_limits<int>::max()))
    return false;
  *size_32 = static_cast<int32>(size_64);
  return true;
}

bool SystemUtils::AtomicFileWrite(const FilePath& filename,
                                  const char* data,
                                  int size) {
  FilePath scratch_file;
  if (!file_util::CreateTemporaryFileInDir(filename.DirName(), &scratch_file))
    return false;
  if (file_util::WriteFile(scratch_file, data, size) != size)
    return false;

  return (file_util::ReplaceFile(scratch_file, filename) &&
          chmod(filename.value().c_str(), (S_IRUSR | S_IWUSR | S_IROTH)) == 0);
}

}  // namespace login_manager
