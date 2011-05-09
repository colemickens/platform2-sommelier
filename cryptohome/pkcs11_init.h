// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pkcs11Init - class for handling opencryptoki initialization.

#ifndef CRYPTOHOME_PKCS11_INIT_H_
#define CRYPTOHOME_PKCS11_INIT_H_

#include <sys/types.h>

#include <string>

#include <base/basictypes.h>
#include <base/scoped_ptr.h>
#include <chromeos/process.h>
#include <glib.h>
#include <opencryptoki/pkcs11.h>

namespace cryptohome {

class Pkcs11Init {
 public:
  Pkcs11Init() : is_initialized_(false), pkcs11_group_id_(0),
                 chronos_user_id_(0), chronos_group_id_(0),
                 default_process_(new chromeos::ProcessImpl),
                 process_(default_process_.get()) { }
  virtual ~Pkcs11Init() { }

  virtual void GetTpmTokenInfo(gchar **OUT_label,
                               gchar **OUT_user_pin);

  // Initialize openCryptoki. This includes starting and setting up the
  // necessary service daemons, and initializing the tokens.
  virtual bool Initialize();

  // Start the PKCS11 service daemons.
  virtual bool SetUpPkcs11System();

  // Initialize a PKCS#11 token and set default SO and User PINs. Will return
  // false if the token was found to be already initialized.
  virtual bool SetUpTPMToken();

  // Sets the correct symlinks to the user's opencryptoki token dir. Also,
  // sets permissions correctly for all the files and directory corresponding
  // to the token.
  virtual bool SetUpLinksAndPermissions();

  // Check if the PKCS user token is valid.
  virtual bool IsTokenBroken();

  // Remove the user's token dir. Useful when token directory is found to be
  // corrupted.
  virtual bool RemoveUserTokenDir();

  virtual bool is_initialized() const { return is_initialized_; }

  // Sets the Pkcs11 initialization to true or false by dropping or removing
  // the initialized file.
  virtual bool SetInitialized(bool status);

  static const CK_SLOT_ID kDefaultTpmSlotId;
  static const CK_ULONG kMaxPinLen;
  static const CK_ULONG kMaxLabelLen;
  static const CK_CHAR kDefaultOpencryptokiSoPin[];
  static const CK_CHAR kDefaultOpencryptokiUserPin[];
  static const CK_CHAR kDefaultSoPin[];
  static const CK_CHAR kDefaultUserPin[];
  static const CK_CHAR kDefaultLabel[];
  static const char kOpencryptokiDir[];
  static const char kUserTokenLink[];
  static const char kRootTokenLink[];
  static const char kUserTokenDir[];
  static const char kRootTokenDir[];
  static const char kPkcs11Group[];
  static const char kOldTokenEntry[];
  static const std::string kPkcs11StartupPath;
  static const std::string kPkcsSlotdPath;
  static const std::string kPkcsSlotPath;
  static const std::string kPkcsSlotCmd[];

  static const std::string kPkcs11InitializedFile;

 private:
  // Set the PIN on |slot_id| for user |user| to |new_pin| of length
  // |new_pin_len|
  bool SetPin(CK_SLOT_ID slot_id, CK_USER_TYPE user,
              CK_CHAR_PTR old_pin, CK_ULONG old_pin_len,
              CK_CHAR_PTR new_pin, CK_ULONG new_pin_len);

  bool is_initialized_;

  gid_t pkcs11_group_id_;
  uid_t chronos_user_id_;
  gid_t chronos_group_id_;

  scoped_ptr<chromeos::ProcessImpl> default_process_;
  chromeos::ProcessImpl* process_;

  DISALLOW_COPY_AND_ASSIGN(Pkcs11Init);
};

}  // namespace cryptohome

#endif  // CRYPTOHOME_PKCS11_INIT_H_
