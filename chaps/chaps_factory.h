// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_FACTORY_H_
#define CHAPS_CHAPS_FACTORY_H_

#include <string>

#include <base/files/file_path.h>

#include "pkcs11/cryptoki.h"

namespace chaps {

class HandleGenerator;
class Object;
class ObjectImporter;
class ObjectPolicy;
class ObjectPool;
class ObjectStore;
class Session;
class TPMUtility;

// ChapsFactory is a factory for a number of interfaces in the Chaps
// environment. Having this factory allows object implementations to be
// decoupled and allows the creation of mock objects.
class ChapsFactory {
 public:
  virtual ~ChapsFactory() {}
  virtual Session* CreateSession(int slot_id,
                                 ObjectPool* token_object_pool,
                                 TPMUtility* tpm_utility,
                                 HandleGenerator* handle_generator,
                                 bool is_read_only) = 0;
  virtual ObjectPool* CreateObjectPool(HandleGenerator* handle_generator,
                                       ObjectStore* store,
                                       ObjectImporter* importer) = 0;
  virtual ObjectStore* CreateObjectStore(const base::FilePath& file_name) = 0;
  virtual Object* CreateObject() = 0;
  virtual ObjectPolicy* CreateObjectPolicy(CK_OBJECT_CLASS type) = 0;
  // Returns NULL if no importer is available.
  virtual ObjectImporter* CreateObjectImporter(int slot_id,
                                               const base::FilePath& path,
                                               TPMUtility* tpm_utility) = 0;
};

}  // namespace chaps

#endif  // CHAPS_CHAPS_FACTORY_H_
