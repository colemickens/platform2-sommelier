// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_POLICY_SERVICE_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/error_constants.h>
#include <chromeos/dbus/service_constants.h>

namespace enterprise_management {
class PolicyFetchResponse;
}

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}  // namespace base

namespace login_manager {

class OwnerKey;
class PolicyStore;

// Manages policy storage and retrieval from an underlying PolicyStore, thereby
// enforcing policy signatures against a given policy key. Also handles key
// rotations in case a new policy payload comes with an updated policy key.
class PolicyService : public base::RefCountedThreadSafe<PolicyService> {
 public:
  // Flags determining what do to with new keys in Store().
  enum KeyInstallFlags {
    KEY_ROTATE = 1,      // Existing key may be rotated.
    KEY_INSTALL_NEW = 2, // Allow to install a key if none is present.
    KEY_CLOBBER = 4,     // OK to replace the existing key without any checks.
  };

  // Wraps a policy service error with code and message.
  class Error {
   public:
    Error();
    Error(ChromeOSLoginError code, const std::string& message);

    // Sets new error code and message.
    void Set(ChromeOSLoginError code, const std::string& message);

    ChromeOSLoginError code() const { return code_; }
    const std::string& message() const { return message_; }

   private:
    ChromeOSLoginError code_;
    std::string message_;

    DISALLOW_COPY_AND_ASSIGN(Error);
  };

  // Callback interface for asynchronous completion of a Store operation.
  class Completion {
   public:
    virtual ~Completion();
    virtual void Success() = 0;
    virtual void Failure(const Error& error) = 0;
  };

  // Delegate for notifications about key and policy getting persisted.
  class Delegate {
   public:
    virtual ~Delegate();
    virtual void OnPolicyPersisted(bool success) = 0;
    virtual void OnKeyPersisted(bool success) = 0;
  };

  // Takes ownership of |policy_store| and |policy_key|.
  PolicyService(PolicyStore* policy_store,
                OwnerKey* policy_key,
                const scoped_refptr<base::MessageLoopProxy>& main_loop);
  virtual ~PolicyService();

  // Reads policy key and data from disk.
  // Returns false if policy key loading fails, which is fatal. True otherwise.
  virtual bool Initialize();

  // Stores a new policy. Verifies the passed-in policy blob against the policy
  // key if it exists, takes care of key rotation if required and persists
  // everything to disk. The |flags| parameter determines what to do with a
  // new key present in the policy. See KeyInstallFlags for possible values.
  //
  // Returns false on immediate errors. Otherwise, returns true and reports the
  // status of the operation through |completion|.
  virtual bool Store(const uint8* policy_blob,
                     uint32 len,
                     Completion* completion,
                     int flags);

  // Retrieves the current policy blob. Returns true if successful, false
  // otherwise.
  virtual bool Retrieve(std::vector<uint8>* policy_blob);

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
  OwnerKey* key() { return policy_key_.get(); }
  const scoped_refptr<base::MessageLoopProxy>& main_loop() {
    return main_loop_;
  }

  // Reads policy key and data from disk.
  // Returns false if policy key loading fails, which is fatal. True otherwise.
  // If policy file loading fails, |policy_success| will be set to false.
  // It will be set to true otherwise, including if there is no file to read.
  bool DoInitialize(bool* policy_success);

  // Schedules the key to be persisted.
  void PersistKey();

  // Schedules the policy to be persisted.
  void PersistPolicy();

  // Triggers persisting the policy to disk and reports the result to the given
  // completion context.
  void PersistPolicyWithCompletion(Completion* completion);

  // Store a policy blob. This does the heavy lifting for Store(), making the
  // signature checks, taking care of key changes and persisting policy and key
  // data to disk.
  bool StorePolicy(const enterprise_management::PolicyFetchResponse& policy,
                   Completion* completion,
                   int flags);

 private:
  // Takes care of persisting the policy key to disk.
  void PersistKeyOnLoop();

  // Persists policy to disk on the main thread. If |completion| is non-NULL
  // it will be signaled when done.
  void PersistPolicyOnLoop(Completion* completion);

  // Completes a key storage operation on the UI thread, reporting the result to
  // the delegate.
  void OnKeyPersisted(bool status);

  // Finishes persisting policy with |status|, notifying the delegate and
  // reporting the status through |completion|.
  void OnPolicyPersisted(Completion* completion, bool status);

  // Takes care of inidicating asynchronous call completion on the UI thread.
  // Calls completion->Success() or completion->Failure() depending on |status|,
  // and sets the error code and message appropriately.
  void CompleteCall(Completion* completion,
                    bool status,
                    ChromeOSLoginError code,
                    const std::string& msg);

 private:
  scoped_ptr<PolicyStore> policy_store_;
  scoped_ptr<OwnerKey> policy_key_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_POLICY_SERVICE_H_
