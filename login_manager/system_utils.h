// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

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
#include <dbus/dbus-glib.h>
#include <glib.h>

namespace base {
class FilePath;
}

struct DBusPendingCall;

namespace login_manager {

class ScopedDBusPendingCall;

class SystemUtils {
 public:
  SystemUtils() {}
  virtual ~SystemUtils() {}

  // Write |data| to |fd|, retrying on EINTR.
  // Static because it needs to be used inside signal handlers.
  static void RetryingWrite(int fd, const char* data, size_t data_length);

  // Sends |signal| to |pid|, with uid and euid set to |owner|.
  // NOTE: Your saved UID is kept unchanged.  If you expect to drop and regain
  // root privs, MAKE SURE YOUR suid == 0.
  virtual int kill(pid_t pid, uid_t owner, int signal) = 0;

  // Returns time, in seconds, since the unix epoch.
  virtual time_t time(time_t* t) = 0;

  // Forks a new process.  In the parent, returns child's pid.  In child, 0.
  virtual pid_t fork() = 0;

  // Returns 0 if normal mode, 1 if developer mode, -1 if error.
  virtual int IsDevMode() = 0;

  // Returns: true if child specified by |child_spec| exited,
  //          false if we time out.
  virtual bool ChildIsGone(pid_t child_spec, base::TimeDelta timeout) = 0;

  virtual bool EnsureAndReturnSafeFileSize(const base::FilePath& file,
                                           int32* file_size_32) = 0;

  // Returns whether a file exists.
  virtual bool Exists(const base::FilePath& file) = 0;

  // Creates a uniquely-named read-only file under |dir|.
  // Upon success, sets |temp_file| and returns true. Upon failure, |temp_file|
  // remains untouched.
  virtual bool CreateReadOnlyFileInTempDir(base::FilePath* temp_file) = 0;

  // Generates a guaranteed-unique filename in a write-only temp dir.
  // Returns false upon failure.
  virtual bool GetUniqueFilenameInWriteOnlyTempDir(
      base::FilePath* temp_file_path) = 0;

  // Removes a file.
  virtual bool RemoveFile(const base::FilePath& filename) = 0;

  // Atomically writes the given buffer into the file, overwriting any
  // data that was previously there.  Returns true upon success, false
  // otherwise.
  virtual bool AtomicFileWrite(const base::FilePath& filename,
                               const char* data,
                               int size) = 0;

  // Calls |method_name| on power manager.
  virtual void CallMethodOnPowerManager(const char* method_name) = 0;

  // Initiates an async call of |method_name| on Chromium.  Returns opaque
  // pointer that can be used to retrieve the response at a later time.
  virtual scoped_ptr<ScopedDBusPendingCall> CallAsyncMethodOnChromium(
      const char* method_name) = 0;

  // Expects |pending_call| to have succeeded by now.  Returns true if
  // so, false if not, and ensures that |pending_call| is never going
  // to return after now.  If |pending_call| returned an error, it is
  // logged before returning.
  //
  // |pending_call| will no longer be usable; the caller should destroy.
  virtual bool CheckAsyncMethodSuccess(DBusPendingCall* pending_call) = 0;

  // |pending_call| will no longer be usable; the caller should destroy.
  virtual void CancelAsyncMethodCall(DBusPendingCall* pending_call) = 0;

  // Makes a best-effort attempt to append |msg| to the system log that is
  // persisted across stateful partition wipes.
  virtual void AppendToClobberLog(const char* msg) const = 0;

  // Initializes |error| with |code| and |message|; returns it via |context|.
  virtual void SetAndSendGError(ChromeOSLoginError code,
                                DBusGMethodInvocation* context,
                                const char* message) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
