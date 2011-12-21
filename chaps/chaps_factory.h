// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_FACTORY_H
#define CHAPS_CHAPS_FACTORY_H

#include <string>

namespace chaps {

class Object;
class ObjectPool;
class Session;
class TPMUtility;

// ChapsFactory is a factory for a number of interfaces in the Chaps
// environment.  Having this factory allows object implementations to be
// decoupled and allows the creation of mock objects.
class ChapsFactory {
 public:
  virtual ~ChapsFactory() {}
  virtual Session* CreateSession(int slot_id,
                                 ObjectPool* token_object_pool,
                                 TPMUtility* tpm_utility,
                                 bool is_read_only) = 0;
  virtual ObjectPool* CreateObjectPool() = 0;
  virtual ObjectPool* CreatePersistentObjectPool(
      const std::string& file_name) = 0;
  virtual Object* CreateObject() = 0;
};

}  // namespace

#endif  // CHAPS_CHAPS_FACTORY_H
