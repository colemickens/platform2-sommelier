// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APMANAGER_RPC_INTERFACE_H_
#define APMANAGER_RPC_INTERFACE_H_

// TODO(zqiu): put this under a compiler flag (e.g. __DBUS__).
#include <dbus/object_path.h>

namespace apmanager {

// TODO(zqiu): put this under a compiler flag (e.g. __DBUS__).
typedef dbus::ObjectPath RPCObjectIdentifier;

}  // namespace apmanager

#endif  // APMANAGER_RPC_INTERFACE_H_
