// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_
#define LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_

#include "login_manager/session_manager_interface.h"

#include <base/file_path.h>
#include <gmock/gmock.h>

namespace login_manager {

class MockSessionManager : public SessionManagerInterface {
 public:
  MockSessionManager();
  virtual ~MockSessionManager();

  MOCK_METHOD0(AnnounceSessionStoppingIfNeeded, void());
  MOCK_METHOD0(AnnounceSessionStopped, void());
  MOCK_METHOD1(ImportValidateAndStoreGeneratedKey, void(const FilePath&));
  MOCK_METHOD0(ScreenIsLocked, bool());
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD0(Finalize, void());
  MOCK_METHOD1(ValidateAndStoreOwnerKey, void(const std::string&)) OVERRIDE;
  MOCK_METHOD2(EmitLoginPromptReady, gboolean(gboolean*, GError**)) OVERRIDE;
  MOCK_METHOD1(EmitLoginPromptVisible, gboolean(GError**)) OVERRIDE;
  MOCK_METHOD4(EnableChromeTesting, gboolean(gboolean,
                                             const gchar**,
                                             gchar**,
                                             GError**)) OVERRIDE;
  MOCK_METHOD4(StartSession,
               gboolean(gchar*, gchar*, gboolean*, GError**)) OVERRIDE;
  MOCK_METHOD3(StopSession, gboolean(gchar*, gboolean*, GError**)) OVERRIDE;
  MOCK_METHOD2(StorePolicy, gboolean(GArray*, DBusGMethodInvocation*)) OVERRIDE;
  MOCK_METHOD2(RetrievePolicy, gboolean(GArray**, GError**)) OVERRIDE;
  MOCK_METHOD2(StoreUserPolicy,
               gboolean(GArray*, DBusGMethodInvocation*)) OVERRIDE;
  MOCK_METHOD2(RetrieveUserPolicy, gboolean(GArray**, GError**)) OVERRIDE;
  MOCK_METHOD3(StoreDeviceLocalAccountPolicy,
               gboolean(gchar*, GArray*, DBusGMethodInvocation*)) OVERRIDE;
  MOCK_METHOD3(RetrieveDeviceLocalAccountPolicy,
               gboolean(gchar*, GArray**, GError**)) OVERRIDE;
  MOCK_METHOD2(RetrieveSessionState, gboolean(gchar**, gchar**)) OVERRIDE;
  MOCK_METHOD1(LockScreen, gboolean(GError**)) OVERRIDE;
  MOCK_METHOD1(HandleLockScreenShown, gboolean(GError**)) OVERRIDE;

  MOCK_METHOD1(UnlockScreen, gboolean(GError**)) OVERRIDE;
  MOCK_METHOD1(HandleLockScreenDismissed, gboolean(GError**)) OVERRIDE;

  MOCK_METHOD4(RestartJob,
               gboolean(gint, gchar*, gboolean*, GError**)) OVERRIDE;
  MOCK_METHOD5(RestartJobWithAuth,
               gboolean(gint, gchar*, gchar*, gboolean*, GError**)) OVERRIDE;
  MOCK_METHOD2(StartDeviceWipe, gboolean(gboolean*, GError**)) OVERRIDE;
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_
