// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_FACTORY_IMPL_H
#define CHAPS_CHAPS_FACTORY_IMPL_H

#include "chaps/chaps_factory.h"

#include <base/basictypes.h>

namespace chaps {

class ChapsFactoryImpl : public ChapsFactory {
 public:
 ChapsFactoryImpl() {}
  virtual ~ChapsFactoryImpl() {}
  virtual Session* CreateSession(int slot_id,
                                 ObjectPool* token_object_pool,
                                 TPMUtility* tpm_utility,
                                 HandleGenerator* handle_generator,
                                 bool is_read_only);
  virtual ObjectPool* CreateObjectPool(HandleGenerator* handle_generator,
                                       ObjectStore* store);
  virtual ObjectStore* CreateObjectStore(const FilePath& file_name);
  virtual Object* CreateObject();
  virtual ObjectPolicy* CreateObjectPolicy(CK_OBJECT_CLASS type);

 private:
  DISALLOW_COPY_AND_ASSIGN(ChapsFactoryImpl);
};

}  // namespace

#endif  // CHAPS_CHAPS_FACTORY_H
