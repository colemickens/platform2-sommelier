 // Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_
#define LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

#include <base/basictypes.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>

#include "login_manager/owner_key_loss_mitigator.h"
#include "login_manager/policy_service.h"

namespace login_manager {
class KeyGenerator;
class LoginMetrics;
class NssUtil;
class OwnerKeyLossMitigator;

// A policy service specifically for device policy, adding in a few helpers for
// generating a new key for the device owner, handling key loss mitigation,
// storing owner properties etc.
class DevicePolicyService : public PolicyService {
 public:
  virtual ~DevicePolicyService();

  // Instantiates a regular (non-testing) device policy service instance.
  static DevicePolicyService* Create(
      LoginMetrics* metrics,
      OwnerKeyLossMitigator* mitigator,
      const scoped_refptr<base::MessageLoopProxy>& main_loop);

  // Checks whether the given |current_user| is the device owner. The result of
  // the check is returned in |is_owner|. If so, it is validated that the device
  // policy settings are set up appropriately:
  // - If |current_user| has the owner key, put her on the login white list.
  // - If policy claims |current_user| is the device owner but she doesn't
  //   appear to have the owner key, run key mitigation.
  // Returns true on success. Fills in |error| upon encountering an error.
  virtual bool CheckAndHandleOwnerLogin(const std::string& current_user,
                                        bool* is_owner,
                                        Error* error);

  // Ensures that the public key in |buf| is legitimately paired with a private
  // key held by the current user, signs and stores some ownership-related
  // metadata, and then stores this key off as the new device owner key. Returns
  // true if successful, false otherwise
  virtual bool ValidateAndStoreOwnerKey(const std::string& current_user,
                                        const std::string& buf);

  // Checks whether the key is missing.
  virtual bool KeyMissing();

  // Overriden from PolicyService to check the serial number recovery flag.
  // TODO(mnissler): Remove once bogus enterprise serials are fixed.
  virtual bool Initialize();

  // Given info about whether we were able to load the Owner key and the
  // device policy, report the state of these files via |metrics_|.
  virtual void ReportPolicyFileMetrics(bool key_success, bool policy_success);

  virtual bool Store(const uint8* policy_blob,
                     uint32 len,
                     Completion* completion,
                     int flags);

  static const char kPolicyPath[];
  static const char kSerialRecoveryFlagFile[];

  // Format of this string is documented in device_management_backend.proto.
  static const char kDevicePolicyType[];

 private:
  friend class DevicePolicyServiceTest;
  friend class MockDevicePolicyService;

  // Takes ownership of |policy_store|, |policy_key|, |system_utils|, and |nss|.
  DevicePolicyService(const FilePath& serial_recovery_flag_file,
                      PolicyStore* policy_store,
                      OwnerKey* policy_key,
                      const scoped_refptr<base::MessageLoopProxy>& main_loop,
                      NssUtil* nss,
                      LoginMetrics* metrics,
                      OwnerKeyLossMitigator* mitigator);

  // Assuming the current user has access to the owner private key (read: is the
  // owner), this call whitelists |current_user| and sets a property indicating
  // |current_user| is the owner in the current policy and schedules a
  // PersistPolicy().
  // Returns false on failure, with |error| set appropriately. |error| can be
  // NULL, should you wish to ignore the particulars.
  bool StoreOwnerProperties(const std::string& current_user, Error* error);

  // Checks the user's NSS database to see if she has the private key.
  bool CurrentUserHasOwnerKey(const std::vector<uint8>& key, Error* error);

  // Returns true if the current user is listed in |policy_| as the
  // device owner.  Returns false if not, or if that cannot be determined.
  bool CurrentUserIsOwner(const std::string& current_user);

  // Checks the serial number recovery flag and updates the flag file.
  // TODO(mnissler): Remove once bogus enterprise serials are fixed.
  void UpdateSerialNumberRecoveryFlagFile();

  const FilePath serial_recovery_flag_file_;
  scoped_ptr<NssUtil> nss_;
  LoginMetrics* metrics_;
  OwnerKeyLossMitigator* mitigator_;

  DISALLOW_COPY_AND_ASSIGN(DevicePolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_DEVICE_POLICY_SERVICE_H_
