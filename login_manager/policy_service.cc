// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "login_manager/policy_service.h"

#include <string>

#include <base/logging.h>
#include <base/message_loop_proxy.h>
#include <base/synchronization/waitable_event.h>
#include <base/task.h>

#include "login_manager/bindings/device_management_backend.pb.h"
#include "login_manager/nss_util.h"
#include "login_manager/owner_key.h"
#include "login_manager/policy_store.h"

namespace em = enterprise_management;

namespace login_manager {

PolicyService::PolicyService(
    PolicyStore* policy_store,
    OwnerKey* policy_key,
    SystemUtils* system_utils,
    const scoped_refptr<base::MessageLoopProxy>& main_loop,
    const scoped_refptr<base::MessageLoopProxy>& io_loop)
    : policy_store_(policy_store),
      policy_key_(policy_key),
      system_(system_utils),
      main_loop_(main_loop),
      io_loop_(io_loop) {
}

PolicyService::~PolicyService() {
}

bool PolicyService::Initialize() {
  if (!key()->PopulateFromDiskIfPossible()) {
    LOG(ERROR) << "Failed to load policy key from disk.";
    return false;
  }

  if (key()->IsPopulated()) {
    if (!store()->LoadOrCreate())
      LOG(ERROR) << "Failed to load policy data, continuing anyway.";
  }

  return true;
}

gboolean PolicyService::Store(GArray* policy_blob,
                              DBusGMethodInvocation* context,
                              int flags) {
  em::PolicyFetchResponse policy;
  if (!policy.ParseFromArray(policy_blob->data, policy_blob->len) ||
      !policy.has_policy_data() ||
      !policy.has_policy_data_signature()) {
    const char msg[] = "Unable to parse policy protobuf.";
    LOG(ERROR) << msg;
    system_->SetAndSendGError(CHROMEOS_LOGIN_ERROR_DECODE_FAIL, context, msg);
    return FALSE;
  }

  // Determine if the policy has pushed a new owner key and, if so, set it.
  if (policy.has_new_public_key() && !key()->Equals(policy.new_public_key())) {
    // The policy contains a new key, and it is different from |key_|.
    std::vector<uint8> der;
    NssUtil::BlobFromBuffer(policy.new_public_key(), &der);

    bool installed = false;
    if (key()->IsPopulated()) {
      if (policy.has_new_public_key_signature() && (flags & KEY_ROTATE)) {
        // Graceful key rotation.
        LOG(INFO) << "Rotating policy key.";
        std::vector<uint8> sig;
        NssUtil::BlobFromBuffer(policy.new_public_key_signature(), &sig);
        installed = key()->Rotate(der, sig);
      }
    } else if (flags & KEY_INSTALL_NEW) {
      LOG(INFO) << "Installing new policy key.";
      installed = key()->PopulateFromBuffer(der);
    }
    if (!installed && (flags & KEY_CLOBBER)) {
      LOG(INFO) << "Clobbering existing policy key.";
      installed = key()->ClobberCompromisedKey(der);
    }

    if (!installed) {
      const char msg[] = "Failed to install policy key!";
      LOG(ERROR) << msg;
      system_->SetAndSendGError(CHROMEOS_LOGIN_ERROR_ILLEGAL_PUBKEY,
                                context,
                                msg);
      return FALSE;
    }

    // If here, need to persist the key just loaded into memory to disk.
    PersistKey();
  }

  // Validate signature on policy and persist to disk.
  const std::string& data(policy.policy_data());
  const std::string& sig(policy.policy_data_signature());
  if (!key()->Verify(reinterpret_cast<const uint8*>(data.c_str()),
                     data.size(),
                     reinterpret_cast<const uint8*>(sig.c_str()),
                     sig.size())) {
    const char msg[] = "Signature could not be verified.";
    LOG(ERROR) << msg;
    system_->SetAndSendGError(CHROMEOS_LOGIN_ERROR_VERIFY_FAIL, context, msg);
    return FALSE;
  }

  store()->Set(policy);
  PersistPolicyWithContext(context);
  return TRUE;
}

gboolean PolicyService::Retrieve(GArray** policy_blob, GError** error) {
  const em::PolicyFetchResponse& policy = store()->Get();
  int size = policy.ByteSize();
  *policy_blob = g_array_sized_new(FALSE, FALSE, sizeof(uint8), size);
  if (!*policy_blob) {
    const char msg[] = "Unable to allocate memory for response.";
    LOG(ERROR) << msg;
    system_->SetGError(error, CHROMEOS_LOGIN_ERROR_DECODE_FAIL, msg);
    return FALSE;
  }
  g_array_set_size(*policy_blob, size);
  uint8* start = reinterpret_cast<uint8*>((*policy_blob)->data);
  uint8* end = policy.SerializeWithCachedSizesToArray(start);
  return (end - start == (*policy_blob)->len) ? TRUE : FALSE;
}

bool PolicyService::PersistPolicySync() {
  // Even if we haven't gotten around to processing a persist task.
  base::WaitableEvent event(true, false);
  bool result;
  io_loop_->PostTask(FROM_HERE,
                     NewRunnableMethod(this,
                                       &PolicyService::PersistPolicyOnIOLoop,
                                       &event,
                                       &result));
  event.Wait();
  return result;
}

void PolicyService::PersistKey() {
  io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyService::PersistKeyOnIOLoop,
                        static_cast<bool*>(NULL)));
}

void PolicyService::PersistPolicy() {
  base::WaitableEvent* event = NULL;
  bool* result = NULL;
  io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyService::PersistPolicyOnIOLoop,
                        event,
                        result));
}

void PolicyService::PersistPolicyWithContext(DBusGMethodInvocation* context) {
  io_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &PolicyService::PersistPolicyWithContextOnIOLoop,
                        context));
}

void PolicyService::PersistKeyOnIOLoop(bool* result) {
  DCHECK(io_loop_->BelongsToCurrentThread());
  bool status = key()->Persist();
  if (status)
    LOG(INFO) << "Persisted policy key to disk.";
  else
    LOG(ERROR) << "Failed to persist policy key to disk.";
  if (result)
    *result = status;
}

void PolicyService::PersistPolicyOnIOLoop(base::WaitableEvent* event,
                                          bool* result) {
  DCHECK(io_loop_->BelongsToCurrentThread());
  bool status = store()->Persist();
  if (status)
    LOG(INFO) << "Persisted policy to disk.";
  else
    LOG(ERROR) << "Failed to persist policy to disk.";
  if (result)
    *result = status;
  if (event)
    event->Signal();
}

void PolicyService::PersistPolicyWithContextOnIOLoop(
    DBusGMethodInvocation* context) {
  DCHECK(io_loop_->BelongsToCurrentThread());
  bool status = store()->Persist();
  std::string msg = "Failed to persist policy data to disk.";
  if (!status)
    LOG(ERROR) << msg;
  if (context) {
    main_loop_->PostTask(FROM_HERE,
                         NewRunnableMethod(this,
                                           &PolicyService::CompleteDBusCall,
                                           context,
                                           status,
                                           CHROMEOS_LOGIN_ERROR_ENCODE_FAIL,
                                           msg));
  }
}

void PolicyService::CompleteDBusCall(DBusGMethodInvocation* context,
                                     bool status,
                                     ChromeOSLoginError code,
                                     const std::string& msg) {
  if (status) {
    dbus_g_method_return(context, true);
  } else {
    system_->SetAndSendGError(code, context, msg.c_str());
  }
}

}  // namespace login_manager
