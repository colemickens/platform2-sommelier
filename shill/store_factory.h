// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_STORE_FACTORY_H_
#define SHILL_STORE_FACTORY_H_

#include <base/lazy_instance.h>

#include "shill/glib.h"

namespace shill {

class StoreInterface;

class StoreFactory {
 public:
  // This is a singleton. Use StoreFactory::GetInstance()->Foo().
  static StoreFactory* GetInstance();

  StoreInterface* CreateStore();

 protected:
  StoreFactory();

 private:
  friend struct base::DefaultLazyInstanceTraits<StoreFactory>;
  GLib glib_;

  DISALLOW_COPY_AND_ASSIGN(StoreFactory);
};

}  // namespace shill

#endif  // SHILL_STORE_FACTORY_H_
