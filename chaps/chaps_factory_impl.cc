// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chaps/chaps_factory_impl.h"

#include <string>

#include <base/logging.h>

#include "chaps/object_impl.h"
#include "chaps/object_policy_cert.h"
#include "chaps/object_policy_common.h"
#include "chaps/object_policy_data.h"
#include "chaps/object_policy_private_key.h"
#include "chaps/object_policy_public_key.h"
#include "chaps/object_policy_secret_key.h"
#include "chaps/object_pool_impl.h"
#include "chaps/object_store_fake.h"
#include "chaps/session_impl.h"

using std::string;

namespace chaps {

Session* ChapsFactoryImpl::CreateSession(int slot_id,
                                         ObjectPool* token_object_pool,
                                         TPMUtility* tpm_utility,
                                         HandleGenerator* handle_generator,
                                         bool is_read_only) {
  return new SessionImpl(slot_id,
                         token_object_pool,
                         tpm_utility,
                         this,
                         handle_generator,
                         is_read_only);
}

ObjectPool* ChapsFactoryImpl::CreateObjectPool(
    HandleGenerator* handle_generator,
    ObjectStore* object_store) {
  scoped_ptr<ObjectPoolImpl> pool(new ObjectPoolImpl(this,
                                                     handle_generator,
                                                     object_store));
  CHECK(pool->Init()) << "Object pool initialization failed.";
  return pool.release();
}

ObjectStore* ChapsFactoryImpl::CreateObjectStore(const FilePath& file_name) {
  // TODO(dkrahn) Implement a real object store.
  return new ObjectStoreFake();
}

Object* ChapsFactoryImpl::CreateObject() {
  return new ObjectImpl(this);
}

ObjectPolicy* ChapsFactoryImpl::CreateObjectPolicy(CK_OBJECT_CLASS type) {
  switch (type) {
    case CKO_DATA:
      return new ObjectPolicyData();
    case CKO_CERTIFICATE:
      return new ObjectPolicyCert();
    case CKO_PUBLIC_KEY:
      return new ObjectPolicyPublicKey();
    case CKO_PRIVATE_KEY:
      return new ObjectPolicyPrivateKey();
    case CKO_SECRET_KEY:
      return new ObjectPolicySecretKey();
  }
  return new ObjectPolicyCommon();
}

}  // namespace chaps
