// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/store_factory.h"

#include "shill/json_store.h"
#include "shill/key_file_store.h"

namespace shill {

namespace {

base::LazyInstance<StoreFactory> g_persistent_store_factory
    = LAZY_INSTANCE_INITIALIZER;

}  // namespace

StoreFactory::StoreFactory() {}

// static
StoreFactory* StoreFactory::GetInstance() {
  return g_persistent_store_factory.Pointer();
}

StoreInterface* StoreFactory::CreateStore(const base::FilePath& path) {
#if defined(ENABLE_JSON_STORE)
  return new JsonStore(path);
#else
  return new KeyFileStore(path);
#endif
}

}  // namespace shill
