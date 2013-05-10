// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Chaps client PAM module. It loads the users token into the
// user-slot when they login.

#include <grp.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <security/pam_modules.h>

#include <string>

#include <base/file_util.h>
#include <base/file_path.h>
#include <base/logging.h>
#include <chromeos/secure_blob.h>
#include <chromeos/utility.h>

#include "chaps/chaps_utility.h"
#include "chaps/isolate.h"
#include "chaps/isolate_login_client.h"
#include "chaps/pam_helper.h"
#include "chaps/platform_globals.h"
#include "chaps/token_file_manager.h"
#include "chaps/token_manager_client.h"

using std::string;
using chromeos::SecureBlob;

#define PAM_EXPORT_SPEC EXPORT_SPEC PAM_EXTERN

namespace {

const char kLogoutOnCloseSessionEnvName[] = "CHAPS_LOGOUT_ON_CLOSE_SESSION";

// Isolate login client used to provide chaps login functionality.
chaps::IsolateLoginClient* g_login_client = NULL;

// Pam helper used to get data passed via the pam_handle.
chaps::PamHelper* g_pam_helper = NULL;

// Set to true when Init() is first called.
bool g_is_initialized = false;

}  // namespace

namespace chaps {

EXPORT_SPEC void EnableMock(IsolateLoginClient* login_client,
                            PamHelper* pam_helper) {
  CHECK(!g_is_initialized);
  g_login_client = login_client;
  g_pam_helper = pam_helper;
  g_is_initialized = true;
}

EXPORT_SPEC void DisableMock() {
  g_login_client = NULL;
  g_pam_helper = NULL;
  g_is_initialized = false;
}

}  // namespace chaps

static bool GetChapsdUser(uid_t* uid, gid_t* gid) {
  CHECK(uid);
  CHECK(gid);
  const struct passwd* chaps_pwd = getpwnam(chaps::kChapsdProcessUser);
  if (!chaps_pwd) {
    return false;
  }
  *uid = chaps_pwd->pw_uid;
  *gid = chaps_pwd->pw_gid;
  return true;
}

static bool Init() {
  if (!g_is_initialized) {
    uid_t chapsd_uid;
    gid_t chapsd_gid;
    if (!GetChapsdUser(&chapsd_uid, &chapsd_gid)) {
      LOG(ERROR) << "Failed to get chapsd user";
      return false;
    }
    g_login_client = new chaps::IsolateLoginClient(
        new chaps::IsolateCredentialManager(),
        new chaps::TokenFileManager(chapsd_uid, chapsd_gid),
        new chaps::TokenManagerClient());
    g_pam_helper = new chaps::PamHelper();
    g_is_initialized = true;
  }
  return true;
}

PAM_EXPORT_SPEC int pam_sm_authenticate(pam_handle_t* pam_handle, int flags,
                                        int argc, const char** argv) {
  if (!Init())
    return PAM_SERVICE_ERR;

  string user;
  if (!g_pam_helper->GetPamUser(pam_handle, &user))
    return PAM_SERVICE_ERR;

  SecureBlob password;
  if (!g_pam_helper->GetPamPassword(pam_handle, false, &password))
    return PAM_AUTH_ERR;

  if (!g_pam_helper->SaveUserAndPassword(pam_handle, user, password))
    return PAM_SERVICE_ERR;

  return PAM_SUCCESS;
}

PAM_EXPORT_SPEC int pam_sm_open_session(pam_handle_t* pam_handle, int flags,
                                        int argc, const char** argv) {
  if (!Init())
    return PAM_SERVICE_ERR;

  string user;
  if (!g_pam_helper->GetPamUser(pam_handle, &user))
    return PAM_SERVICE_ERR;

  string saved_user;
  SecureBlob saved_password;
  if (!g_pam_helper->RetrieveUserAndPassword(pam_handle, &saved_user,
                                             &saved_password)) {
    // This can happen if pam_sm_authenticate wasn't called in this pam session.
    return PAM_IGNORE;
  }

  if (user.compare(saved_user) != 0) {
    // User who authenticated is opening a session as a different user, this
    // can happen for example in when user sudo's.
    return PAM_IGNORE;
  }

  if (!g_login_client->LoginUser(saved_user, saved_password))
    return PAM_SERVICE_ERR;

  if (!g_pam_helper->PutEnvironmentVariable(
           pam_handle, kLogoutOnCloseSessionEnvName, "1"))
    return PAM_SERVICE_ERR;

  return PAM_SUCCESS;
}

PAM_EXPORT_SPEC int pam_sm_close_session(pam_handle_t* pam_handle, int flags,
                                         int argc, const char** argv) {
  if (!Init())
    return PAM_SERVICE_ERR;

  string close_session_env;
  if (!g_pam_helper->GetEnvironmentVariable(
           pam_handle, kLogoutOnCloseSessionEnvName, &close_session_env) ||
      close_session_env.compare("1") != 0) {
    // We never logged user in, so don't log them out here.
    return PAM_IGNORE;
  }

  string user;
  if (!g_pam_helper->GetPamUser(pam_handle, &user))
    return PAM_SERVICE_ERR;

  if (!g_login_client->LogoutUser(user))
    return PAM_SERVICE_ERR;

  return PAM_SUCCESS;
}

PAM_EXPORT_SPEC int pam_sm_chauthtok(pam_handle_t* pam_handle, int flags,
                                     int argc, const char** argv) {
  if (!Init())
    return PAM_SERVICE_ERR;

  if (flags & PAM_PRELIM_CHECK || !(flags & PAM_UPDATE_AUTHTOK))
    return PAM_IGNORE;

  string user;
  if (!g_pam_helper->GetPamUser(pam_handle, &user))
    return PAM_SERVICE_ERR;

  SecureBlob old_password;
  if (!g_pam_helper->GetPamPassword(pam_handle, true, &old_password))
    return PAM_AUTH_ERR;

  SecureBlob new_password;
  if (!g_pam_helper->GetPamPassword(pam_handle, false, &new_password))
    return PAM_AUTH_ERR;

  if (!g_login_client->ChangeUserAuth(user, old_password, new_password))
    return PAM_SERVICE_ERR;

  return PAM_SUCCESS;
}

PAM_EXPORT_SPEC int pam_sm_setcred(pam_handle_t* pam_handle, int flags,
                                   int argc, const char** argv) {
  return PAM_IGNORE;
}

PAM_EXPORT_SPEC int pam_sm_acct_mgmt(pam_handle_t* pam_handle, int flags,
                                     int argc, const char** argv) {
  return PAM_IGNORE;
}
