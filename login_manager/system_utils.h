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
#include <base/memory/scoped_ptr.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus-glib.h>
#include <glib.h>

class FilePath;
struct DBusPendingCall;

namespace login_manager {
namespace gobject {
struct SessionManager;
}

class ScopedDBusPendingCall;

class SystemUtils {
 public:
  SystemUtils();
  virtual ~SystemUtils();

  // Sends |signal| to |pid|, with uid and euid set to |owner|.
  // NOTE: Your saved UID is kept unchanged.  If you expect to drop and regain
  // root privs, MAKE SURE YOUR suid == 0.
  virtual int kill(pid_t pid, uid_t owner, int signal);

  // Returns time, in seconds, since the unix epoch.
  virtual time_t time(time_t* t);

  // Forks a new process.  In the parent, returns child's pid.  In child, 0.
  virtual pid_t fork();

  // Returns 0 if normal mode, 1 if developer mode, -1 if error.
  virtual int IsDevMode();

  // Returns: true if child specified by |child_spec| exited,
  //          false if we time out.
  virtual bool ChildIsGone(pid_t child_spec, int timeout);

  virtual bool EnsureAndReturnSafeFileSize(const FilePath& file,
                                           int32* file_size_32);

  virtual bool EnsureAndReturnSafeSize(int64 size_64, int32* size_32);

  // Returns whether a file exists.
  virtual bool Exists(const FilePath& file);

  // Generates a guaranteed-unique filename in a write-only temp dir.
  // Returns false upon failure.
  virtual bool GetUniqueFilenameInWriteOnlyTempDir(FilePath* temp_file_path);

  // Removes a file.
  virtual bool RemoveFile(const FilePath& filename);

  // Atomically writes the given buffer into the file, overwriting any
  // data that was previously there.  Returns the number of bytes
  // written, or -1 on error.
  virtual bool AtomicFileWrite(const FilePath& filename,
                               const char* data,
                               int size);

  // Broadcasts |signal_name| from the session manager DBus interface.
  virtual void EmitSignal(const char* signal_name);

  // Broadcasts |signal_name| from the session manager DBus interface,
  // optionally adding |payload| as args if it is not empty.
  virtual void EmitSignalWithStringArgs(
      const char* signal_name,
      const std::vector<std::string>& payload);

  // Same as above, but accepts a boolean status that'll be encoded as
  // |kSignalSuccess| and |kSignalFailure| respectively.
  virtual void EmitStatusSignal(const char* signal_name, bool status);

  // Calls |method_name| on power manager.
  virtual void CallMethodOnPowerManager(const char* method_name);

  // Initiates an async call of |method_name| on Chromium.  Returns opaque
  // pointer that can be used to retrieve the response at a later time.
  //
  // This can't return scoped_ptr<ScopedDBusPendingCall> because gmock can't
  // handle a scoped_ptr return type.
  virtual scoped_ptr<ScopedDBusPendingCall> CallAsyncMethodOnChromium(
      const char* method_name);

  // Expects |pending_call| to have succeeded by now.  Returns true if
  // so, false if not, and ensures that |pending_call| is never going
  // to return after now.  If |pending_call| returned an error, it is
  // logged before returning.
  //
  // |pending_call| will no longer be usable; the caller should destroy.
  virtual bool CheckAsyncMethodSuccess(DBusPendingCall* pending_call);

  // |pending_call| will no longer be usable; the caller should destroy.
  virtual void CancelAsyncMethodCall(DBusPendingCall* pending_call);

  // Makes a best-effort attempt to append |msg| to the system log that is
  // persisted across stateful partition wipes.
  virtual void AppendToClobberLog(const char* msg) const;

  // Initializes |error| with |code| and |message|; returns it via |context|.
  virtual void SetAndSendGError(ChromeOSLoginError code,
                                DBusGMethodInvocation* context,
                                const char* message);

 private:
  // If this file exists on the next boot, the stateful partition will be wiped.
  static const char kResetFile[];

  // Strings for encoding boolean status in signals.
  static const char kSignalSuccess[];
  static const char kSignalFailure[];

  // Emits |signal_name| from |interface|, optionally adding contents
  // of |payload| as args if it is not empty.
  static void EmitSignalFrom(const char* interface,
                             const char* signal_name,
                             const std::vector<std::string>& payload);

  // Call |interface|.|method_name| on object |path| provided by service
  // |destination| with no arguments. Blocks until the called method returns.
  static void CallMethodOn(const char* destination,
                           const char* path,
                           const char* interface,
                           const char* method_name);

  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
