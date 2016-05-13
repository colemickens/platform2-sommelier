// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_POLICY_SERVICE_H_

#include <stdint.h>

#include <string>
#include <vector>

#include <base/callback.h>
#include <base/files/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <chromeos/dbus/service_constants.h>

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace base {
class WaitableEvent;
}  // namespace base

namespace login_manager {

class PolicyKey;
class PolicyStore;

// Manages policy storage and retrieval from an underlying PolicyStore, thereby
// enforcing policy signatures against a given policy key. Also handles key
// rotations in case a new policy payload comes with an updated policy key.
class PolicyService {
 public:
  // Flags determining what do to with new keys in Store().
  enum KeyInstallFlags {
    KEY_ROTATE = 1,       // Existing key may be rotated.
    KEY_INSTALL_NEW = 2,  // Allow to install a key if none is present.
    KEY_CLOBBER = 4,      // OK to replace the existing key without any checks.
  };

  // Wraps a policy service error with code and message.
  class Error {
   public:
    Error();
    Error(const std::string& code, const std::string& message);

    // Sets new error code and message.
    void Set(const std::string& code, const std::string& message);

    const std::string& code() const { return code_; }
    const std::string& message() const { return message_; }

   private:
    std::string code_;
    std::string message_;

    DISALLOW_COPY_AND_ASSIGN(Error);
  };

  // Callback for asynchronous completion of a Store operation.
  using Completion = base::Callback<void(const PolicyService::Error&)>;

  // Delegate for notifications about key and policy getting persisted.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnPolicyPersisted(bool success) = 0;
    virtual void OnKeyPersisted(bool success) = 0;
  };

  PolicyService(std::unique_ptr<PolicyStore> policy_store,
                PolicyKey* policy_key);
  virtual ~PolicyService();

  // Stores a new policy. Verifies the passed-in policy blob against the policy
  // key if it exists, takes care of key rotation if required and persists
  // everything to disk. The |flags| parameter determines what to do with a
  // new key present in the policy. See KeyInstallFlags for possible values.
  //
  // Returns false on immediate errors. Otherwise, returns true and reports the
  // status of the operation through |completion|.
  virtual bool Store(const uint8_t* policy_blob,
                     uint32_t len,
                     const Completion& completion,
                     int flags);

  // Retrieves the current policy blob. Returns true if successful, false
  // otherwise.
  virtual bool Retrieve(std::vector<uint8_t>* policy_blob);

  // Policy is persisted to disk on the IO loop. The current thread waits for
  // completion and reports back the status afterwards.
  virtual bool PersistPolicySync();

  // Accessors for the delegate. PolicyService doesn't own the delegate, thus
  // client code must make sure that the delegate pointer stays valid.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }
  Delegate* delegate() { return delegate_; }

 protected:
  friend class PolicyServiceTest;

  PolicyStore* store() { return policy_store_.get(); }
  PolicyKey* key() { return policy_key_; }
  void set_policy_key_for_test(PolicyKey* key) { policy_key_ = key; }

  // Schedules the key to be persisted.
  void PersistKey();

  // Schedules the policy to be persisted.
  void PersistPolicy();

  // Triggers persisting the policy to disk and reports the result to the given
  // completion context.
  void PersistPolicyWithCompletion(const Completion& completion);

  // Store a policy blob. This does the heavy lifting for Store(), making the
  // signature checks, taking care of key changes and persisting policy and key
  // data to disk.
  bool StorePolicy(const enterprise_management::PolicyFetchResponse& policy,
                   const Completion& completion,
                   int flags);

  // Completes a key storage operation on the UI thread, reporting the result to
  // the delegate.
  virtual void OnKeyPersisted(bool status);

  // Persists policy to disk on the main thread. If |completion| is non-null
  // it will be signaled when done.
  virtual void PersistPolicyOnLoop(const Completion& completion);

  // Finishes persisting policy, notifying the delegate and reporting the
  // |dbus_error_type| through |completion|. |completion| may be null, and
  // in that case the reporting part is not done. |dbus_error_type| is a
  // dbus_error constant and can be a non-error, like kNone.
  void OnPolicyPersisted(const Completion& completion,
                         const char* dbus_error_type);

 private:
  // Takes care of persisting the policy key to disk.
  void PersistKeyOnLoop();

 private:
  std::unique_ptr<PolicyStore> policy_store_;
  PolicyKey* policy_key_;
  Delegate* delegate_;

  base::WeakPtrFactory<PolicyService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_POLICY_SERVICE_H_
