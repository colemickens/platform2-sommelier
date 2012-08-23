// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_SYSTEM_UTILS_H_

#include <time.h>
#include <unistd.h>

#include <string>

#include <base/basictypes.h>
#include <base/stringprintf.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>
#include <glib.h>

class FilePath;

namespace login_manager {
namespace gobject {
struct SessionManager;
}

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

  // Removes a file.
  virtual bool RemoveFile(const FilePath& filename);

  // Atomically writes the given buffer into the file, overwriting any
  // data that was previously there.  Returns the number of bytes
  // written, or -1 on error.
  virtual bool AtomicFileWrite(const FilePath& filename,
                               const char* data,
                               int size);

  // Broadcasts |signal| over DBus, originating from |origin|, adding
  // |payload| and |user| as args.  It is an error to pass NULL for
  // |payload|, |origin| or |user|.
  virtual void BroadcastSignal(gobject::SessionManager* origin,
                               guint signal,
                               const char* payload,
                               const char* user);

  // Broadcasts |signal| over DBus, originating from |origin|.  It is
  // an error to pass NULL for |origin|.
  virtual void BroadcastSignalNoArgs(gobject::SessionManager* origin,
                                     guint signal);

  // TODO(cmasone): Move this to libchromeos as a part of factoring ownership
  //                API out of the session_manager.
  // http://code.google.com/p/chromium-os/issues/detail?id=5929
  //
  // Sends |signal_name| to Chromium browser, optionally adding |payload|
  // as an arg if it is not NULL.
  virtual void SendSignalToChromium(const char* signal_name,
                                    const char* payload);

  // Same as above, but accepts a boolean status that'll be encoded as
  // |kSignalSuccess| and |kSignalFailure| respectively.
  virtual void SendStatusSignalToChromium(const char* signal_name, bool status);

  // TODO(cmasone): Move this to libchromeos as a part of factoring ownership
  //                API out of the session_manager.
  // http://code.google.com/p/chromium-os/issues/detail?id=5929
  //
  // Sends |signal_name| to power manager.
  virtual void SendSignalToPowerManager(const char* signal_name);

  // Makes a best-effort attempt to append |msg| to the system log that is
  // persisted across stateful partition wipes.
  virtual void AppendToClobberLog(const char* msg) const;

  // Initializes |error| with |code| and |message|.
  virtual void SetGError(GError** error,
                         ChromeOSLoginError code,
                         const char* message);

  // Initializes |error| with |code| and |message|.
  virtual void SetAndSendGError(ChromeOSLoginError code,
                                DBusGMethodInvocation* context,
                                const char* message);

 private:
  // If this file exists on the next boot, the stateful partition will be wiped.
  static const char kResetFile[];

  // Strings for encoding boolean status in signals.
  static const char kSignalSuccess[];
  static const char kSignalFailure[];

  // Sends |signal_name| to |interface, optionally adding |payload|
  // as an arg if it is not NULL.
  static void SendSignalTo(const char* interface,
                           const char* signal_name,
                           const char* payload);

  DISALLOW_COPY_AND_ASSIGN(SystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_H_
