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
  MOCK_METHOD2(ImportValidateAndStoreGeneratedKey, void(const std::string&,
                                                        const base::FilePath&));
  MOCK_METHOD0(ScreenIsLocked, bool());
  MOCK_METHOD0(Initialize, bool());
  MOCK_METHOD0(Finalize, void());
  MOCK_METHOD2(EmitLoginPromptReady, gboolean(gboolean*, GError**));
  MOCK_METHOD1(EmitLoginPromptVisible, gboolean(GError**));
  MOCK_METHOD4(EnableChromeTesting, gboolean(gboolean,
                                             const gchar**,
                                             gchar**,
                                             GError**));
  MOCK_METHOD4(StartSession,
               gboolean(gchar*, gchar*, gboolean*, GError**));
  MOCK_METHOD3(StopSession, gboolean(gchar*, gboolean*, GError**));
  MOCK_METHOD2(StorePolicy, gboolean(GArray*, DBusGMethodInvocation*));
  MOCK_METHOD2(RetrievePolicy, gboolean(GArray**, GError**));
  MOCK_METHOD3(StorePolicyForUser,
               gboolean(gchar*, GArray*, DBusGMethodInvocation*));
  MOCK_METHOD3(RetrievePolicyForUser, gboolean(gchar*, GArray**, GError**));
  MOCK_METHOD3(StoreDeviceLocalAccountPolicy,
               gboolean(gchar*, GArray*, DBusGMethodInvocation*));
  MOCK_METHOD3(RetrieveDeviceLocalAccountPolicy,
               gboolean(gchar*, GArray**, GError**));
  MOCK_METHOD1(RetrieveSessionState, gboolean(gchar**));
  MOCK_METHOD0(RetrieveActiveSessions, GHashTable*(void));
  MOCK_METHOD1(LockScreen, gboolean(GError**));
  MOCK_METHOD1(HandleLockScreenShown, gboolean(GError**));

  MOCK_METHOD1(HandleLockScreenDismissed, gboolean(GError**));

  MOCK_METHOD4(RestartJob,
               gboolean(gint, gchar*, gboolean*, GError**));
  MOCK_METHOD5(RestartJobWithAuth,
               gboolean(gint, gchar*, gchar*, gboolean*, GError**));
  MOCK_METHOD2(StartDeviceWipe, gboolean(gboolean*, GError**));
  MOCK_METHOD3(SetFlagsForUser,
               gboolean(gchar*, const gchar**, GError**));
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_MOCK_SESSION_MANAGER_H_
