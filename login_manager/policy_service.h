// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_POLICY_SERVICE_H_
#define LOGIN_MANAGER_POLICY_SERVICE_H_

#include <string>

#include <dbus/dbus-glib-lowlevel.h>

#include <base/basictypes.h>
#include <base/file_path.h>
#include <base/memory/ref_counted.h>
#include <base/memory/scoped_ptr.h>
#include <chromeos/dbus/service_constants.h>

#include "login_manager/system_utils.h"

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

  // Takes ownership of |policy_store| and |policy_key|.
  PolicyService(PolicyStore* policy_store,
                OwnerKey* policy_key,
                SystemUtils* system_utils,
                const scoped_refptr<base::MessageLoopProxy>& main_loop,
                const scoped_refptr<base::MessageLoopProxy>& io_loop);
  virtual ~PolicyService();

  // Reads policy key and data from disk.
  virtual bool Initialize();

  // Stores a new policy. Verifies the passed-in policy blob against the policy
  // key if it exists, takes care of key rotation if required and persists
  // everything to disk. The |flags| parameter determines what to do with a
  // new key present in the policy. See KeyInstallFlags for possible values.
  //
  // Returns FALSE on immediate errors. Otherwise, returns TRUE and reports the
  // status of the operation through |context|.
  virtual gboolean Store(GArray* policy_blob,
                         DBusGMethodInvocation* context,
                         int flags);

  // Retrieves the current policy blob. Returns TRUE if successful. On errors,
  // FALSE is returned and |error| filled in appropriately.
  virtual gboolean Retrieve(GArray** policy_blob, GError** error);

  // Policy is persisted to disk on the IO loop. The current thread waits for
  // completion and reports back the status afterwards.
  virtual bool PersistPolicySync();

 protected:
  friend class PolicyServiceTest;

  PolicyStore* store() { return policy_store_.get(); }
  OwnerKey* key() { return policy_key_.get(); }
  SystemUtils* system() { return system_.get(); }
  const scoped_refptr<base::MessageLoopProxy>& main_loop() {
    return main_loop_;
  }

  // Schedules the key to be persisted.
  virtual void PersistKey();

  // Schedules the policy to be persisted.
  virtual void PersistPolicy();

  // Triggers persisting the policy to disk and finishes the given DBUS call,
  // inidicating the result.
  virtual void PersistPolicyWithContext(DBusGMethodInvocation* context);

  // Takes care of persisting the policy key to disk and triggers a
  // kOwnerKeySetSignal in order to report the status.
  virtual void PersistKeyOnIOLoop(bool* result);

  // Persists policy to disk on the IO thread and delivers the status through a
  // kPropertyChangeCompleteSignal. If |event| is non-NULL, it will be signaled
  // when done. |result| receives the status if it is non-NULL.
  virtual void PersistPolicyOnIOLoop(base::WaitableEvent* event, bool* result);

  // Persists the policy to disk and finishes the DBUS call referenced by
  // |context| with an appropriate status code.
  virtual void PersistPolicyWithContextOnIOLoop(DBusGMethodInvocation* context);

  // Finishes the DBus call referenced by |context|. If |status| is true, the
  // call will complete successfully, otherwise the given error |code| will be
  // reported back.
  void CompleteDBusCall(DBusGMethodInvocation* context,
                        bool status,
                        ChromeOSLoginError code,
                        const std::string& message);

 private:
  scoped_ptr<PolicyStore> policy_store_;
  scoped_ptr<OwnerKey> policy_key_;
  scoped_ptr<SystemUtils> system_;
  scoped_refptr<base::MessageLoopProxy> main_loop_;
  scoped_refptr<base::MessageLoopProxy> io_loop_;

  DISALLOW_COPY_AND_ASSIGN(PolicyService);
};

}  // namespace login_manager

#endif  // LOGIN_MANAGER_POLICY_SERVICE_H_
