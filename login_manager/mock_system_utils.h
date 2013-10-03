// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include "login_manager/system_utils.h"

#include <unistd.h>

#include <string>
#include <vector>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <base/time.h>
#include <dbus/dbus.h>
#include <gmock/gmock.h>

namespace login_manager {

class ScopedDBusPendingCall;

class MockSystemUtils : public SystemUtils {
 public:
  MockSystemUtils();
  virtual ~MockSystemUtils();
  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*)); // NOLINT
  MOCK_METHOD0(fork, pid_t(void));
  MOCK_METHOD0(IsDevMode, int(void));
  MOCK_METHOD2(ChildIsGone, bool(pid_t child_spec, base::TimeDelta timeout));

  // All filesystem-touching methods write to a ScopedTempDir that's owned by
  // this class.
  bool Exists(const FilePath& file) OVERRIDE;
  bool AtomicFileWrite(const FilePath& file,
                       const char* data,
                       int size) OVERRIDE;
  bool ReadFileToString(const FilePath& file, std::string* out);
  bool EnsureAndReturnSafeFileSize(const FilePath& file,
                                   int32* file_size_32) OVERRIDE;
  bool RemoveFile(const FilePath& file) OVERRIDE;

  bool GetUniqueFilenameInWriteOnlyTempDir(FilePath* temp_file_path) OVERRIDE;
  bool CreateReadOnlyFileInTempDir(FilePath* temp_file_path) OVERRIDE;
  // Get filename to be returned by the above. Returns full path to the file.
  // An empty path is returned on failure.
  base::FilePath GetUniqueFilename();

  MOCK_METHOD1(EmitSignal, void(const char*));
  MOCK_METHOD2(EmitSignalWithStringArgs, void(const char*,
                                              const std::vector<std::string>&));
  MOCK_METHOD2(EmitStatusSignal, void(const char*, bool));
  MOCK_METHOD1(CallMethodOnPowerManager, void(const char*));

  // gmock can't handle methods that return scoped_ptrs.
  // To simulate fake async calls, one can use
  // EnqueueFakePendingCall() below to add fake ScopedDBusPendingCalls
  // to a FIFO queue that will be used to service calls to this method.
  // If the queue becomes exhausted and this method is called again, test
  // failures will be added.
  scoped_ptr<ScopedDBusPendingCall> CallAsyncMethodOnChromium(
      const char* method_name) OVERRIDE;

  MOCK_METHOD1(CheckAsyncMethodSuccess, bool(DBusPendingCall*));

  MOCK_METHOD1(CancelAsyncMethodCall, void(DBusPendingCall*));
  MOCK_CONST_METHOD1(AppendToClobberLog, void(const char*));
  MOCK_METHOD3(SetAndSendGError, void(ChromeOSLoginError,
                                      DBusGMethodInvocation*,
                                      const char*));

  // Add |fake_call| to the FIFO queue of fake pending calls managed by this
  // mock class.  If this queue is not exhausted by the time of this class'
  // destruction, the associated test will fail.
  void EnqueueFakePendingCall(scoped_ptr<ScopedDBusPendingCall> fake_call);

 private:
  // Ensures that temp_dir_ (inherited from SystemUtils) exists.
  // Returns true if it already does, or if it can be created. Else, False.
  // This call is idempotent
  bool EnsureTempDir();

  // Returns the given path "chrooted" inside temp_dir_, so to speak.
  // Ex: /var/run/foo -> /tmp/.org.Chromium.whatever/var/run/foo
  base::FilePath PutInsideTempdir(const base::FilePath& path);

  // To fake out GetUniqueFilenameInWriteOnlyTempDir() and
  // CreateReadOnlyFileInTempDir(), we just generate a single "unique"
  // path inside the temp dir managed by this class, store it here,
  // and return it whenever asked.
  base::FilePath unique_file_path_;

  SystemUtils real_utils_;
  ScopedVector<ScopedDBusPendingCall> fake_calls_;
  DISALLOW_COPY_AND_ASSIGN(MockSystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
