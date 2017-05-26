// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_POLICY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <base/callback.h>
#include <base/memory/weak_ptr.h>
#include <brillo/errors/error.h>
#include <chromeos/dbus/service_constants.h>

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace login_manager {

class PolicyKey;
class PolicyStore;

// Whether policy signature must be checked in PolicyService::Store().
enum class SignatureCheck { kEnabled, kDisabled };

// Manages policy storage and retrieval from an underlying PolicyStore, thereby
// enforcing policy signatures against a given policy key. Also handles key
// rotations in case a new policy payload comes with an updated policy key.
class PolicyService {
 public:
  // Flags determining what do to with new keys in Store().
  enum KeyInstallFlags {
    KEY_NONE = 0,         // No key changes allowed.
    KEY_ROTATE = 1,       // Existing key may be rotated.
    KEY_INSTALL_NEW = 2,  // Allow to install a key if none is present.
    KEY_CLOBBER = 4,      // OK to replace the existing key without any checks.
  };

  // Callback for asynchronous completion of a Store operation.
  // On success, |error| is nullptr. Otherwise, it contains an instance
  // with detailed info.
  using Completion = base::Callback<void(brillo::ErrorPtr error)>;

  // Delegate for notifications about key and policy getting persisted.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnPolicyPersisted(bool success) = 0;
    virtual void OnKeyPersisted(bool success) = 0;
  };

  PolicyService(std::unique_ptr<PolicyStore> policy_store,
                PolicyKey* policy_key);
  virtual ~PolicyService();

  // Stores a new policy. If mandated by |signature_check|, verifies the
  // passed-in policy blob against the policy key (if it exists), takes care of
  // key rotation if required and persists everything to disk. The |key_flags|
  // parameter determines what to do with a new key present in the policy, see
  // KeyInstallFlags for possible values.
  //
  // Returns false on immediate errors. Otherwise, returns true and reports the
  // status of the operation through |completion|.
  virtual bool Store(const std::vector<uint8_t>& policy_blob,
                     int key_flags,
                     SignatureCheck signature_check,
                     const Completion& completion);

  // Retrieves the current policy blob (does not verify the signature). Returns
  // true if successful, false otherwise.
  virtual bool Retrieve(std::vector<uint8_t>* policy_blob);

  // Persists policy to disk synchronously and passes |completion| and the
  // result to OnPolicyPersisted().
  virtual void PersistPolicy(const Completion& completion);

  // Accessors for the delegate. PolicyService doesn't own the delegate, thus
  // client code must make sure that the delegate pointer stays valid.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 protected:
  friend class PolicyServiceTest;

  PolicyStore* store() { return policy_store_.get(); }
  PolicyKey* key() { return policy_key_; }
  void set_policy_key_for_test(PolicyKey* key) { policy_key_ = key; }

  // Posts a task to run PersistKey().
  void PostPersistKeyTask();

  // Posts a task to run PersistPolicy().
  void PostPersistPolicyTask(const Completion& completion);

  // Store a policy blob. This does the heavy lifting for Store(), making the
  // signature checks, taking care of key changes and persisting policy and key
  // data to disk.
  bool StorePolicy(const enterprise_management::PolicyFetchResponse& policy,
                   int key_flags,
                   SignatureCheck signature_check,
                   const Completion& completion);

  // Handles completion of a key storage operation, reporting the result to
  // |delegate_|.
  virtual void OnKeyPersisted(bool status);

  // Finishes persisting policy, notifying |delegate_| and reporting the
  // |dbus_error_code| through |completion|. |completion| may be null, and in
  // that case the reporting part is not done. |dbus_error_code| is a dbus_error
  // constant and can be a non-error, like kNone.
  void OnPolicyPersisted(const Completion& completion,
                         const std::string& dbus_error_code);

 private:
  // Persists key() to disk synchronously and passes the result to
  // OnKeyPersisted().
  void PersistKey();

 private:
  std::unique_ptr<PolicyStore> policy_store_;
  PolicyKey* policy_key_;
  Delegate* delegate_;

  // Put at the last member, so that inflight weakptrs will be invalidated
  // before other members' destruction.
  base::WeakPtrFactory<PolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_POLICY_SERVICE_H_
