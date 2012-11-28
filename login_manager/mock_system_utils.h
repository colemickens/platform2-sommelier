// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include "login_manager/system_utils.h"

#include <unistd.h>

#include <base/file_path.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/scoped_vector.h>
#include <dbus/dbus.h>
#include <gmock/gmock.h>

namespace login_manager {
namespace gobject {
struct SessionManager;
}

class ScopedDBusPendingCall;

class MockSystemUtils : public SystemUtils {
 public:
  MockSystemUtils();
  virtual ~MockSystemUtils();
  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*)); // NOLINT
  MOCK_METHOD0(fork, pid_t(void));
  MOCK_METHOD0(IsDevMode, int(void));
  MOCK_METHOD1(Exists, bool(const FilePath&));
  MOCK_METHOD3(AtomicFileWrite, bool(const FilePath&, const char*, int));
  MOCK_METHOD2(ChildIsGone, bool(pid_t child_spec, int timeout));
  MOCK_METHOD2(EnsureAndReturnSafeFileSize,
               bool(const FilePath& file, int32* file_size_32));
  MOCK_METHOD2(EnsureAndReturnSafeSize,
               bool(int64 size_64, int32* size_32));
  MOCK_METHOD4(BroadcastSignal, void(gobject::SessionManager*,
                                     guint,
                                     const char*,
                                     const char*));
  MOCK_METHOD2(SendSignalToChromium, void(const char*, const char*));
  MOCK_METHOD2(SendStatusSignalToChromium, void(const char*, bool));
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
  ScopedVector<ScopedDBusPendingCall> fake_calls_;
  DISALLOW_COPY_AND_ASSIGN(MockSystemUtils);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
