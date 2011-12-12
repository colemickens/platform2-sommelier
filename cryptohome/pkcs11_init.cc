// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init.

#include "pkcs11_init.h"

#include <string.h>

#include <iostream>

#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/cryptohome.h>
#include <errno.h>
#include <glib.h>
#include <opencryptoki/pkcs11.h>


namespace cryptohome {

// static
const CK_SLOT_ID Pkcs11Init::kDefaultTpmSlotId = 0;
const CK_ULONG Pkcs11Init::kMaxPinLen = 9;
const CK_ULONG Pkcs11Init::kMaxLabelLen = 32;
// openCryptoki 2.2.4 PIN defaults as per the README at:
// http://opencryptoki.git.sourceforge.net/git/gitweb.cgi?p=opencryptoki
//       /opencryptoki;a=summary
const CK_CHAR Pkcs11Init::kDefaultOpencryptokiSoPin[] = "87654321";
const CK_CHAR Pkcs11Init::kDefaultOpencryptokiUserPin[] = "12345678";
const CK_CHAR Pkcs11Init::kDefaultSoPin[] = "000000";
const CK_CHAR Pkcs11Init::kDefaultUserPin[] = "111111";
const CK_CHAR Pkcs11Init::kDefaultLabel[] = "User-Specific TPM Token";
const char Pkcs11Init::kOpencryptokiDir[] = "/var/lib/opencryptoki";
const char Pkcs11Init::kUserDir[] = "/home/chronos/user";
const char Pkcs11Init::kUserTokenDir[] = "/home/chronos/user/.tpm";
const char Pkcs11Init::kPkcs11Group[] = "pkcs11";
const char Pkcs11Init::kTokenConfigFile[] =
    "/var/lib/opencryptoki/pk_config_data";
const char Pkcs11Init::kPkcsSlotdPath[] = "/usr/sbin/pkcsslotd";
const char Pkcs11Init::kPkcsSlotPath[] = "/usr/sbin/pkcs_slot";
const char Pkcs11Init::kChapsdProcessName[] = "chapsd";
const char Pkcs11Init::kPkcsslotdProcessName[] = "pkcsslotd";
const char* Pkcs11Init::kPkcsSlotCmd[] = {
  Pkcs11Init::kPkcsSlotPath,
  "0",
  "tpm"
};
const char* Pkcs11Init::kSymlinkSources[] = {
  "/var/lib/opencryptoki/tpm/chronos",
  "/var/lib/opencryptoki/tpm/ipsec",
  "/var/lib/opencryptoki/tpm/root",
  NULL
};
const char* Pkcs11Init::kSensitiveTokenFiles[] = {
  "PUBLIC_ROOT_KEY.pem",
  "PRIVATE_ROOT_KEY.pem",
  NULL
};

// This file in the user's cryptohome signifies that Pkcs11 initialization
// was completed for the user.
const std::string Pkcs11Init::kPkcs11InitializedFile =
    "/home/chronos/user/.tpm/.isinitialized";

extern const char* kTpmOwnedFile;
extern const char kDefaultSharedUser[];
extern const char kDefaultSharedAccessGroup[];

// Helper function to copy a CK_CHAR string |src| to another CK_CHAR string
// |dest| with at most most |len-1| characters. |dest| is then null-terminated.
void CKCharcpy(CK_CHAR* dest, const CK_CHAR* src, CK_ULONG len) {
  strncpy(reinterpret_cast<char*>(dest),
          reinterpret_cast<const char*>(src),
          len - 1);
  dest[len - 1] = '\0';
}

void Pkcs11Init::GetTpmTokenInfo(gchar **OUT_label,
                                 gchar **OUT_user_pin) {
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

void Pkcs11Init::GetTpmTokenInfoForUser(gchar *username,
                                        gchar **OUT_label,
                                        gchar **OUT_user_pin) {
  // TODO(ellyjones): make this work for real, perhaps? crosbug.com/22127
  *OUT_label = g_strdup(reinterpret_cast<const gchar *>(kDefaultLabel));
  *OUT_user_pin = g_strdup(reinterpret_cast<const gchar *>(kDefaultUserPin));
}

bool Pkcs11Init::InitializeOpencryptoki() {
  LOG(INFO) << "Initializing Opencryptoki subsystem.";
  // Determine required uid and gids.
  if (!platform_->GetGroupId(kPkcs11Group, &pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't get the group ID for group " << kPkcs11Group;
    return false;
  }
  if (!platform_->GetGroupId(kDefaultSharedAccessGroup, &access_group_id_)) {
    LOG(ERROR) << "Couldn't get the group ID for group "
               << kDefaultSharedAccessGroup;
    return false;
  }
  if (!platform_->GetUserId(kDefaultSharedUser, &chronos_user_id_,
                            &chronos_group_id_)) {
    LOG(ERROR) << "Couldn't get the user/group ID for user "
               << kDefaultSharedUser;
    return false;
  }

  // We need to get PKCS #11 daemons into an initial state.  This is necessary
  // for force_pkcs11_init.  Pkcsslotd will be restarted below; chapsd will be
  // immediately respawned by init.
  if (platform_->TerminatePidsByName(kChapsdProcessName, false))
    LOG(INFO) << "Terminated " << kChapsdProcessName;
  if (platform_->TerminatePidsByName(kPkcsslotdProcessName, false))
    LOG(INFO) << "Terminated " << kPkcsslotdProcessName;
  if (platform_->TerminatePidsByName(kPkcsslotdProcessName, true))
    LOG(INFO) << "Killed " << kPkcsslotdProcessName;

  if (!IsOpencryptokiDirectoryValid() &&
      !SetupOpencryptokiDirectory())
    return false;

  if(!ConfigureTPMAsToken())
    return false;

  // Slot daemon can only be started after the opencryptoki directory has
  // correctly been setup and the TPM is initialized to be used as the slot 0
  // token.
  if (!StartPkcs11Daemon())
    return false;
  // Wait for chapsd to respawn.  Normally, it is running by now so this should
  // be quick.
  std::vector<pid_t> pids;
  int waited = 0;
  const int max_wait = 1000;
  platform_->GetPidsByName(kChapsdProcessName, &pids);
  while (pids.size() == 0 && waited < max_wait) {
    usleep(100000);
    waited += 100;
    pids.clear();
    platform_->GetPidsByName(kChapsdProcessName, &pids);
  }

  is_opencryptoki_ready_ = true;
  return true;
}

bool Pkcs11Init::InitializeToken() {
  LOG(INFO) << "User token will be initialized.";
  if (!is_opencryptoki_ready_) {
    LOG(ERROR) << "Opencryptoki subsystem is not ready. Did you call "
               << "InitializeOpencryptoki?";
    return false;
  }

  if (!SetupUserTokenDirectory())
    return false;

  // PIN initialization needs to be performed as chronos:pkcs11.
  uid_t saved_uid;
  gid_t saved_gid;
  if (!platform_->SetProcessId(chronos_user_id_, pkcs11_group_id_, &saved_uid,
                               &saved_gid))
    return false;
  bool success = SetUserTokenPins() && SetUserTokenFilePermissions();
  // Although sensitive files should no longer be created, sanitize as a fail-
  // safe.
  if (!SanitizeTokenDirectory())
    success = false;
  if (!SetInitialized(success))
    success = false;
  if (!platform_->SetProcessId(saved_uid, saved_gid, NULL, NULL))
    return false;
  // This is needed to ensure that contents of the files created by
  // opencryptoki during initialization get flushed to the underlying storage
  // subsystem. Due to delayed writeback, a system crash at this point may
  // leave these files in a corrupted state. Opencryptoki does not handle
  // this quite that gracefully.
  platform_->Sync();
  if (success) {
    // The initialization process can leave chapsd in a security-officer state.
    // We want to start it in a fresh state for normal use.
    if (platform_->TerminatePidsByName(kChapsdProcessName, false))
      LOG(INFO) << "Terminated " << kChapsdProcessName;
  }
  return success;
}

bool Pkcs11Init::ConfigureTPMAsToken() {
  // Remove old token config file.
  if (platform_->FileExists(kTokenConfigFile) &&
      !platform_->DeleteFile(kTokenConfigFile, false))
    return false;

  // Setup the TPM as a token.
  process_->Reset(0);
  for(unsigned int i = 0; i < arraysize(kPkcsSlotCmd); i++)
    process_->AddArg(kPkcsSlotCmd[i]);
  process_->SetUid(chronos_user_id_);
  process_->SetGid(pkcs11_group_id_);
  if(process_->Run() != 0) {
    LOG(ERROR) << "Couldn't set the TPM as a token.";
    return false;
  }
  return true;
}

bool Pkcs11Init::StartPkcs11Daemon() {
  process_->Reset(0);
  process_->AddArg(kPkcsSlotdPath);
  // TODO(gauravsh): Figure out if pkcsslotd can be run as a different primary
  // uid.
  process_->SetUid(chronos_user_id_);
  process_->SetGid(pkcs11_group_id_);
  if(process_->Run() != 0) {
    LOG(ERROR) << "Couldn't start the pkcs slot daemon: " << kPkcsSlotdPath;
    return false;
  }
  return true;
}

bool Pkcs11Init::SetUserTokenPins() {
  CK_RV rv = C_Initialize(NULL);
  if (rv != CKR_CRYPTOKI_ALREADY_INITIALIZED && rv != CKR_OK) {
    LOG(ERROR) << "C_Initialize returned error code 0x" << std::hex << rv;
    return false;
  }

  CK_SLOT_ID slot_id = kDefaultTpmSlotId;

  // Initialize the token by login using the default openCryptoki SO PIN.
  CK_CHAR old_pin[kMaxPinLen];
  CK_CHAR ck_label[kMaxLabelLen];
  CKCharcpy(old_pin, kDefaultOpencryptokiSoPin, sizeof(old_pin));
  CKCharcpy(ck_label, kDefaultLabel, sizeof(ck_label));

  LOG(INFO) << "Initializing token by using default openCryptoki SO PIN.";
  rv = C_InitToken(slot_id,
                   old_pin, strlen(reinterpret_cast<const char*>(old_pin)),
                   ck_label);
  if (rv != CKR_OK) {
    LOG(ERROR) << "C_InitToken returned error code 0x" << std::hex << rv;
    return false;
  }

  // Reset the SO PIN to our default value.
  CK_CHAR new_pin[kMaxPinLen];
  CKCharcpy(new_pin, kDefaultSoPin, sizeof(new_pin));
  LOG(INFO) << "Resetting the SO PIN on the token to our default value.";
  if (!SetPin(slot_id, CKU_SO,
              old_pin, strlen(reinterpret_cast<const char*>(old_pin)),
              new_pin, strlen(reinterpret_cast<const char*>(new_pin)))) {
    LOG(ERROR) << "Couldn't reset the SO PIN on the token.";
    return false;
  }

  // Reset the User PIN to our default value.
  CKCharcpy(old_pin, kDefaultOpencryptokiUserPin, sizeof(old_pin));
  CKCharcpy(new_pin, kDefaultUserPin, sizeof(new_pin));
  LOG(INFO) << "Resetting the User PIN on the token to our default value.";
  if (!SetPin(slot_id, CKU_USER,
              old_pin, strlen(reinterpret_cast<const char*>(old_pin)),
              new_pin, strlen(reinterpret_cast<const char*>(new_pin)))) {
    LOG(ERROR) << "Couldn't reset the User PIN on the token.";
    return false;
  }

  LOG(INFO) << "Token initialization complete.";
  return true;
}

bool Pkcs11Init::IsOpencryptokiDirectoryValid() {
  if (!platform_->DirectoryExists(kOpencryptokiDir))
    return false;
  for (int i = 0; kSymlinkSources[i]; ++i) {
    std::string target = platform_->ReadLink(kSymlinkSources[i]);
    if (target != kUserTokenDir) {
      LOG(ERROR) << "Symlink " << kSymlinkSources[i] << " target should be \""
                 << kUserTokenDir << "\", not \"" << target << "\"";
      return false;
    }
  }
  return true;
}

bool Pkcs11Init::SetupOpencryptokiDirectory() {
  if (platform_->DirectoryExists(kOpencryptokiDir)) {
    if (!platform_->DeleteFile(kOpencryptokiDir, true)) {
      PLOG(ERROR) << "Unable to remove " << kOpencryptokiDir;
      return false;
    }
  }
  if (!platform_->CreateDirectory(kOpencryptokiDir)) {
    PLOG(ERROR) << "Unable to create " << kOpencryptokiDir;
    return false;
  }

  // Set correct ownership on the directory (root:$PKCS11_GROUP_ID).
  if (!platform_->SetOwnership(std::string(kOpencryptokiDir),
                               static_cast<uid_t>(0),  // Root uid is 0.
                               pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kOpencryptokiDir;
    return false;
  }

  // Make the directory group writable.
  mode_t opencryptoki_dir_perms = S_IRUSR | S_IWUSR | S_IXUSR |
      S_IRGRP | S_IWGRP | S_IXGRP;   // ug + rwx
  if (!platform_->SetPermissions(std::string(kOpencryptokiDir),
                                 opencryptoki_dir_perms)) {
    LOG(ERROR) << "Couldn't set permissions for " << kOpencryptokiDir;
    return false;
  }

  std::string opencryptoki_tpm = StringPrintf("%s/tpm", kOpencryptokiDir);
  if (!platform_->CreateDirectory(opencryptoki_tpm)) {
    LOG(ERROR) << "Couldn't create openCryptoki token directory "
               << opencryptoki_tpm;
    return false;
  }

  if (!platform_->SetOwnership(opencryptoki_tpm,
                               static_cast<uid_t>(0),  // Root uid is 0.
                               pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kOpencryptokiDir;
    return false;
  }

  if (!platform_->SetPermissions(opencryptoki_tpm,
                                 opencryptoki_dir_perms)) {
    LOG(ERROR) << "Couldn't set permissions for " << kOpencryptokiDir;
    return false;
  }

  // Ensure symbolic links for the token directories are correctly setup.
  // TODO(gauravsh): (crosbug.com/7284) Patch openCryptoki so that this is no
  // longer necessary.
  for (int i = 0; kSymlinkSources[i]; ++i) {
    if (!platform_->Symlink(std::string(kUserTokenDir),
                            std::string(kSymlinkSources[i]))) {
      PLOG(ERROR) << "Couldn't create symlink for " << kOpencryptokiDir;
      return false;
    }
  }

  LOG(INFO) << "Finished setting up " << kOpencryptokiDir;
  return true;
}

bool Pkcs11Init::SetupUserTokenDirectory() {
  LOG(INFO) << "Removing user token dir: " << kUserTokenDir;
  RemoveUserTokenDir();
  // Create token directory, since initialization neither creates or populates
  // it.
  std::string token_obj_dir = StringPrintf("%s/TOK_OBJ", kUserTokenDir);
  if (!platform_->DirectoryExists(token_obj_dir) &&
      !platform_->CreateDirectory(token_obj_dir))
    return false;

  // Setup permissions on the user token directory to ensure that users can
  // access their own data.
  mode_t user_dir_perms = S_IRWXU | S_IXGRP;  // u+rwx g+x
  if (!platform_->SetOwnership(std::string(kUserDir),
                               chronos_user_id_,
                               access_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kUserDir;
    return false;
  }
  if (!platform_->SetPermissions(std::string(kUserDir), user_dir_perms)) {
    LOG(ERROR) << "Couldn't set file permissions for " << kUserDir;
    return false;
  }

  if (!platform_->SetOwnershipRecursive(std::string(kUserTokenDir),
                                      chronos_user_id_,
                                      pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kUserTokenDir;
    return false;
  }

  return true;
}

bool Pkcs11Init::SetUserTokenFilePermissions() {
  if (!platform_->SetOwnershipRecursive(kUserTokenDir,
                                        chronos_user_id_,
                                        pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kUserTokenDir;
    return false;
  }

  mode_t dir_perms = S_IRWXU | S_IRGRP | S_IXGRP;  // u+rwx g+rx
  mode_t file_perms = S_IRUSR | S_IWUSR | S_IRGRP;  // u+rw g+r
  if (!platform_->SetPermissionsRecursive(kUserTokenDir,
                                          dir_perms,
                                          file_perms)) {
    LOG(ERROR) << "Couldn't set file permissions for " << kUserTokenDir;
    return false;
  }

  return true;
}

bool Pkcs11Init::RemoveUserTokenDir() {
  if (platform_->DirectoryExists(kUserTokenDir) &&
      !platform_->DeleteFile(kUserTokenDir, true)) {
    PLOG(ERROR) << "Could not delete existing user token directory: "
                << kUserTokenDir;
    return false;
  }
  return true;
}

bool Pkcs11Init::IsUserTokenBroken() {
  if (!platform_->FileExists(kTpmOwnedFile)) {
    LOG(WARNING) << "TPM is not owned, token can not be valid.";
    return true;
  }

  // Check if our "it is initialized" special file is there.
  if (!platform_->FileExists(kPkcs11InitializedFile))
    return true;

  if (!CheckTokenInSlot(kDefaultTpmSlotId, kDefaultLabel)) {
    LOG(WARNING) << "Token failed basic sanity checks. Can not be valid.";
    return true;
  }

  if (!platform_->FileExists(StringPrintf("%s/NVTOK.DAT",
                                        kUserTokenDir)) ||
      !platform_->FileExists(StringPrintf("%s/TOK_OBJ/70000000",
                                        kUserTokenDir))) {
    LOG(WARNING) << "PKCS#11 token is missing some files. Possibly not yet "
                 << "initialized? TOK_OBJ contents were: ";
    // Log the contents of what was observed in exists in the TOK_OBJ directory.
    std::vector<std::string> tok_file_list;
    platform_->EnumerateFiles(StringPrintf("%s/TOK_OBJ/",  kUserTokenDir), true,
                            &tok_file_list);
    for (std::vector<std::string>::iterator it = tok_file_list.begin();
         it != tok_file_list.end(); ++it)
      LOG(WARNING) << *it;
    return true;
  }

  LOG(INFO) << "PKCS#11 token looks ok.";
  return false;
}

bool Pkcs11Init::SetInitialized(bool status) {
  if (!status) {
    // Remove initialization state.
    platform_->DeleteFile(kPkcs11InitializedFile, false);
    is_initialized_ = false;
    return true;
  }

  chromeos::Blob empty_blob;
  if (!platform_->WriteFile(kPkcs11InitializedFile, empty_blob))
    return false;
  is_initialized_ = true;
  return true;
}

bool Pkcs11Init::SanitizeTokenDirectory() {
  for (int i = 0; kSensitiveTokenFiles[i]; ++i) {
    std::string full_path = StringPrintf("%s/%s",
                                         kUserTokenDir,
                                         kSensitiveTokenFiles[i]);
    if (platform_->FileExists(full_path)) {
      LOG(INFO) << "Removing " << full_path;
      if (!platform_->DeleteFile(full_path, false)) {
        PLOG(ERROR) << "Could not remove " << full_path;
        return false;
      }
    }
  }
  return true;
}

bool Pkcs11Init::SetPin(CK_SLOT_ID slot_id, CK_USER_TYPE user,
                        CK_CHAR_PTR old_pin, CK_ULONG old_pin_len,
                        CK_CHAR_PTR new_pin, CK_ULONG new_pin_len) {
  CK_RV rv;
  CK_SESSION_HANDLE session_handle = 0;
  rv = C_OpenSession(slot_id, CKF_RW_SESSION | CKF_SERIAL_SESSION,
                     NULL, NULL,
                     &session_handle);
  if (rv != CKR_OK) {
    LOG(ERROR) << "C_OpenSession returned error code 0x" << std::hex << rv;
    return false;
  }

  rv = C_Login(session_handle, user, old_pin, old_pin_len);
  if (rv != CKR_OK) {
    C_CloseAllSessions(slot_id);
    LOG(ERROR) << "Couldn't login using the old pin.";
    LOG(ERROR) << "C_Login returned error code 0x" << std::hex << rv;
    return false;
  }

  rv = C_SetPIN(session_handle, old_pin, old_pin_len, new_pin, new_pin_len);
  if (rv != CKR_OK) {
    C_CloseAllSessions(slot_id);
    LOG(ERROR) << "C_SetPIN returned error code 0x" << std::hex << rv;
    return false;
  }

  rv = C_CloseAllSessions(slot_id);
  if (rv != CKR_OK) {
    LOG(ERROR) << "C_CloseAllSessions returned error code 0x" << std::hex << rv;
    return false;
  }

  return true;
}

bool Pkcs11Init::CheckTokenInSlot(CK_SLOT_ID slot_id,
                                  const CK_CHAR* expected_label) {
  CK_RV rv;
  CK_SESSION_HANDLE session_handle = 0;
  CK_SESSION_INFO session_info;
  CK_TOKEN_INFO token_info;

  rv = C_Initialize(NULL);
  if (rv != CKR_OK && rv != CKR_CRYPTOKI_ALREADY_INITIALIZED) {
    LOG(WARNING) << "C_Initialize failed while checking token: "
                 << std::hex << rv;
    return false;
  }

  rv = C_OpenSession(slot_id, CKF_RW_SESSION | CKF_SERIAL_SESSION,
                     NULL, NULL,
                     &session_handle);
  if (rv != CKR_OK) {
    LOG(WARNING) << "Could not open session on slot " << slot_id
                 << " while checking token." << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  memset(&session_info, 0, sizeof(session_info));
  rv = C_GetSessionInfo(session_handle, &session_info);
  if (rv != CKR_OK || session_info.slotID != slot_id) {
    LOG(WARNING) << "Could not get session info on " << slot_id
                 << " while checking token: " << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  rv = C_GetTokenInfo(slot_id, &token_info);
  if (rv != CKR_OK || !(token_info.flags & CKF_TOKEN_INITIALIZED)) {
    LOG(WARNING) << "Could not get token info on " << slot_id
                 << " while checking token: " << std::hex << rv;
    C_CloseAllSessions(slot_id);
    return false;
  }

  if (strncmp(reinterpret_cast<const char*>(expected_label),
              reinterpret_cast<const char*>(token_info.label), kMaxLabelLen)) {
    LOG(WARNING) << "Token Label (" << token_info.label << ") does not match "
                 << "expected label (" << expected_label << ")";
    return false;
  }

  C_CloseAllSessions(slot_id);
  return true;
}

}  // namespace cryptohome
