// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
#define LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_

#include "login_manager/system_utils.h"

#include <unistd.h>

#include <base/file_path.h>
#include <gmock/gmock.h>

namespace login_manager {
namespace gobject {
struct SessionManager;
}

class MockSystemUtils : public SystemUtils {
 public:
  MockSystemUtils();
  ~MockSystemUtils();
  MOCK_METHOD3(kill, int(pid_t pid, uid_t uid, int signal));
  MOCK_METHOD1(time, time_t(time_t*));
  MOCK_METHOD0(IsDevMode, int(void));
  MOCK_METHOD2(ChildIsGone, bool(pid_t child_spec, int timeout));
  MOCK_METHOD2(EnsureAndReturnSafeFileSize,
               bool(const FilePath& file, int32* file_size_32));
  MOCK_METHOD2(EnsureAndReturnSafeSize,
               bool(int64 size_64, int32* size_32));
  MOCK_METHOD0(TouchResetFile, bool(void));
  MOCK_METHOD4(BroadcastSignal, void(gobject::SessionManager*,
                                     guint,
                                     const char*,
                                     const char*));
  MOCK_METHOD2(SendSignalToChromium, void(const char*, const char*));
  MOCK_METHOD2(SendStatusSignalToChromium, void(const char*, bool));
  MOCK_METHOD1(SendSignalToPowerManager, void(const char*));
  MOCK_CONST_METHOD1(AppendToClobberLog, void(const char*));
  MOCK_METHOD3(SetAndSendGError, void(ChromeOSLoginError,
                                      DBusGMethodInvocation*,
                                      const char*));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SYSTEM_UTILS_H_
