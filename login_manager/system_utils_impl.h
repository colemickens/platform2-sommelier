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
#include <dbus/dbus-glib.h>
#include <glib.h>

namespace base {
class FilePath;
}

struct DBusPendingCall;

namespace login_manager {

class ScopedDBusPendingCall;

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

  virtual void CallMethodOnPowerManager(const char* method_name) OVERRIDE;
  virtual scoped_ptr<ScopedDBusPendingCall> CallAsyncMethodOnChromium(
      const char* method_name) OVERRIDE;
  virtual bool CheckAsyncMethodSuccess(DBusPendingCall* pending_call) OVERRIDE;
  virtual void CancelAsyncMethodCall(DBusPendingCall* pending_call) OVERRIDE;

  virtual void AppendToClobberLog(const char* msg) const OVERRIDE;
  virtual void SetAndSendGError(ChromeOSLoginError code,
                                DBusGMethodInvocation* context,
                                const char* message) OVERRIDE;
 private:
  // If this file exists on the next boot, the stateful partition will be wiped.
  static const char kResetFile[];

  // Call |interface|.|method_name| on object |path| provided by service
  // |destination| with no arguments. Blocks until the called method returns.
  static void CallMethodOn(const char* destination,
                           const char* path,
                           const char* interface,
                           const char* method_name);

  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(SystemUtilsImpl);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_SYSTEM_UTILS_IMPL_H_
