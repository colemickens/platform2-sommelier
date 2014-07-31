// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "peerd/manager.h"

#include <dbus/exported_object.h>
#include <dbus/object_path.h>

#include "peerd/dbus_constants.h"

using peerd::dbus_constants::kManagerServicePath;

namespace peerd {

Manager::Manager(scoped_refptr<dbus::Bus> bus)
  : bus_(bus),
    exported_object_(bus->GetExportedObject(
        dbus::ObjectPath(kManagerServicePath))) {
}

Manager::~Manager() {
  exported_object_->Unregister();
  exported_object_ = nullptr;
}

}  // namespace peerd
