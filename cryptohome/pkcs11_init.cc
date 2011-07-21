// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init.

#include "pkcs11_init.h"

#include <iostream>

#include <base/logging.h>
#include <base/string_util.h>
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
const char Pkcs11Init::kUserTokenLink[] = "/var/lib/opencryptoki/tpm/chronos";
const char Pkcs11Init::kIPsecTokenLink[] = "/var/lib/opencryptoki/tpm/ipsec";
const char Pkcs11Init::kRootTokenLink[] = "/var/lib/opencryptoki/tpm/root";
const char Pkcs11Init::kUserDir[] = "/home/chronos/user";
const char Pkcs11Init::kUserTokenDir[] = "/home/chronos/user/.tpm";
const char Pkcs11Init::kRootTokenDir[] = "./chronos";
const char Pkcs11Init::kPkcs11Group[] = "pkcs11";
const char Pkcs11Init::kTokenConfigFile[] =
    "/var/lib/opencryptoki/pk_config_data";
const std::string Pkcs11Init::kPkcsSlotdPath = "/usr/sbin/pkcsslotd";
const std::string Pkcs11Init::kPkcsSlotPath = "/usr/sbin/pkcs_slot";
const std::string Pkcs11Init::kPkcsSlotCmd[] = { Pkcs11Init::kPkcsSlotPath,
                                                 "0", "tpm" };
// This file in the user's cryptohome signifies that Pkcs11 initialization
// was completed for the user.
const std::string Pkcs11Init::kPkcs11InitializedFile =
    "/home/chronos/user/.tpm/.isinitialized";

extern const char* kTpmOwnedFile;
extern const std::string kDefaultSharedUser;

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

bool Pkcs11Init::Initialize() {
  // Determine required uid and gids.
  if (!platform_->GetGroupId(kPkcs11Group, &pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't get the group ID for group " << kPkcs11Group;
    return false;
  }
  if (!platform_->GetUserId(kDefaultSharedUser, &chronos_user_id_,
                          &chronos_group_id_)) {
    LOG(ERROR) << "Couldn't get the user/group ID for user "
               << kDefaultSharedUser;
    return false;
  }

  if (!platform_->DirectoryExists(kOpencryptokiDir) &&
      !SetupOpencryptokiDirectory())
    return false;

  if(!ConfigureTPMAsToken())
    return false;

  // Slot daemon can only be started after the opencryptoki directory has
  // correctly been setup and the TPM is initialized to be used as the slot 0
  // token.
  if (!StartPkcs11Daemon())
    return false;

  if (IsUserTokenBroken()) {
    LOG(WARNING) << "User token will be reinitialized.";

    if (!SetupUserTokenDirectory())
      return false;

    // PIN initialization needs to be performed as chronos:pkcs11.
    uid_t saved_uid;
    gid_t saved_gid;
    if (!platform_->SetProcessId(chronos_user_id_, pkcs11_group_id_, &saved_uid,
                                 &saved_gid))
      return false;
    bool success = SetUserTokenPins() && SetUserTokenFilePermissions();
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
    return success;
  }

  LOG(INFO) << "PKCS#11 is already initialized.";
  return true;
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

bool Pkcs11Init::SetupOpencryptokiDirectory() {
  std::string opencryptoki_tpm = StringPrintf("%s/tpm", kOpencryptokiDir);
  if (!platform_->CreateDirectory(opencryptoki_tpm)) {
    LOG(ERROR) << "Couldn't create openCryptoki token directory "
               << opencryptoki_tpm;
    return false;
  }

  // Ensure symbolic links for the token directories are correctly setup.
  // TODO(gauravsh): (crosbug.com/7284) Patch openCryptoki so that this is no
  // longer necessary.
  if (!platform_->Symlink(std::string(kUserTokenDir),
                          std::string(kUserTokenLink)))
    return false;

  if (!platform_->Symlink(std::string(kUserTokenDir),
                          std::string(kIPsecTokenLink)))
    return false;

  if (!platform_->Symlink(std::string(kRootTokenDir),
                          std::string(kRootTokenLink)))
    return false;

  // Set correct ownership on the directory (root:$PKCS11_GROUP_ID).
  if (!platform_->SetOwnership(std::string(kOpencryptokiDir),
                               static_cast<uid_t>(0),  // Root uid is 0.
                               pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kOpencryptokiDir;
    return false;
  }

  if (!platform_->SetOwnership(opencryptoki_tpm,
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
  if (!platform_->SetPermissions(opencryptoki_tpm,
                                 opencryptoki_dir_perms)) {
    LOG(ERROR) << "Couldn't set permissions for " << kOpencryptokiDir;
    return false;
  }
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
                               pkcs11_group_id_)) {
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

  // For pkcs11 group to be able to read data from the token, the map file for
  // communicating with pkcsslotd also needs to be writable by pkcs11 group.
  mode_t stmapfile_perms = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;  // ug+rw
  std::string stmapfile = StringPrintf("%s/.stmapfile", kUserTokenDir);
  if (!platform_->SetPermissions(stmapfile, stmapfile_perms)) {
    LOG(ERROR) << "Couldn't set file permissions for " << stmapfile_perms;
    return false;
  }

  return true;
}

bool Pkcs11Init::RemoveUserTokenDir() {
  if (platform_->DirectoryExists(kUserTokenDir)) {
    return platform_->DeleteFile(kUserTokenDir, true);
  }
  return true;
}

bool Pkcs11Init::IsUserTokenBroken() {
  // A non-existent token is also broken.
  if (!platform_->FileExists(StringPrintf("%s/NVTOK.DAT", kUserTokenDir)))
    return true;

  if (!platform_->FileExists(kTpmOwnedFile)) {
    LOG(WARNING) << "TPM is not owned, token can't valid.";
    return true;
  }

  if (!platform_->FileExists(StringPrintf("%s/PRIVATE_ROOT_KEY.pem",
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

  // At this point, we are done with basic sanity checking.
  // Now check if our "it is initialized" special file is still there.
  if (!platform_->FileExists(kPkcs11InitializedFile))
    return true;

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

}  // namespace cryptohome
