// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains the implementation of class Pkcs11Init.

#include "pkcs11_init.h"

#include <iostream>

#include <base/logging.h>
#include <base/string_util.h>
#include <glib.h>
#include <opencryptoki/pkcs11.h>

#include "platform.h"


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
const CK_CHAR Pkcs11Init::kDefaultLabel[] = "TPM";
const char Pkcs11Init::kOpencryptokiDir[] = "/var/lib/opencryptoki";
const char Pkcs11Init::kUserTokenLink[] = "/var/lib/opencryptoki/tpm/chronos";
const char Pkcs11Init::kRootTokenLink[] = "/var/lib/opencryptoki/tpm/root";
const char Pkcs11Init::kUserTokenDir[] = "/home/chronos/user/.tpm";
const char Pkcs11Init::kRootTokenDir[] = "./chronos";
const char Pkcs11Init::kPkcs11Group[] = "pkcs11";
const char Pkcs11Init::kOldTokenEntry[] =
    "/var/lib/opencryptoki/pk_config_data";
const std::string Pkcs11Init::kPkcs11StartupPath = "/usr/sbin/pkcs11_startup";
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
  Platform platform;
  // Determine required uid and gids.
  if (!platform.GetGroupId(kPkcs11Group, &pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't get the group ID for group " << kPkcs11Group;
    return false;
  }
  if (!platform.GetUserId(kDefaultSharedUser, &chronos_user_id_,
                          &chronos_group_id_)) {
    LOG(ERROR) << "Couldn't get the user/group ID for user "
               << kDefaultSharedUser;
    return false;
  }

  if (!SetUpPkcs11System())
    return false;

  if (IsTokenBroken()) {
    LOG(WARNING) << "Broken Token. Removing user token dir " << kUserTokenDir;

    RemoveUserTokenDir();
    if (!SetUpLinksAndPermissions())
      return false;
    return SetUpTPMToken();
  }
  LOG(INFO) << "PKCS#11 is already initialized.";
  return true;
}

bool Pkcs11Init::SetUpPkcs11System() {
  Platform platform;
  // Run the pkcs11_startup script that will detect available tokens and update
  // pk_config_data appropriately. This is used later by pkcsslotd.
  std::string startup_cmd = kPkcs11StartupPath;
  std::string startup_args[1] = { startup_cmd };
  std::vector<std::string> startup_arguments;
  startup_arguments.assign(startup_args,
                           startup_args + arraysize(startup_args));
  // TODO(gauravsh): Consider moving to using process functions from
  // src/common/chromeos/process.h instead.
  //
  // Note: This must be run as root so that it can create the necessary
  // directories with the correct permissions under /var/lib. Since it's a
  // and not a service daemon, we are OK with that.
  if (!platform.Exec(startup_cmd, startup_arguments,
                     static_cast<uid_t>(0),  // Run as root.
                     pkcs11_group_id_))
    return false;

  // Start the slot manager daemon (pkcsslotd).
  std::string slotd_cmd = kPkcsSlotdPath;
  std::string slotd_args[1] = { slotd_cmd };
  std::vector<std::string> slotd_arguments;
  slotd_arguments.assign(slotd_args, slotd_args + arraysize(slotd_args));
  // TODO(gauravsh): Figure out if pkcsslotd can be run as a different primary
  // uid.
  if (!platform.Exec(slotd_cmd, slotd_arguments, chronos_user_id_,
                     pkcs11_group_id_))
    return false;
  return true;
}

bool Pkcs11Init::SetUpTPMToken() {
  Platform platform;

  // Setup the TPM as a token.
  std::vector<std::string> arguments;
  arguments.assign(kPkcsSlotCmd, kPkcsSlotCmd + arraysize(kPkcsSlotCmd));
  if (!platform.Exec(kPkcsSlotPath, arguments, 0, /* chronos_user_id_, */
                    pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't configure the tpm as a token. "
               << kPkcsSlotPath << " " << kPkcsSlotCmd[1] << " "
               << kPkcsSlotCmd[2] << "failed.";
    return false;
  }

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
  return SetInitialized(true);
}

bool Pkcs11Init::SetUpLinksAndPermissions() {
  Platform platform;
  std::string opencryptoki_tpm = StringPrintf("%s/tpm", kOpencryptokiDir);
  if (!platform.CreateDirectory(opencryptoki_tpm)) {
    LOG(ERROR) << "Couldn't create openCryptoki token directory "
               << opencryptoki_tpm;
    return false;
  }

  // Ensure symbolic links for the token directories are correctly setup.
  // TODO(gauravsh): (crosbug.com/7284) Patch openCryptoki so that this is no
  // longer necessary.
  if (!platform.Symlink(std::string(kUserTokenDir),
                        std::string(kUserTokenLink)))
    return false;

  if (!platform.Symlink(std::string(kRootTokenDir),
                        std::string(kRootTokenLink)))
    return false;

  // Remove old token directory.
  if (platform.DirectoryExists(kOldTokenEntry) &&
      !platform.DeleteFile(kOldTokenEntry, true))
    return false;

  // Create token directory, since initialization neither creates or populates
  // it.
  std::string token_obj_dir = StringPrintf("%s/TOK_OBJ", kUserTokenDir);
  if (!platform.DirectoryExists(token_obj_dir) &&
      !platform.CreateDirectory(token_obj_dir))
    return false;

  // Set correct permissions on opencryptoki directory (root:$PKCS11_GROUP_ID).
  if (!platform.SetOwnershipRecursive(std::string(kOpencryptokiDir),
                                      static_cast<uid_t>(0),  // Root uid is 0.
                                      pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kOpencryptokiDir;
    return false;
  }

  // Setup permissions on the user token directory to ensure that users can
  // access their own data.
  if (!platform.SetOwnershipRecursive(std::string(kUserTokenDir),
                                      chronos_user_id_,
                                      pkcs11_group_id_)) {
    LOG(ERROR) << "Couldn't set file ownership for " << kOpencryptokiDir;
    return false;
  }

  return true;
}

bool Pkcs11Init::IsTokenBroken() {
  Platform platform;
  // A non-existent token is also broken.
  if (!platform.FileExists(StringPrintf("%s/NVTOK.DAT", kUserTokenDir)))
    return true;

  if (!platform.FileExists(kTpmOwnedFile)) {
    LOG(WARNING) << "TPM is not owned, token can't valid.";
    return true;
  }

  if (!platform.FileExists(StringPrintf("%s/PRIVATE_ROOT_KEY.pem",
                                        kUserTokenDir)) ||
      !platform.FileExists(StringPrintf("%s/TOK_OBJ/70000000",
                                        kUserTokenDir))) {
    LOG(WARNING) << "PKCS#11 token is missing some files. Possibly not yet "
                 << "initialized? TOK_OBJ contents were: ";
    // Log the contents of what was observed in exists in the TOK_OBJ directory.
    std::vector<std::string> tok_file_list;
    platform.EnumerateFiles(StringPrintf("%s/TOK_OBJ/",  kUserTokenDir), true,
                            &tok_file_list);
    for (std::vector<std::string>::iterator it = tok_file_list.begin();
         it != tok_file_list.end(); ++it)
      LOG(WARNING) << *it;
    return true;
  }

  // At this point, we are done with basic sanity checking.
  // Now check if our "it is initialized" special file is still there.
  if (!platform.FileExists(kPkcs11InitializedFile))
    return true;

  LOG(INFO) << "PKCS#11 token looks ok.";
  return false;
}

bool Pkcs11Init::RemoveUserTokenDir() {
  Platform platform;
  if (platform.DirectoryExists(kUserTokenDir)) {
    return platform.DeleteFile(kUserTokenDir, true);
  }
  return true;
}

bool Pkcs11Init::SetInitialized(bool status) {
  Platform platform;
  if (!status) {
    // Remove initialization state.
    platform.DeleteFile(kPkcs11InitializedFile, false);
    is_initialized_ = false;
    return true;
  }

  chromeos::Blob empty_blob;
  if (!platform.WriteFile(kPkcs11InitializedFile, empty_blob))
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
